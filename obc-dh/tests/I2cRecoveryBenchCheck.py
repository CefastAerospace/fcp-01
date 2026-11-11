#!/usr/bin/env python3
"""COM-003. Coleta a bancada i2c-recovery-check com executor FreeRTOS e sinais simulados."""
import argparse
import json
from pathlib import Path
import re
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--report", type=Path, default=Path("/tmp/fcp01-i2c-recovery-bench.json"))
    args = parser.parse_args()
    report = {"passed": False, "port": args.port, "simulated_gpio": True,
              "simulated_recovery_clock": True, "lines": []}
    stream = None
    try:
        stream = serial.Serial(args.port, 115200, timeout=0.2)
        time.sleep(1)
        stream.write(b"g")
        stream.flush()
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            line = stream.readline().decode(errors="replace").strip()
            if not line:
                continue
            report["lines"].append(line)
            assert "[I2C-CHECK] FAIL" not in line, line
            if not line.startswith("[I2C-CHECK] PASS "):
                continue
            fields = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)}
            assert fields["cycles"] == 3 and fields["attempts"] == 5, fields
            assert fields["recovered"] == 2 and fields["exhausted"] == 1, fields
            assert fields["context_hold_ms"] > fields["context_queue_ms"], fields
            assert fields["stack"] > 0 and fields["callback_max_us"] >= fields["context_queue_ms"] * 1000, fields
            report["diagnostics"] = fields
            report["passed"] = True
            break
        assert report["passed"], "Bancada sem confirmação PASS em 20 segundos"
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        if stream is not None:
            stream.close()
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"COM-003 bancada aprovada; {args.report}")


if __name__ == "__main__":
    main()
