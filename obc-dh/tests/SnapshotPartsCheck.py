#!/usr/bin/env python3
"""Verifica buffer partido com o gerador struct/zlib já usado para snapshots."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from SnapshotRecordsCheck import snapshot


def main():
    assert len(sys.argv) == 3, "Uso: SnapshotPartsCheck.py verificador leitor"
    checker, reader = (str(Path(arg).resolve()) for arg in sys.argv[1:])
    largest = [{"icao": index, "lat": 90, "lon": -180, "alt": -0.0,
                "identification": "I" * 16, "tick": "T" * 40,
                "extras": [(chr(65 + extra) + "K" * 31, "V" * 64) for extra in range(32)]}
               for index in range(20)]
    maximum = 2**64 - 1
    with tempfile.TemporaryDirectory(prefix="fcp01-split-") as directory:
        root = Path(directory)
        (root / "0.bin").write_bytes(snapshot())
        golden = snapshot(largest, identifier=maximum, boot=maximum, monotonic=maximum)
        (root / "1.bin").write_bytes(golden)
        good, bad = root / "good", root / "bad"
        good.mkdir()
        bad.mkdir()
        subprocess.run([checker, str(root), str(good), str(bad)], check=True)
        name = "boot_ffffffffffffffff_00000000.bin"
        complete = good / name
        original = complete.read_bytes()
        assert original[40:] == golden
        result = subprocess.run([reader, str(complete)], capture_output=True, text=True, check=True)
        assert [json.loads(line)["version"] for line in result.stdout.splitlines()] == [1, 2]
        partial = bad / name
        assert partial.stat().st_size == 40 + 32768
        result = subprocess.run([reader, str(partial)], capture_output=True, text=True)
        assert result.returncode == 1, result.stderr
        assert [json.loads(line)["version"] for line in result.stdout.splitlines()] == [1]
        assert complete.read_bytes() == original
    print("Snapshot partido: bytes preservados e registro parcial não publicado pelo leitor")


if __name__ == "__main__":
    main()
