"""Generate independent v2 vectors and exercise the real codec and file reader.

Run from obc-dh/ after building both executables:
python3 tests/SnapshotRecordsCheck.py /tmp/fcp01-snapshot-codec-check /tmp/fcp01-read-records
"""

import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


def frame(body, *, version=2, record_type=8, boot=1, monotonic=0, magic=b"FCP2"):
    """Build the documented envelope using only Python's standard library."""
    header = struct.pack("<4sBBHQQ", magic, version, record_type, len(body) + 28, boot, monotonic)
    data = header + body
    return data + struct.pack("<I", zlib.crc32(data))


def text(value):
    data = value.encode("utf-8") if isinstance(value, str) else value
    return bytes([len(data)]) + data


def aircraft(data):
    """Lengths and optional fields are encoded independently of C++ helpers."""
    names = ("lat", "lon", "alt", "identification", "tick")
    fields = data.get("fields", sum(1 << bit for bit, name in enumerate(names) if name in data))
    result = data["icao"].to_bytes(3, "little") + bytes([fields])
    for bit, name in enumerate(names[:3]):
        if fields & (1 << bit):
            result += struct.pack("<d", data[name])
    for bit, name in enumerate(names[3:], 3):
        if fields & (1 << bit):
            result += text(data[name])
    extras = data.get("extras", [])
    result += bytes([len(extras)])
    for key, value in extras:
        result += text(key) + text(value)
    return result


def snapshot(items=(), *, identifier=0, boot=1, monotonic=0):
    body = struct.pack("<QB", identifier, len(items)) + b"".join(map(aircraft, items))
    return frame(body, boot=boot, monotonic=monotonic)


