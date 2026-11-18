#!/usr/bin/env python3
"""Captura o boot instrumentado por USB sem enviar telecomandos ou aprovar a integração."""
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
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    report = {"diagnosis_captured": False, "functional_acceptance": "not_run",
              "inputs_sent": [], "port": args.port, "lines": [], "allocations": [],
              "started_utc": datetime.now(timezone.utc).isoformat()}
    stream = None
    with args.report.open("x", encoding="utf-8") as output:
        try:
            stream = open_serial(args.port, settle_s=0.5, quiet_s=0.1, max_wait_s=0.8)
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                line = stream.readline().decode(errors="replace").strip()
                if not line:
                    continue
                report["lines"].append(line)
                row = dict(re.findall(r"(\w+)=([^\s]+)", line))
                if line.startswith("[HK-CHECK]") and "ready" in row:
                    report["bench"] = row
                elif line.startswith("[HK-ALLOC]"):
                    report["allocations"].append(row)
                elif line.startswith("[FLOW] Recepcao="):
                    report["flow_result"] = line
                    break
            pairs = {(row["task"], row["phase"]) for row in report["allocations"]}
            expected = {(task, phase) for task in ("uart", "process", "writer") for phase in ("before", "after")}
            if pairs != expected or report.get("bench", {}).get("hook_status") != "0":
                raise RuntimeError("Captura incompleta ou callback de diagnostico indisponivel")
            report["diagnosis_captured"] = True
        except BaseException as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            if stream is not None:
                stream.close()
            report["finished_utc"] = datetime.now(timezone.utc).isoformat()
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(json.dumps({"diagnosis_captured": report["diagnosis_captured"],
                      "allocations": report["allocations"], "flow_result": report.get("flow_result")},
                     ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
