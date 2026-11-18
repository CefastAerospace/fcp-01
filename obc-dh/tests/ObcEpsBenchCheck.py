#!/usr/bin/env python3
"""Verifica int009-ina219-check durante 30 s, somente ESP32/USB, sem sensores.

Conserva relatório exclusivo, inclusive em falha. Não comprova sensores reais,
não altera critérios anteriores e não grava/restaura firmware automaticamente.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial
from fcp_gs.protocol import encode_tc, parse_line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    report = {"passed": False, "duration_s": 30, "lines": [], "channels": {},
              "health": [], "exchanges": [], "telemetry": []}
    expected_addresses = {"SOLAR": "0x40", "BATTERY": "0x41", "GENERAL": "0x44"}
    measurements = ("vin_minus_V", "vin_plus_V", "shunt_mV", "current_A",
                    "power_minus_W", "power_plus_W")
    attempts = {channel: [] for channel in expected_addresses}
    sent, acknowledgments = {}, {}
    stream = None
    sequence = int(time.time() * 1000) & 0xffff
    last_counter = None
    observing = False
    maximum_progress_ms = None

    def receive():
        nonlocal last_counter, maximum_progress_ms
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return
        host_s = time.monotonic()
        report["lines"].append({"host_s": host_s, "text": line})
        assert not any(error in line for error in
                       ("Guru Meditation", "Task watchdog got triggered", "Interrupt wdt timeout")), line
        if observing:
            assert not line.startswith("[OBC] Boot de bancada:"), line
        values = dict(re.findall(r"(\w+)=([^\s]+)", line))
        if line.startswith("[EPS] channel="):
            channel = values["channel"]
            assert channel in expected_addresses, values
            assert values["address"] == expected_addresses[channel], values
            assert values["available"] == "0" and values["has_valid"] == "0", values
            assert all(values[key] == "null" for key in measurements), values
            attempted_ms = int(values["attempted_ms"])
            previous = attempts[channel]
            if not previous or attempted_ms != previous[-1]:
                if previous:
                    assert attempted_ms - previous[-1] >= 5000, (channel, previous, attempted_ms)
                previous.append(attempted_ms)
            report["channels"][channel] = values
        elif line.startswith("[EPS-HEALTH] "):
            assert values["active"] == "1" and values["service"] == "1", values
            progress_ms = int(values["progress_ms"])
            if maximum_progress_ms is not None:
                assert progress_ms > maximum_progress_ms, values
            maximum_progress_ms = progress_ms
            assert int(values["stack_free_bytes"]) > 0, values
            report["health"].append(values)
        elif line.startswith("[WATCHDOG] resource="):
            assert values["blocked"] == "0", values

        received = parse_line(line)
        if received.kind != "packet":
            return
        packet = received.packet
        assert len(line.encode()) <= 255, packet
        counter = packet["n"]
        assert type(counter) is int and 0 <= counter <= 65535, packet
        if last_counter is not None:
            assert counter == (last_counter + 1) % 65536, packet
        last_counter = counter
        if packet["t"] == "ack":
            assert packet["seq"] in sent, packet
            elapsed = host_s - sent[packet["seq"]]
            assert packet["q"] <= 200 and elapsed <= 2, (packet, elapsed)
            assert packet["st"] == "ok", packet
            acknowledgments[packet["seq"]] = packet
            report["exchanges"].append({"ack": packet, "response_ms": elapsed * 1000})
        elif packet["t"] == "tm":
            assert packet["md"] == "bench", packet
            assert all(packet[key] is None for key in ("vb", "ib", "vs", "is")), packet
            report["telemetry"].append(packet)

    def exchange(command):
        nonlocal sequence
        sequence = (sequence + 1) % 65536
        telemetry_before = len(report["telemetry"])
        sent[sequence] = time.monotonic()
        stream.write(encode_tc(sequence, command))
        stream.flush()
        deadline = sent[sequence] + 2
        while time.monotonic() < deadline:
            complete = sequence in acknowledgments and (
                command != "request_tm" or len(report["telemetry"]) > telemetry_before)
            if complete:
                return
            receive()
        raise AssertionError(f"Resposta ausente em 2 s: {command}, seq={sequence}")

    # Criação exclusiva conserva relatórios anteriores e não abre a porta se houver colisão.
    with args.report.open("x", encoding="utf-8") as output:
        try:
            stream = open_serial(args.port)
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline and (
                    len(report["channels"]) != 3 or not report["health"]):
                receive()
            assert len(report["channels"]) == 3 and report["health"], "Diagnostico EPS ausente"
            observing = True
            begin = time.monotonic()
            while time.monotonic() - begin < 30:
                exchange("ping")
                exchange("request_tm")
                until = min(begin + 30, time.monotonic() + 1)
                while time.monotonic() < until:
                    receive()
            assert all(len(values) >= 3 for values in attempts.values()), attempts
            assert len(report["health"]) >= 3 and report["telemetry"], report["health"]
            report["attempted_ms"] = attempts
            report["observed_s"] = time.monotonic() - begin
            report["passed"] = True
        except BaseException as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            if stream is not None:
                stream.close()
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print("INA219 USB: indisponibilidade, cadencia, progresso e TC/TM aprovados")


if __name__ == "__main__":
    main()
