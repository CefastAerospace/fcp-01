#!/usr/bin/env python3
"""Vetores adicionais independentes; não altera flow-fixtures.fcpu."""
import json
from pathlib import Path
import struct
import zlib


def BuildFixtures(root, scenario):
    def frame(body, session, sequence=1, padded=False):
        encoded = json.dumps(body, separators=(",", ":")).encode()
        if padded:
            encoded += b" " * (65536 - len(encoded))
        assert len(encoded) <= 65536
        header = struct.pack("<4sBBQII", b"FCPU", 1, 1, session, sequence, len(encoded))
        data = header + encoded
        return data + struct.pack("<I", zlib.crc32(data))

    def small(identity, altitude=100):
        return {"id": identity, "messages": {"000001": {
            "Latitude": "0", "Longitude": "0", "Altitude": f"{altitude} feet"}}}

    if scenario == "capacity":
        fixtures = b"".join(frame(small(index, 99 + index), 42, index)
                            for index in range(1, 42))
    else:
        assert scenario == "memory"
        dense = {"id": 3, "messages": {f"{index:06X}": {
            "Latitude": "0", "Longitude": "0", "Altitude": "100 feet",
            "Identification": "i" * 16, "tick": "t" * 40,
            **{f"{extra:02d}" + "k" * 30: "v" * 56 for extra in range(32)}}
            for index in range(1, 21)}}
        exhausted = {"id": 4, "messages": {}, "unknown": [None] * 10000}
        assert len(json.dumps(dense, separators=(",", ":")).encode()) == 63181
        fixtures = frame(small(1), 1001) + frame(small(2), 1002, padded=True)
        fixtures += frame(dense, 1003) + frame(exhausted, 1004) + frame(small(5, 101), 1005)
    destination = root / ".pio" / f"{scenario}-fixtures.fcpu"
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_bytes() != fixtures:
        destination.write_bytes(fixtures)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenario", choices=("capacity", "memory"))
    args = parser.parse_args()
    BuildFixtures(Path(__file__).resolve().parents[1], args.scenario)
else:
    Import("env")
    scenario = "capacity" if env.subst("$PIOENV") == "int006-capacity-check" else "memory"
    BuildFixtures(Path(env.subst("$PROJECT_DIR")), scenario)
