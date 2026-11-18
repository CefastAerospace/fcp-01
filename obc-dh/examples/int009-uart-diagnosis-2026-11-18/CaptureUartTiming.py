#!/usr/bin/env python3
"""Capture UART timing without sending commands; this is diagnostic evidence only."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--seconds", type=float, default=90)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error("--seconds must be between 1 and 300")
    result = {"diagnostic_only": True, "functional_acceptance": "not_run",
              "port": args.port, "commands_sent": 0, "lines": [], "trace": [],
              "started_utc": datetime.now(timezone.utc).isoformat()}
    with args.report.open("x", encoding="utf-8") as output:
        try:
            with open_serial(args.port, settle_s=0.5, quiet_s=0.1, max_wait_s=0.8) as stream:
                started = time.monotonic()
                while time.monotonic() - started < args.seconds:
                    line = stream.readline().decode(errors="replace").strip()
                    if not line:
                        continue
                    result["lines"].append(line)
                    if line.startswith("[HK-UART]"):
                        result["trace"].append(dict(re.findall(r"(\w+)=([^\s]+)", line)))
                result["observed_s"] = time.monotonic() - started
            result["diagnosis_captured"] = bool(result["trace"])
        finally:
            result["finished_utc"] = datetime.now(timezone.utc).isoformat()
            json.dump(result, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(json.dumps({"diagnosis_captured": result["diagnosis_captured"],
                      "trace": result["trace"], "report": str(args.report)}))


if __name__ == "__main__":
    main()
