#!/usr/bin/env python3
"""Verifica recursos por USB; --bench usa int008-resources-check.

Sem --bench, usa obc sem sensores/cartão/Pi/rádio. Pausa deliberada TT&C
tem cenário próprio; limites normais de TC são verificados antes/depois.
Não comprova rádio, SD físico, watchdog ou retomada operacional.
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
    report = {"passed": False, "bench": args.bench, "lines": [], "ttc": [], "memory": [],
              "markers": [], "exchanges": [], "checks": []}
    sequence = int(time.time() * 1000) & 0x7FFF
    stream = None
    pending, acknowledgments = {}, {}
    last_counter = None
    boots = set()

    def receive():
        nonlocal last_counter
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return
        report["lines"].append(line)
        assert "FAIL" not in line and "Guru Meditation" not in line, line
        values = dict(re.findall(r"(\w+)=([^\s]+)", line))
        row = {key: int(value) if value.isdigit() else value for key, value in values.items()}
        if line.startswith("[TTC-HEALTH] "):
            report["ttc"].append(row)
        elif line.startswith("[MEMORY-HEALTH] "):
            report["memory"].append(row)
        elif line.startswith("[RESOURCE-CHECK] "):
            report["markers"].append(row)
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
            assert seq in pending and seq not in acknowledgments, packet
            elapsed = (time.monotonic() - pending[seq]) * 1000
            assert packet["st"] == "ok" and packet["q"] <= 200 and elapsed <= 2000, (packet, elapsed)
            acknowledgments[seq] = packet
            report["exchanges"].append({"ack": packet, "response_ms": round(elapsed, 3)})
        elif packet["t"] == "ac":
            assert args.bench, packet
            stream.write(encode_confirmation(tuple(packet["id"])))
            stream.flush()

    def exchange(command, parameters=None):
        nonlocal sequence
        sequence = (sequence + 1) & 0x7FFF
        pending[sequence] = time.monotonic()
        stream.write(encode_tc(sequence, command, parameters))
        stream.flush()
        until = pending[sequence] + 2
        while sequence not in acknowledgments and time.monotonic() < until:
            receive()
        assert sequence in acknowledgments, ("ACK ausente", sequence, command)

    with args.report.open("x", encoding="utf-8") as output:
        try:
            stream = open_serial(args.port)
            for _ in range(3):
                exchange("ping")
                exchange("request_tm")
            if args.bench:
                exchange("set_time", {"unix": 1700000000})
                until = time.monotonic() + 40
                while time.monotonic() < until and not any(row.get("done") == 1 for row in report["markers"]):
                    receive()
                assert any(row.get("done") == 1 for row in report["markers"]), "bancada nao concluiu"
            # Drena registros e observa estabilidade depois das injeções/ociosidade.
            until = time.monotonic() + 5
            while time.monotonic() < until:
                receive()
            for _ in range(3):
                exchange("ping")
                exchange("request_tm")
            assert len(acknowledgments) == len(pending)
            ttc = report["ttc"]
            memory = [row for row in report["memory"] if "byte_heap" in row]
            stacks = [row for row in report["memory"] if "task" in row]
            assert ttc and memory and {row["task"] for row in stacks} == {"uart", "process", "writer", "i2c", "ttc", "loop"}
            if args.bench:
                markers = report["markers"]
                suspects = [row for row in ttc if row["state"] == "suspect" and row["changed"]]
                assert len(suspects) == 1 and suspects[0]["age_ms"] > 2000, suspects
                pause = next(row for row in markers if row.get("pause_end") == 1)
                assert pause["elapsed_ms"] >= 2300
                assert ttc[-1]["state"] == ttc[-1]["radio"] == "ok"
                assert any(row["radio"] == "unavailable" and row["state"] == "ok" for row in ttc)
                assert any(row["rf_seen"] and row["wired_seen"] and row["rf_age_ms"] > 2000 for row in ttc)
                low = next(row for row in memory if row["byte_heap"] == 8191)
                band = next(row for row in memory if row["byte_heap"] == 10000)
                restored = next(row for row in memory if row["byte_heap"] == 12288)
                assert low["heap_low"] == low["block_low"] == low["changed"] == 1
                assert band["heap_low"] == band["block_low"] == 1 and band["changed"] == 0
                assert restored["heap_low"] == restored["block_low"] == 0 and restored["changed"] == 1
                assert any(row["task"] == "writer" and row["stack_free"] == 1023 and row["low"] for row in stacks)
                assert any(row["task"] == "writer" and row["stack_free"] == 4096 and row["low"] for row in stacks)
                assert memory[-1]["stack_low_mask"] == 4 and memory[-1]["allocation_failures"] == 1
                assert sum(row["allocation_delta"] for row in memory) == 1
                kinds = {(row["record_code"], row["kind"]) for row in markers if "record_code" in row}
                expected = {(260, "fault"), (260, "event"), (261, "fault"), (261, "event"),
                            (262, "fault"), (262, "event"), (263, "fault"), (264, "fault")}
                assert expected <= kinds, (expected - kinds, kinds)
                assert len(boots) == 1, boots
                report["checks"] = ["Ttc real: pausa finita e retorno, radio independente de progresso",
                                    "contato por meio e silencio sem inferir perda",
                                    "histerese de heap/bloco, alerta historico de pilha e recusa entre amostras",
                                    "EVENT/FAULT 260..264 confirmados pelo backend simulado, sem reboot",
                                    "TC normal antes/depois: q<=200 ms, ACK<=2 s, quadros<=255 bytes"]
            else:
                assert all(row["state"] == "ok" and row["radio"] == "unavailable" for row in ttc), ttc
                assert all(not row["heap_low"] and not row["block_low"] and not row["stack_low_mask"]
                           and not row["allocation_failures"] for row in memory), memory
                assert all(not row["active"] for row in stacks if row["task"] == "writer")
                report["checks"] = ["obc sem perifericos: radio indisponivel, USB e progresso ativos",
                                    "heap/pilhas sem falso alerta; escritor inativo nao vira margem zero",
                                    "TC q<=200 ms e ACK<=2 s, sem injeção ou reboot"]
            report["passed"] = True
        except Exception as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            if stream is not None:
                stream.close()
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(f"ObcResourcesBenchCheck passed; {args.report}")


if __name__ == "__main__":
    main()
