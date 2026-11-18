#!/usr/bin/env python3
"""Verify FCP4 in the simulated journal on an ESP32 without sensors or commands."""

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--reader", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    report = {"passed": False, "port": args.port, "commands_sent": 0, "lines": []}
    prefix = "[HK-ACQUISITION] hex="
    expected = {
        ("eps_acquisition", channel, address)
        for channel, address in enumerate((0x40, 0x41, 0x44))
    } | {("thermal_acquisition", channel, 0x48 + channel) for channel in range(5)}
    with args.report.open("x", encoding="utf-8") as output:
        try:
            binary = args.report.with_suffix(".bin")
            with binary.open("xb") as capture, open_serial(args.port) as stream:
                end = time.monotonic() + 22
                while time.monotonic() < end:
                    line = stream.readline().decode("ascii", errors="replace").strip()
                    if not line:
                        continue
                    report["lines"].append(line)
                    assert "Guru Meditation" not in line and "Backtrace:" not in line, line
                    if line.startswith(prefix):
                        capture.write(bytes.fromhex(line[len(prefix):]))
            result = subprocess.run([args.reader.resolve(), binary], check=True, capture_output=True)
            records = [json.loads(line) for line in result.stdout.splitlines()]
            report["records"] = records
            observed = set()
            attempts = set()
            for record in records:
                data = record["data"]
                channel = (record["type"], data["channel"], data["address"])
                assert channel in expected, channel
                observed.add(channel)
                attempt = (record["boot_id"], channel, data["started_ms"])
                assert attempt not in attempts, attempt
                attempts.add(attempt)
                assert record["boot_id"] > 0 and data["started_ms"] <= record["monotonic_ms"]
                assert data["operation"] == 1 and data["result"] != 0
                for field in ("vin_minus_V", "vin_plus_V", "shunt_mV", "current_A",
                              "power_minus_W", "power_plus_W", "temperature_C"):
                    assert data.get(field) is None
            assert observed == expected, sorted(expected - observed)
            poll = [int(match.group(1)) for line in report["lines"]
                    if (match := re.search(r"\bpoll_gap_ms=(\d+)", line))]
            assert poll and max(poll) <= 10, poll
            report["maximum_poll_gap_ms"] = max(poll)
            report["passed"] = True
        except BaseException as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(f"FCP4 USB: {len(records)} registros dos oito canais sem comandos")


if __name__ == "__main__":
    main()
