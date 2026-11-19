#!/usr/bin/env python3
"""Executa um cenário USB da TES-006 e guarda o log bruto."""
import argparse
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("scenario", choices=("nominal", "absent", "full", "recovered"))
    parser.add_argument("run_id", help="quatro dígitos hexadecimais minúsculos")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]{4}", args.run_id):
        parser.error("identificador inválido")
    if args.report.exists():
        parser.error("escolha um caminho de relatório novo")

    codes = {"nominal": "N", "absent": "A", "full": "F", "recovered": "R"}
    case = codes[args.scenario]
    done = f"[TES006] done case={case} id={args.run_id} passed="
    report = {"port": args.port, "scenario": args.scenario, "run_id": args.run_id, "started": time.time(),
              "passed": False, "lines": []}

    try:
        with open_serial(args.port) as stream:
            stream.write(f"{case} {args.run_id}\n".encode("ascii"))
            stream.flush()
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline:
                line = stream.readline().decode(errors="replace").strip()
                if line:
                    report["lines"].append(line)
                    if line.startswith(done):
                        report["passed"] = line == done + "1"
                        break

        if report["passed"] and case in ("N", "R"):
            blocks = []
            for label in ("ab", "cd", "ef"):
                prefix = f"[TES006-DATA] op=read_{label} "
                line = next(line for line in report["lines"] if line.startswith(prefix))
                blocks.append(bytes.fromhex(line.split("hex=", 1)[1]))
            content = b"".join(blocks)
            report["extracted_hex"] = content.hex()
            report["passed"] = content == b"ABCDEF"
    except Exception as error:
        report["passed"] = False
        report["error"] = str(error)
        raise
    finally:
        with args.report.open("x") as output:
            json.dump(report, output, indent=2)

    if not report["passed"]:
        for line in report["lines"][-15:]:
            print(line)
        raise SystemExit(f"TES-006 {args.scenario}: verificação pendente ou reprovada. {args.report}")
    print(f"TES-006 {args.scenario}: passou. Relatório em {args.report}")


if __name__ == "__main__":
    main()
