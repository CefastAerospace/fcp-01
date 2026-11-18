#!/usr/bin/env python3
"""Verifica diagnóstico I2C por USB; --bench usa int008-i2c-health-check.

Sem --bench, exige obc sem sensores, com serviço ocioso e nenhum falso alerta.
Não comprova barramento físico, watchdog ou políticas de recuperação automática.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial
from fcp_gs.protocol import encode_confirmation, encode_tc, parse_line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--bench", action="store_true")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    report = {"passed": False, "bench": args.bench, "lines": [], "health": [], "markers": [], "exchanges": []}
    sequence = int(time.time() * 1000) & 0xFFFF
    stream = None
    started, acknowledgments = {}, {}
    boots = set()
    last_counter = None
    done_at = None

    def send(command, parameters=None):
        nonlocal sequence
        sequence = (sequence + 1) & 0xFFFF
        started[sequence] = time.monotonic()
        stream.write(encode_tc(sequence, command, parameters))
        stream.flush()

    def receive():
        nonlocal last_counter, done_at
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return
        report["lines"].append(line)
        assert "FAIL" not in line and "Guru Meditation" not in line, line
        values = dict(re.findall(r"(\w+)=([^\s]+)", line))
        row = {key: int(value) if value.isdigit() else value for key, value in values.items()}
        if line.startswith("[I2C-HEALTH] "):
            report["health"].append(row)
        if line.startswith("[I2C-HEALTH-CHECK] "):
            report["markers"].append(row)
            if row.get("done") == 1:
                done_at = time.monotonic()
        if line.startswith("[FLOW-CHECK] Arquivo simulado:"):
            boots.add(line.rsplit(" ", 1)[1].split("_")[1])
        received = parse_line(line)
        if received.kind != "packet":
            return
        packet = received.packet
        assert len(line.encode()) <= 255, packet
        if last_counter is not None:
            assert packet["n"] == (last_counter + 1) % 65536, packet
        last_counter = packet["n"]
        if packet["t"] == "ack":
            seq = packet["seq"]
            assert seq in started and seq not in acknowledgments, packet
            elapsed_ms = (time.monotonic() - started[seq]) * 1000
            assert packet["st"] == "ok" and packet["q"] <= 200 and elapsed_ms <= 2000, (packet, elapsed_ms)
            acknowledgments[seq] = packet
            report["exchanges"].append({"ack": packet, "response_ms": round(elapsed_ms, 3)})
        elif packet["t"] == "ac":
            assert args.bench, packet
            stream.write(encode_confirmation(tuple(packet["id"])))
            stream.flush()

    with args.report.open("x", encoding="utf-8") as output:
        try:
            stream = open_serial(args.port)
            if args.bench:
                send("set_time", {"unix": 1700000000})
            deadline = time.monotonic() + (20 if args.bench else 4)
            next_command = 0
            while time.monotonic() < deadline:
                now = time.monotonic()
                if now >= next_command:
                    send("ping")
                    send("request_tm")
                    next_command = now + 0.5
                receive()
                assert not [seq for seq, at in started.items() if seq not in acknowledgments and time.monotonic() - at > 2]
                if done_at is not None and time.monotonic() - done_at >= 2:
                    break
            until = time.monotonic() + 2
            while len(acknowledgments) < len(started) and time.monotonic() < until:
                receive()
            assert len(acknowledgments) == len(started) and acknowledgments
            health, markers = report["health"], report["markers"]
            assert health, "sem diagnostico I2C"
            if args.bench:
                assert done_at is not None and len(boots) == 1, boots
                callback = [row for row in health if row["changed"] and row["state"] == "suspect" and row["activity"] == "callback"]
                recovery = [row for row in health if row["changed"] and row["state"] == "suspect" and row["activity"] == "recovery"]
                assert len(callback) == len(recovery) == 1, (callback, recovery)
                assert callback[0]["age_ms"] > callback[0]["limit_ms"] == 100
                assert recovery[0]["age_ms"] > recovery[0]["limit_ms"] == 200
                cases = {row["case"]: row["monotonic_ms"] for row in markers if "case" in row}
                allowed = [(cases["callback_begin"], cases["callback_end"]),
                           (cases["slow_recovery_begin"], cases["slow_recovery_end"])]
                for row in health:
                    if row["state"] == "suspect":
                        assert any(begin <= row["monotonic_ms"] <= end for begin, end in allowed), row
                assert any(row["changed"] and row["state"] == "ok" and cases["callback_end"] <= row["monotonic_ms"] < cases["normal_recovery_begin"] for row in health)
                assert any(row["changed"] and row["state"] == "unavailable" for row in health)
                assert health[-1]["state"] == "ok" and health[-1]["service"] == "ready"
                context = next(row for row in markers if "context_hold_ms" in row)
                assert context["context_hold_ms"] >= 350 > context["queue_wait_ms"] == 100 and context["calls"] == 1
                done = next(row for row in markers if row.get("done") == 1)
                assert (done["cycles"], done["recovered"], done["exhausted"]) == (4, 2, 2) and done["stack"] > 0
                kinds = {row["kind"] for row in markers if row.get("record_code") == 259}
                assert {"fault", "event"} <= kinds, kinds
                report["checks"] = ["callback 350 ms e recuperacao 450 ms detectados uma vez cada",
                                    "ociosidade, erros retornados e recuperacao normal sem falso alerta",
                                    "contexto preservado, esgotamento sem repeticao automatica, retomada explicita",
                                    "EVENT/FAULT 259 no backend simulado; USB q<=200 ms e ACK<=2 s"]
            else:
                assert all(row["state"] == "ok" and row["service"] == "ready" and row["activity"] == "idle" for row in health), health
                assert not any(row["changed"] for row in health), health
                report["checks"] = ["sem sensores: I2C ocioso, sem falso alerta ou recuperacao automatica"]
            report["passed"] = True
        except Exception as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            if stream is not None:
                stream.close()
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(f"ObcI2cHealthBenchCheck passed; {args.report}")


if __name__ == "__main__":
    main()
