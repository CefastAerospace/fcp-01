#!/usr/bin/env python3
"""Gera FCPU independente em flash apenas para o ambiente int006-check."""
from pathlib import Path
import struct
import zlib


def BuildFixtures(root):
    sample = (root / "payload.json").read_bytes()

    def frame(sequence, body):
        data = struct.pack("<4sBBQII", b"FCPU", 1, 1, 42, sequence, len(body)) + body
        return data + struct.pack("<I", zlib.crc32(data))

    initial = frame(1, sample)
    corrupt = bytearray(frame(22, sample))
    corrupt[-1] ^= 1
    fixtures = initial + initial + b"".join(frame(seq, sample) for seq in range(2, 22))
    fixtures += corrupt + frame(22, b'{"id":0,"messages":{}}')
    destination = root / ".pio/flow-fixtures.fcpu"
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_bytes() != fixtures:
        destination.write_bytes(fixtures)


if __name__ == "__main__":
    BuildFixtures(Path(__file__).resolve().parents[1])
else:
    Import("env")
    BuildFixtures(Path(env.subst("$PROJECT_DIR")))
