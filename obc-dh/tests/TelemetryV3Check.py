#!/usr/bin/env python3
"""Vetores struct/zlib independentes e leitura real de arquivos v1/v2/v3."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


def record(magic, version, kind, body, ms=5000):
    raw = struct.pack("<4sBBHQQ", magic, version, kind, 28 + len(body), 7, ms) + body
    return raw + struct.pack("<I", zlib.crc32(raw))


def main():
    assert len(sys.argv) == 3, "Uso: TelemetryV3Check.py verificador leitor"
    check, reader = (str(Path(arg).resolve()) for arg in sys.argv[1:])
    mask = 1 | (1 << 14) | (7 << 16)
    body = struct.pack("<IIB11fHIhfHHH", mask, 0, 0, *([0.0] * 11), 0, 0, 0, 0.0, 3, 1, 2)
    tm = record(b"FCP3", 3, 9, body)
    boot = record(b"FCP1", 1, 6, struct.pack("<QI", 6, 0), 0)
    snapshot = record(b"FCP2", 2, 8, struct.pack("<QB", 0, 0))
    with tempfile.TemporaryDirectory(prefix="fcp01-tm-v3-") as directory:
        path = Path(directory) / "records.bin"
        path.write_bytes(tm)
        subprocess.run([check, str(path)], check=True)
        original = boot + snapshot + tm
        path.write_bytes(original)
        result = subprocess.run([reader, str(path)], capture_output=True, text=True, check=True)
        rows = [json.loads(line) for line in result.stdout.splitlines()]
        assert [row["version"] for row in rows] == [1, 2, 3], rows
        data = rows[2]["data"]
        assert data["ts"] == 0 and data["ac"] == 0 and data["msg"] is None, data
        assert data["rssi"] is None and data["rx"] == 3 and data["err"] == 1 and data["rst"] == 2, data
        assert path.read_bytes() == original
        corrupt = bytearray(tm)
        corrupt[-1] ^= 1
        path.write_bytes(boot + corrupt + snapshot + tm)
        result = subprocess.run([reader, str(path)], capture_output=True, text=True)
        assert result.returncode == 1, result.stderr
        assert [json.loads(line)["version"] for line in result.stdout.splitlines()] == [1, 2, 3]
    print("TM v3: vetor independente, ausência, corrupção e leitura mista aprovados")


if __name__ == "__main__":
    main()
