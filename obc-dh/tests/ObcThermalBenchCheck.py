#!/usr/bin/env python3
"""Verifica int009-lm75a-check por 30 s, somente ESP32/USB e sem sensores.

Preserva relatório exclusivo mesmo em falha. Não valida sensores reais,
precisão térmica, proteção, cartão ou RF. Não altera/grava firmware.
"""
import argparse
from datetime import datetime, timezone
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
    report = {"passed": False, "duration_s": 30, "port": args.port,
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "lines": [], "channels": {}, "health": [], "exchanges": [],
              "telemetry": [], "boot_ids": []}
    expected_addresses = {f"thermal_{index + 1}": f"0x{0x48 + index:02x}" for index in range(5)}
    attempts = {channel: [] for channel in expected_addresses}
    sent, acknowledgments = {}, {}
    stream = None
    sequence = int(time.time() * 1000) & 0xffff
    last_counter = None
    last_progress = None
    observing = False

    def receive():
        nonlocal last_counter, last_progress
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return
        report["lines"].append(line)
        assert "Guru Meditation" not in line and "Backtrace:" not in line, line
        if line.startswith("[OBC] Tempo/NVS: OK;"):
            report["boot_ids"].append(int(re.search(r"boot_id=(\d+)", line).group(1)))
            assert not observing, "Novo boot durante a observacao"
        row = dict(re.findall(r"(\w+)=([^\s]+)", line))
        if line.startswith("[THERMAL] channel="):
            channel = row["channel"]
            assert channel in expected_addresses and row["address"] == expected_addresses[channel], row
            assert row["available"] == "0" and row["has_valid"] == "0", row
            assert row["temperature_C"] == "null" and row["last_valid_C"] == "null", row
            assert row["attempted"] == "1", row
            attempted_ms = int(row["attempted_ms"])
            values = attempts[channel]
            if not values or values[-1] != attempted_ms:
                if values:
                    assert attempted_ms - values[-1] >= 5000, (channel, values, attempted_ms)
                values.append(attempted_ms)
            report["channels"][channel] = row
        elif line.startswith("[THERMAL-HEALTH]"):
            assert row["active"] == "1" and row["service"] == "1", row
            progress = int(row["progress_ms"])
            assert last_progress is None or progress > last_progress, row
            last_progress = progress
            assert 0 <= int(row["age_ms"]) <= 1000, row
            assert int(row["stack_free_bytes"]) > 0, row
            assert int(row["operations"]) >= 5, row
            assert int(row["operations"]) == int(row["failures"]), row
            report["health"].append(row)

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
            seq = packet["seq"]
            assert seq in sent and packet["cmd"] == sent[seq]["cmd"], packet
            assert packet["st"] == "ok" and type(packet["q"]) is int and 0 <= packet["q"] <= 200, packet
            elapsed_ms = (time.monotonic() - sent[seq]["started"]) * 1000
            assert elapsed_ms <= 2000, (packet, elapsed_ms)
            acknowledgments[seq] = packet
            report["exchanges"].append({"ack": packet, "response_ms": elapsed_ms})
        elif packet["t"] == "tm":
            assert packet["md"] == "bench", packet
            for key in ("ts", "vb", "ib", "vs", "is", "tp", "w", "az", "ac", "msg", "rssi", "snr"):
                assert packet[key] is None, (key, packet)
            report["telemetry"].append(packet)
        else:
            raise AssertionError(f"Pacote inesperado: {packet}")

    def exchange(command):
        nonlocal sequence
        sequence = (sequence + 1) & 0xffff
        seq = sequence
        before = len(report["telemetry"])
        sent[seq] = {"cmd": command, "started": time.monotonic()}
        stream.write(encode_tc(seq, command))
        stream.flush()
        deadline = sent[seq]["started"] + 2
        while time.monotonic() < deadline:
            if seq in acknowledgments and (command != "request_tm" or len(report["telemetry"]) > before):
                return
            receive()
        raise AssertionError(f"Sem ACK/TM em 2 s: {command}, seq={seq}")

    # Não abre a porta nem sobrescreve evidência se o destino já existe.
    with args.report.open("x", encoding="utf-8") as output:
        try:
            stream = open_serial(args.port)
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline and (
                    len(report["channels"]) != 5 or not report["health"]):
                receive()
            assert len(report["channels"]) == 5 and report["health"], "Diagnostico termico ausente"
            assert len(report["boot_ids"]) == 1, report["boot_ids"]
            observing = True
            begin = time.monotonic()
            while time.monotonic() - begin < 30:
                exchange("ping")
                exchange("request_tm")
                until = min(begin + 30, time.monotonic() + 1)
                while time.monotonic() < until:
                    receive()
            assert all(len(values) >= 5 for values in attempts.values()), attempts
            assert len(report["health"]) >= 5 and len(report["telemetry"]) >= 5, report
            report["attempted_ms"] = attempts
            report["observed_s"] = time.monotonic() - begin
            report["minimum_stack_free_bytes"] = min(int(row["stack_free_bytes"]) for row in report["health"])
            report["passed"] = True
        except BaseException as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            if stream is not None:
                stream.close()
            report["finished_utc"] = datetime.now(timezone.utc).isoformat()
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(f"LM75A USB: indisponibilidade, cadencia, progresso e TC/TM aprovados; {args.report}")


if __name__ == "__main__":
    main()
