#!/usr/bin/env python3
"""Compare the FCP4 codec against independent wire vectors and verify mixed-file export."""

import argparse
import json
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path


def frame(magic, version, kind, tick, body):
    content = struct.pack("<4sBBHQQ", magic, version, kind, 28 + len(body), 7, tick) + body
    return content + struct.pack("<I", zlib.crc32(content))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("encoder", type=Path)
    parser.add_argument("reader", type=Path)
    parser.add_argument("delivery_logs", type=Path)
    args = parser.parse_args()
    eps = frame(b"FCP4", 4, 10, 125, struct.pack(
        "<QQ10B6f", 120, 2, 2, 0x44, 2, 0, 1, 2, 4, 0, 2, 0x3f,
        3.25, 3.5, 10.0, -0.125, -0.40625, -0.4375,
    ))
    timeout = frame(b"FCP4", 4, 11, 125, struct.pack(
        "<QQ10Bf", 120, 2, 4, 0x4c, 2, 5, 0, 0, 0, 0, 0, 0, 0.0,
    ))
    zero = frame(b"FCP4", 4, 11, 125, struct.pack(
        "<QQ10Bf", 120, 2, 4, 0x4c, 2, 0, 1, 0, 0, 0, 0, 1, 0.0,
    ))
    assert subprocess.check_output([args.encoder.resolve()]) == eps + timeout + zero
    reference = frame(b"FCP1", 1, 7, 25, struct.pack("<II", 1700000000, 0))
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "mixed.bin"
        path.write_bytes(reference + eps + timeout + zero)
        result = subprocess.run([args.reader.resolve(), path], check=True, capture_output=True)
        records = [json.loads(line) for line in result.stdout.splitlines()]
        assert [row["type"] for row in records] == [
            "time_reference", "eps_acquisition", "thermal_acquisition", "thermal_acquisition",
        ]
        assert records[1]["data"]["channel"] == 2 and records[1]["data"]["current_A"] == -0.125
        assert records[1]["unix_ms"] == 1700000000100
        assert records[2]["data"]["temperature_C"] is None
        assert records[3]["data"]["temperature_C"] == 0
        corrupt = eps[:-1] + bytes([eps[-1] ^ 1])
        path.write_bytes(reference + corrupt + zero + timeout[:40])
        result = subprocess.run([args.reader.resolve(), path], capture_output=True)
        assert result.returncode == 1
        assert b"CRC incorreto" in result.stderr
        assert [json.loads(line)["type"] for line in result.stdout.splitlines()] == [
            "time_reference", "thermal_acquisition",
        ]
        path.unlink()
        body = struct.pack("<4sBBHQQQIB3xddd", b"FCD1", 1, 1, 68, 7, 1, 125,
                           0xABCDEF, 4, 0.0, 0.0, 100.0)
        delivery = body + struct.pack("<I", zlib.crc32(body))
        segment = Path(directory) / "boot_0000000000000007_00000000.bin"
        combined = reference + eps + delivery + zero
        segment.write_bytes(combined)
        result = subprocess.run([args.delivery_logs.resolve(), directory, "recover"],
                                check=True, capture_output=True)
        pending = [json.loads(line) for line in result.stdout.splitlines()]
        assert [(row["sequence"], row["alt"]) for row in pending] == [("0000000000000001", 100)]
        assert b"corrupt_bytes=0 incomplete_tails=0" in result.stderr
        result = subprocess.run([args.reader.resolve(), segment], check=True, capture_output=True)
        assert [json.loads(line)["type"] for line in result.stdout.splitlines()] == [
            "time_reference", "eps_acquisition", "delivery_data", "thermal_acquisition",
        ]
        assert segment.read_bytes() == combined
    print("FCP4: codificador, fila, CRC, truncamento, leitor e recuperação FCD1 aprovados")


if __name__ == "__main__":
    main()
