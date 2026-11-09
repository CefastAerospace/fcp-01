#!/usr/bin/env python3
"""Gera quadros independentes struct/zlib e executa o receptor C++ real."""

import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


def frame(body, sequence=1, session=10, kind=1, version=1):
    if isinstance(body, dict):
        body = json.dumps(body, separators=(",", ":")).encode()
    header = struct.pack("<4sBBQII", b"FCPU", version, kind, session, sequence, len(body))
    return header + body + struct.pack("<I", zlib.crc32(header + body))


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Uso: python3 tests/PayloadReceiverCheck.py /caminho/do/executavel")
    executable = str(Path(sys.argv[1]).resolve())
    sample = Path(__file__).resolve().parents[1] / "payload.json"
    with tempfile.TemporaryDirectory(prefix="fcp01-uart-") as directory:
        root = Path(directory)

        def save(name, body, **kwargs):
            (root / name).write_bytes(frame(body, **kwargs))

        save("sample", sample.read_bytes())
        save("prior", sample.read_bytes(), session=100)
        empty = {"id": 0, "messages": {}}
        save("empty2", empty, sequence=2)
        save("empty3", empty, sequence=3)
        save("gap", empty, sequence=4)
        save("restart", empty, session=11)
        save("restart_bad", empty, session=11, sequence=2)
        save("baseline", empty, sequence=99)
        save("max_sequence", empty, sequence=0xFFFFFFFF)
        save("zero_sequence", empty, sequence=0)
        save("zero_session", empty, session=0)
        save("wrong_version", empty, version=2)
        save("wrong_type", b"", kind=2)
        save("max_id", {"id": 2**64 - 1, "messages": {}})
        save("overflow_id", {"id": 2**64, "messages": {}})
        save("negative_id", {"id": -1, "messages": {}})
        save("float_id", {"id": 1.0, "messages": {}})
        save("missing_id", {"messages": {}})
        save("wrong_messages", {"id": 1, "messages": []})
        save("extra_root", {"id": 1, "messages": {}, "unknown": "x"})
        save("same_tuple_changed", {"id": 7, "messages": {}})
        save("valid_fields", {"id": 123, "messages": {"ABCDEF": {
            "ICAO Address": "abcdef", "Identification": "", "tick": "opaque",
            "Latitude": "0", "Longitude": "-180", "Altitude": "-10 feet",
            "EW velocity": "-12", "diagnostic": "line\nquote\"slash\\"}}})
        save("undecoded", {"id": 2, "messages": {"e495b2": {
            "Latitude": "96298 (not decoded)", "Longitude": "26514 (not decoded)",
            "Altitude": "13050 feet"}}})
        save("one_coordinate", {"id": 2, "messages": {"000001": {
            "Latitude": "12.5", "Longitude": None}}})
        save("null_fields", {"id": 2, "messages": {"000001": {
            "Identification": None, "tick": None, "Altitude": None,
            "Latitude": None, "Longitude": None}}})
        save("bad_numbers", {"id": 2, "messages": {"000001": {
            "Latitude": "91", "Longitude": "0x10", "Altitude": "200 meters"}}})
        for name, fields in {
            "bad_icao_attribute": {"ICAO Address": "000002"},
            "wrong_extra": {"extra": 1},
            "null_extra": {"extra": None},
            "object_extra": {"extra": {}},
            "nul_extra": {"extra": "before\0after"},
            "nul_key": {"before\0after": "value"},
            "empty_key": {"": "value"},
            "key_overflow": {"k" * 33: "value"},
            "value_overflow": {"extra": "v" * 65},
            "identification_overflow": {"Identification": "i" * 17},
            "tick_overflow": {"tick": "t" * 41},
            "extras_overflow": {f"k{i}": "v" for i in range(33)},
            "field_type": {"Altitude": 30000},
        }.items():
            save(name, {"id": 1, "messages": {"000001": fields}})
        save("bad_icao", {"id": 1, "messages": {"E": {}}})
        save("duplicate_icao", {"id": 1, "messages": {"abcdef": {}, "ABCDEF": {}}})
        save("max_aircraft", {"id": 1, "messages": {f"{i:06X}": {} for i in range(20)}})
        save("aircraft_overflow", {"id": 1, "messages": {f"{i:06X}": {} for i in range(21)}})
        save("max_fields", {"id": 1, "messages": {"000001": {
            "Identification": "i" * 16, "tick": "t" * 40,
            **{f"{i:02d}" + "k" * 30: "v" * 64 for i in range(32)}}}})
        small = json.dumps(empty, separators=(",", ":")).encode()
        save("max_wire", small + b" " * (65536 - len(small)))
        oversize = struct.pack("<4sBBQII", b"FCPU", 1, 1, 10, 1, 65537)
        (root / "oversize_header").write_bytes(oversize)
        save("no_memory", {"id": 1, "messages": {}, "unknown": [None] * 10000})
        for name, raw in {
            "trailing": b'{"id":1,"messages":{}}garbage',
            "single_quotes": b"{'id':1,'messages':{}}",
            "bare_key": b'{id:1,"messages":{}}',
            "leading_zero": b'{"id":01,"messages":{}}',
            "trailing_comma": b'{"id":1,"messages":{},}',
            "broken_json": b'{"id":',
            "raw_control": b'{"id":1,"messages":{"000001":{"x":"a\nb"}}}',
            "two_objects": b'{"id":1,"messages":{}}{}',
            "deep": b'{"id":1,"messages":{},"x":[[[[[]]]]]}',
        }.items():
            save(name, raw)
        corrupted = bytearray((root / "sample").read_bytes())
        corrupted[-1] ^= 1
        (root / "bad_crc").write_bytes(corrupted)
        nested = bytearray(frame((root / "sample").read_bytes(), session=20))
        nested[-1] ^= 1
        (root / "nested_bad_crc").write_bytes(nested)
        save("expected_ack", b"", kind=2)
        # NACK para salto 1 -> 4: motivo 5, próxima sequência esperada 2.
        save("expected_nack", struct.pack("<BI", 5, 2), sequence=4, kind=3)
        subprocess.run([executable, directory], check=True)


if __name__ == "__main__":
    main()
