#!/usr/bin/env python3
"""Verifica INT-012 por USB, com injeções isoladas ou --production.

Casos 0..13 correspondem aos ambientes int012 descritos no guia. Reinicia
a placa por RTS antes do cenário. Não altera NVS ou critérios históricos.
Relatório exclusivo conserva linhas, horários do host e resultados parciais.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import time

import serial

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.protocol import encode_confirmation, parse_line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--case", type=int, choices=range(14), default=0)
    parser.add_argument("--production", action="store_true")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    assert not args.production or args.case == 0
    report = {"passed": False, "case": args.case, "production": args.production,
              "lines": [], "markers": [], "boots": [], "acks": [], "telemetry": [], "checks": []}
    stream = None
    sequence = 1000
    last_reason = None
    ready_boots = 0

    def receive():
        nonlocal last_reason, ready_boots
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return
        host_s = time.monotonic()
        report["lines"].append({"host_s": host_s, "text": line})
        assert "[WATCHDOG-CHECK] FAIL" not in line, line
        assert "Interrupt wdt timeout" not in line, line
        values = dict(re.findall(r"(\w+)=([^\s]+)", line))
        row = {key: int(value) if value.isdigit() else value for key, value in values.items()}
        row["host_s"] = host_s
        if line.startswith("[WATCHDOG] ") or line.startswith("[WATCHDOG-CHECK] "):
            report["markers"].append(row)
        if line.startswith("[WATCHDOG] ") and "armed" in row:
            last_reason = row["reset_reason"]
        if line.startswith("[WATCHDOG-CHECK] ") and "boot_id" in row:
            assert row["case"] == args.case, row
            report["boots"].append(row)
        if args.production and line.startswith("[OBC] Tempo/NVS: OK;"):
            report["boots"].append({"boot_id": row["boot_id"], "reset_reason": last_reason, "host_s": host_s})
        if line.startswith("[FLOW] Recepcao="):
            assert "Recepcao=OK" in line, line
            ready_boots += 1
        received = parse_line(line)
        if received.kind != "packet":
            return
        packet = received.packet
        assert len(line.encode()) <= 255, packet
        if packet["t"] == "ack":
            report["acks"].append(packet)
        elif packet["t"] == "tm":
            report["telemetry"].append(packet)
        elif packet["t"] == "ac":
            assert not args.production
            stream.write(encode_confirmation(tuple(packet["id"])))
            stream.flush()

    def wait_for(condition, seconds=4):
        deadline = time.monotonic() + seconds
        while not condition() and time.monotonic() < deadline:
            receive()
        assert condition(), f"condicao nao atendida em {seconds}s"

    def observe(seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            receive()

    def send(command, parameters=None, include_args=False, seq=None):
        nonlocal sequence
        if seq is None:
            sequence += 1
            seq = sequence
        frame = {"t": "tc", "seq": seq, "cmd": command}
        if include_args or parameters is not None:
            frame["args"] = parameters
        stream.write((json.dumps(frame, separators=(",", ":")) + "\n").encode())
        stream.flush()
        return seq

    def exchange(command, parameters=None, expected="ok", include_args=False):
        seq = send(command, parameters, include_args)
        wait_for(lambda: any(ack["seq"] == seq for ack in report["acks"]))
        ack = next(ack for ack in report["acks"] if ack["seq"] == seq)
        assert ack["st"] == expected, ack
        return seq

    def check_new_boot(expected_reason):
        wait_for(lambda: len(report["boots"]) >= 2 and ready_boots >= (1 if args.case == 7 else 2), 25)
        first, second = report["boots"][:2]
        assert second["boot_id"] == first["boot_id"] + 1, report["boots"]
        assert second["reset_reason"] == expected_reason, second
        if not args.production:
            assert second["resumed"] == 1 and second["previous_boot"] == first["boot_id"], second
        start_tm = len(report["telemetry"])
        exchange("request_tm")
        wait_for(lambda: len(report["telemetry"]) > start_tm)
        assert report["telemetry"][-1]["ts"] is None, report["telemetry"][-1]
        observe(6)
        assert len(report["boots"]) == 2, report["boots"]
        exchange("ping")
        report["checks"].append("novo boot confirmado, causa esperada, UTC desconhecido e retorno estavel sem SD")

    with args.report.open("x", encoding="utf-8") as output:
        try:
            stream = serial.Serial()
            stream.port, stream.baudrate, stream.timeout = args.port, 115200, 0.1
            stream.dtr, stream.rts = False, False
            stream.open()
            stream.rts = True
            time.sleep(0.1)
            stream.reset_input_buffer()
            stream.rts = False
            wait_for(lambda: bool(report["boots"]), 8)
            if args.case == 7:
                check_new_boot(6)  # ESP_RST_TASK_WDT no SDK fixado.
                assert any(row.get("pause") == "startup" for row in report["markers"])
            else:
                wait_for(lambda: ready_boots >= 1, 8)
                armed = next(row for row in report["markers"] if "armed" in row)
                assert armed["armed"] == (0 if args.case == 11 else 1), armed
                exchange("ping")
                if 1 <= args.case <= 6:
                    send("set_time", {"unix": 1700000000})
                    check_new_boot(6)
                    resource = {1: "uart", 2: "process", 3: "writer", 4: "i2c", 5: "ttc", 6: "supervisor"}[args.case]
                    assert any(row.get("pause") == resource for row in report["markers"])
                    assert not any("pause_return" in row for row in report["markers"])
                    if args.case <= 5:
                        assert any(row.get("resource") == resource and row.get("blocked") == 1
                                   for row in report["markers"])
                    report["checks"].append("travamento finito acionou watchdog de tarefas antes de retornar")
                elif args.case == 8:
                    exchange("set_time", {"unix": 1700000000})
                    observe(16)
                    transitions = [row["blocked"] for row in report["markers"] if row.get("resource") == "uart"]
                    assert transitions == [1, 0], transitions
                    kinds = {(row.get("record_code"), row.get("kind")) for row in report["markers"]}
                    assert {(265, "fault"), (265, "event")} <= kinds, kinds
                    assert len(report["boots"]) == 1
                    exchange("ping")
                    report["checks"].append("progresso retomado reabilitou alimentacao; eventos reais 265; sem reset")
                elif args.case in (12, 13):
                    exchange("reset_obc", expected="state_rejected")
                    observe(6)
                    assert not any(row.get("reset_scheduled") for row in report["markers"])
                    assert len(report["boots"]) == 1
                    exchange("ping")
                    if args.case == 12:
                        assert any(row.get("reset_timer_ready") == 0 for row in report["markers"])
                    else:
                        assert any("reset_schedule_error" in row and row["pending"] == 0 for row in report["markers"])
                    report["checks"].append("falha de timer/agendamento rejeita comando e conserva atendimento")
                else:
                    # Semântica nova recebe vetores próprios; roteiros históricos ficam intactos.
                    for invalid in ([1], {"unexpected": 1}, "", 42):
                        exchange("reset_obc", invalid, "invalid_param", True)
                    exchange("set_time", {"unix": 1700000000})
                    if args.case in (9, 10):
                        send("reset_obc")
                    else:
                        seq = exchange("reset_obc", {}, include_args=True)
                        observe(0.35)
                        exchange("reset_obc", None, include_args=True)
                        send("reset_obc", {}, include_args=True, seq=seq)
                    check_new_boot(3)  # ESP_RST_SW.
                    scheduled = [row for row in report["markers"] if row.get("reset_scheduled") == 1]
                    assert len(scheduled) == 1, scheduled
                    returned = report["boots"][1]
                    report["schedule_to_boot_ms"] = (returned["host_s"] - scheduled[0]["host_s"]) * 1000
                    assert 1400 <= report["schedule_to_boot_ms"] <= 3500, report["schedule_to_boot_ms"]
                    if not args.production:
                        elapsed = (returned["timer_fired_ms"] - scheduled[0]["monotonic_ms"]) & 0xFFFFFFFF
                        report["timer_elapsed_ms"] = elapsed
                        assert abs(elapsed - 1500) <= 100, elapsed
                    if args.case == 9:
                        assert any(row.get("ack_held") == 1 and row.get("ack_filled") == 4 for row in report["markers"])
                        assert any("falha ao enfileirar ACK" in row["text"] for row in report["lines"])
                    if args.case == 10:
                        assert any(row.get("pause") == "reset_consumer" for row in report["markers"])
                        assert not any(row.get("pause_return") == "reset_consumer" for row in report["markers"])
                    report["checks"].append("reset valido em 1500ms nominais, argumentos rejeitados, sem depender de ACK/loop/SD")
            assert len(report["boots"]) == (1 if args.case in (8, 12, 13) else 2)
            report["passed"] = True
        except Exception as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            if stream is not None:
                stream.close()
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(f"ObcWatchdogBenchCheck passed; {args.report}")


if __name__ == "__main__":
    main()
