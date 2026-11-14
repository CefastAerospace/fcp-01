#!/usr/bin/env python3
"""Verifica saúde OBC por USB; --bench injeta pausas em int008-health-check.

Sem --bench, usa obc sem cartão/Pi e exige escritor inativo, sem falso alerta.
Não valida falhas físicas, watchdog ou recuperação automática de tarefas.
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
    report = {"passed": False, "bench": args.bench, "lines": [], "health": [], "exchanges": [], "checks": []}
    stream = None
    sequence = int(time.time() * 1000) & 0xFFFF
    started, acknowledgments, deliveries = {}, {}, {}
    markers = []
    last_counter = None
    done_at = None
    boots = set()

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
        if line.startswith("[HEALTH] "):
            row = {key: int(value) if value.isdigit() else value for key, value in values.items()}
            report["health"].append(row)
        if line.startswith("[HEALTH-CHECK] "):
            markers.append(values)
            if values.get("done") == "1":
                done_at = time.monotonic()
        if line.startswith("[FLOW-CHECK] Arquivo simulado:"):
            boots.add(line.rsplit(" ", 1)[1].split("_")[1])
        received = parse_line(line)
        if received.kind != "packet":
            return
        packet = received.packet
        assert len(line.encode()) <= 255, packet
        counter = packet["n"]
        if last_counter is not None:
            assert counter == (last_counter + 1) % 65536, packet
        last_counter = counter
        if packet["t"] == "ack":
            seq = packet["seq"]
            assert seq in started and seq not in acknowledgments, packet
            elapsed_ms = (time.monotonic() - started[seq]) * 1000
            assert packet["st"] == "ok" and packet["q"] <= 200 and elapsed_ms <= 2000, (packet, elapsed_ms)
            acknowledgments[seq] = packet
            report["exchanges"].append({"ack": packet, "response_ms": round(elapsed_ms, 3)})
        elif packet["t"] == "ac":
            assert args.bench, packet
            key = tuple(packet["id"])
            content = {name: value for name, value in packet.items() if name != "n"}
            assert deliveries.setdefault(key, content) == content
            stream.write(encode_confirmation(key))
            stream.flush()

    with args.report.open("x", encoding="utf-8") as output:
        try:
            stream = open_serial(args.port)
            if args.bench:
                send("set_time", {"unix": 1700000000})
            begin = time.monotonic()
            deadline = begin + (100 if args.bench else 4)
            next_command = 0
            while time.monotonic() < deadline:
                now = time.monotonic()
                if now >= next_command:
                    send("ping")
                    send("request_tm")
                    next_command = now + 0.5
                receive()
                overdue = [seq for seq, at in started.items() if seq not in acknowledgments and time.monotonic() - at > 2]
                assert not overdue, overdue
                if args.bench and done_at is not None and time.monotonic() - done_at >= 2:
                    break
            until = time.monotonic() + 2
            while len(acknowledgments) < len(started) and time.monotonic() < until:
                receive()
            assert len(acknowledgments) == len(started)
            health = report["health"]
            assert {row["task"] for row in health} == {"uart", "process", "writer"}, health
            if args.bench:
                assert done_at is not None, "bancada nao concluiu"
                limits = {"uart": 100, "process": 2000, "writer": 5000}
                activities = {"uart": "transport", "process": "processing", "writer": "write"}
                for task, limit in limits.items():
                    transitions = [row for row in health if row["task"] == task and row["changed"]]
                    assert [row["state"] for row in transitions] == ["suspect", "ok"], transitions
                    assert transitions[0]["limit_ms"] == limit and transitions[0]["age_ms"] > limit
                    assert transitions[0]["activity"] == activities[task], transitions
                    assert transitions[1]["completions"] > transitions[0]["completions"], transitions
                scan_start = next(int(row["monotonic_ms"]) for row in markers if "scan_begin" in row)
                scan = next(row for row in markers if "scan_end" in row)
                scan_end = int(scan["monotonic_ms"])
                assert int(scan["elapsed_ms"]) >= 6000 and int(scan["operations"]) == 32, scan
                recovery_start = next(int(row["monotonic_ms"]) for row in markers if "recovery_begin" in row)
                recovery_end = next(int(row["monotonic_ms"]) for row in markers if "recovery_end" in row)
                assert recovery_end - recovery_start >= 5000
                for row in health:
                    if scan_start <= row["sampled_ms"] <= scan_end or recovery_start <= row["sampled_ms"] <= recovery_end:
                        assert row["state"] != "suspect", row
                assert len(deliveries) == 7 and len(boots) == 1, (deliveries, boots)
                report["checks"] = [
                    "UART 300ms, processamento 2.5s e escrita 5.5s: suspeita e retorno uma vez por tarefa",
                    "varredura >6s com 32 operacoes progredindo, sem falso alerta",
                    "espera real de recuperacao >=5s sem falso alerta ou reboot",
                    "7 entregas; ping/TM ativos; q<=200ms, ACK<=2s, quadros<=255 bytes e n consecutivo",
                ]
            else:
                assert not any(row["state"] == "suspect" for row in health), health
                assert all(row["state"] == "inactive" for row in health if row["task"] == "writer")
                assert all(row["state"] == "ok" for row in health if row["task"] in ("uart", "process"))
                report["checks"] = ["sem SD/Pi: escritor inativo, UART/processamento progredindo e nenhum falso alerta"]
            report["markers"] = markers
            report["passed"] = True
        except Exception as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            if stream is not None:
                stream.close()
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(f"ObcTaskHealthBenchCheck passed; {args.report}")


if __name__ == "__main__":
    main()
