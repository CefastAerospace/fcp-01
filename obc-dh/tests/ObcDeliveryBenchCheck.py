#!/usr/bin/env python3
"""INT-006, bancada int006-delivery-check após boot novo. Requer pyserial."""
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
    parser.add_argument("--seconds", type=float, default=45)
    parser.add_argument("--report", type=Path, default=Path("/tmp/fcp01-int006-deliveries.json"))
    args = parser.parse_args()
    report = {"passed": False, "port": args.port, "lines": [], "deliveries": {}, "flow": []}
    stream = None
    try:
        stream = open_serial(args.port)
        sequence = int(time.time()) & 0xFFFF
        stream.write(encode_tc(sequence, "set_time", {"unix": int(time.time())}))
        stream.flush()
        acknowledged = False
        retried = False
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            line = stream.readline().decode(errors="replace").strip()
            if not line:
                continue
            report["lines"].append(line)
            assert "FAIL" not in line, line
            parsed = parse_line(line)
            if parsed.kind == "packet" and parsed.packet["t"] == "ack" and parsed.packet["seq"] == sequence:
                assert parsed.packet["st"] == "ok", parsed.packet
                acknowledged = True
            fields = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)}
            if line.startswith("[DELIVERY-CHECK] tx_boot="):
                key = f"{fields['tx_boot']}/{fields['tx_sequence']}"
                original = report["deliveries"].setdefault(key, fields["original_ms"])
                assert original == fields["original_ms"], fields
                retried |= fields["retry"] == 1
            if line.startswith("[FLOW] received="):
                report["flow"].append(fields)
            if line.startswith("[DELIVERY] occupied="):
                report["statistics"] = fields
        assert acknowledged and retried and report["deliveries"], report
        assert report["statistics"]["confirmed"] > 0, report
        assert report["statistics"]["occupied"] <= 32, report
        assert report["flow"], report
        assert max(sample["poll_gap_ms"] for sample in report["flow"]) <= 10, report
        assert all(sample["stack_rx"] > 0 and sample["stack_sd"] > 0 and sample["stack_uart"] > 0
                   for sample in report["flow"]), report
        report["passed"] = True
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        if stream is not None:
            stream.close()
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"INT-006 delivery bench passed; {args.report}")


if __name__ == "__main__":
    main()