def main():
    assert len(sys.argv) == 3, "Expected codec-check and read-records executable paths"
    codec, reader = map(lambda arg: str(Path(arg).resolve()), sys.argv[1:])
    maximum_integer = 2**64 - 1
    first = {
        "icao": 0xE49A48, "lat": -20.109775, "lon": -44.370883, "alt": 38000,
        "identification": "GLO1669", "tick": "2026-11-03T17:50:21.280810+00:00",
        "extras": [("EW velocity", "-282"), ("note", 'line\n"x"\\é')],
    }
    second = {
        "icao": 0xE495B2, "alt": -0.0, "tick": "",
        "extras": [("Latitude", "96298 (not decoded)"), ("Longitude", "26514 (not decoded)")],
    }
    largest = [
        {
            "icao": index, "lat": 90, "lon": -180, "alt": -0.0,
            "identification": "I" * 16, "tick": "T" * 40,
            "extras": [(chr(65 + extra) + "K" * 31, "V" * 64) for extra in range(32)],
        }
        for index in range(20)
    ]
    vectors = [
        snapshot(),
        snapshot([{"icao": 0xFFFFFF}], identifier=maximum_integer, boot=maximum_integer, monotonic=maximum_integer),
        snapshot([first, second], identifier=480, boot=7, monotonic=5000),
        snapshot(largest, identifier=maximum_integer, boot=maximum_integer, monotonic=maximum_integer),
    ]
    assert [len(data) for data in vectors[:2]] == [37, 42]
    assert len(vectors[3]) == 64497
    with tempfile.TemporaryDirectory(prefix="fcp01-snapshot-") as temporary:
        directory = Path(temporary)
        for index, data in enumerate(vectors):
            (directory / f"{index}.bin").write_bytes(data)
        subprocess.run([codec, str(directory)], check=True)

        def read(data, expected=0):
            source = directory / "input.bin"
            source.write_bytes(data)
            result = subprocess.run([reader, str(source)], capture_output=True, text=True, check=False)
            assert result.returncode == expected, (result.returncode, expected, result.stderr)
            assert source.read_bytes() == data, "Reader modified its input"
            rows = [json.loads(line) for line in result.stdout.splitlines()]
            return rows, result.stderr

        empty = read(vectors[0])[0][0]
        assert empty["type"] == "payload_snapshot" and empty["version"] == 2
        assert empty["data"] == {"id": 0, "aircraft": []}
        assert empty["unix_ms"] is None and empty["time_status"] == "no_reference"
        identity = read(vectors[1])[0][0]
        assert identity["boot_id"] == maximum_integer and identity["monotonic_ms"] == maximum_integer
        assert identity["data"]["id"] == maximum_integer
        plane = identity["data"]["aircraft"][0]
        assert plane == {"icao": "FFFFFF", "identification": None, "lat": None, "lon": None,
                         "alt": None, "tick": None, "extras": []}
        sample = read(vectors[2])[0][0]["data"]
        assert sample["id"] == 480 and len(sample["aircraft"]) == 2
        assert sample["aircraft"][0]["identification"] == first["identification"]
        assert sample["aircraft"][0]["tick"] == first["tick"]
        assert sample["aircraft"][0]["lat"] == first["lat"]
        assert sample["aircraft"][0]["extras"][1]["value"] == first["extras"][1][1]
        assert sample["aircraft"][1]["lat"] is None and sample["aircraft"][1]["lon"] is None
        assert math.copysign(1, sample["aircraft"][1]["alt"]) < 0
        assert sample["aircraft"][1]["tick"] == ""
        assert sample["aircraft"][1]["extras"][0]["value"] == "96298 (not decoded)"
        biggest = read(vectors[3])[0][0]["data"]["aircraft"]
        assert len(biggest) == 20 and all(len(plane["extras"]) == 32 for plane in biggest)
        assert biggest[-1]["identification"] == "I" * 16 and biggest[-1]["tick"] == "T" * 40
        assert biggest[-1]["extras"][-1] == {"key": chr(96) + "K" * 31, "value": "V" * 64}

        reference = frame(struct.pack("<II", 100, 162), version=1, record_type=7,
                          boot=7, monotonic=1000, magic=b"FCP1")
        boot = frame(struct.pack("<QI", 6, 0), version=1, record_type=6,
                     boot=7, magic=b"FCP1")
        mixed = vectors[2] + boot + vectors[0] + reference + vectors[1]
        rows, _ = read(mixed)
        assert [row["type"] for row in rows] == ["payload_snapshot", "boot", "payload_snapshot",
                                                "time_reference", "payload_snapshot"]
        assert [row["file_offset"] for row in rows] == [
            0, len(vectors[2]), len(vectors[2]) + len(boot),
            len(vectors[2]) + len(boot) + len(vectors[0]), len(mixed) - len(vectors[1]),
        ]
        assert rows[0]["unix_ms"] == 104162 and rows[1]["unix_ms"] == 99162
        assert rows[0]["reference_offset"] == rows[3]["file_offset"]
        assert rows[2]["unix_ms"] is None and rows[4]["unix_ms"] is None
        assert len(read(vectors[2] + vectors[2])[0]) == 2, "Opaque id must not deduplicate snapshots"

        embedded = snapshot([{"icao": 1, "extras": [("marker", "FCP1 FCP2")]}])
        assert len(read(embedded)[0]) == 1
        broken = bytearray(embedded)
        broken[-1] ^= 1
        rows, diagnostic = read(b"garbage" + broken + boot + vectors[0], 1)
        assert [row["type"] for row in rows] == ["boot", "payload_snapshot"]
        assert "Descartados bytes" in diagnostic
        crossing = b"\0" * (64497 - 2) + vectors[0]
        rows, _ = read(crossing, 1)
        assert len(rows) == 1 and rows[0]["file_offset"] == 64495

        for data in vectors:
            for prefix in sorted({1, 3, 4, 23, 24, 32, 33, len(data) - 1}):
                assert read(data[:prefix], 1)[0] == []
        assert read(b"")[0] == []
        for prefix in range(9, len(vectors[2]) - 28):
            # CRC and outer length remain valid, forcing checks inside the body.
            shortened = frame(vectors[2][24:24 + prefix], boot=7, monotonic=5000)
            assert read(shortened, 1)[0] == []

        invalid = [
            snapshot(boot=0),
            snapshot([{"icao": 1}, {"icao": 1}]),
            snapshot([{"icao": 1, "fields": 128}]),
            snapshot([{"icao": 1, "lat": 1}]),
            snapshot([{"icao": 1, "lat": float("nan"), "lon": 0}]),
            snapshot([{"icao": 1, "lat": 91, "lon": 0}]),
            snapshot([{"icao": 1, "alt": float("inf")}]),
            snapshot([{"icao": 1, "identification": "I" * 17}]),
            snapshot([{"icao": 1, "tick": "T" * 41}]),
            snapshot([{"icao": 1, "extras": [("K" * 33, "v")]}]),
            snapshot([{"icao": 1, "extras": [("k", "V" * 65)]}]),
            snapshot([{"icao": 1, "extras": [("k", "v"), ("k", "w")]}]),
            snapshot([{"icao": 1, "extras": [("", "v")]}]),
            snapshot([{"icao": 1, "extras": [("k", b"x\0y")]}]),
            snapshot([{"icao": 1, "extras": [(str(index), "v") for index in range(33)]}]),
            snapshot([{"icao": index} for index in range(21)]),
            frame(struct.pack("<QB", 0, 0) + b"trailing"),
        ]
        for data in invalid:
            assert read(data, 1)[0] == []
            recovered, _ = read(data + vectors[0], 1)
            assert len(recovered) == 1 and recovered[0]["file_offset"] == len(data)
        for position, replacement in ((4, 3), (5, 9)):
            data = bytearray(vectors[0])
            data[position] = replacement
            assert len(read(data + vectors[0], 1)[0]) == 1

        invalid_text = snapshot([{
            "icao": 1, "identification": b"\xed\xa0\x80", "tick": b"raw\xff",
            "extras": [(b"\xff", b"\xc0\xaf"), ("empty", "")],
        }])
        plane = read(invalid_text)[0][0]["data"]["aircraft"][0]
        assert plane["identification"] is None and plane["identification_hex"] == "eda080"
        assert plane["identification_encoding"] == "invalid_utf8" and plane["tick_hex"] == "726177ff"
        assert plane["extras"][0]["key"] is None and plane["extras"][0]["key_hex"] == "ff"
        assert plane["extras"][0]["value"] is None and plane["extras"][0]["value_hex"] == "c0af"
        assert plane["extras"][1] == {"key": "empty", "value": ""}

    print("Snapshot v2: leitor misto, referencias, recuperacao e preservacao da origem aprovados.")


if __name__ == "__main__":
    main()
