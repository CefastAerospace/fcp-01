#!/usr/bin/env python3
"""INT-006. Vetores independentes e extração local usando arquivos temporários reais."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


def record(boot, sequence, *, confirmation=False, altitude=100, ms=123):
    if confirmation:
        body = struct.pack("<4sBBHQQQ", b"FCD1", 1, 2, 36, boot, sequence, 0)
    else:
        body = struct.pack(
            "<4sBBHQQQIB3xddd", b"FCD1", 1, 1, 68,
            boot, sequence, ms, 0xABCDEF, 4, 0.0, 0.0, altitude,
        )
    return body + struct.pack("<I", zlib.crc32(body))


def run(executable, directory, *args, expected=0):
    result = subprocess.run(
        [executable, str(directory), *args], capture_output=True, check=False,
    )
    assert result.returncode == expected, (args, result.returncode, result.stderr)
    return result


def main():
    executable = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="fcp01-delivery-cli-") as temporary:
        directory = Path(temporary)
        first = record(8, 1)
        second = record(8, 2, altitude=200, ms=999)
        third = record(9, 1, altitude=100, ms=1)
        # BOOT v1 independente também deve atravessar o scanner em blocos.
        boot_body = struct.pack("<4sBBHQQQI", b"FCP1", 1, 6, 40, 8, 0, 7, 0)
        boot = boot_body + struct.pack("<I", zlib.crc32(boot_body))
        name = "boot_0000000000000008_00000000.bin"
        source = boot + first + second
        (directory / name).write_bytes(source)
        next_name = "boot_0000000000000009_00000000.bin"
        (directory / next_name).write_bytes(record(8, 1, confirmation=True) + third)
        listed = run(executable, directory, "list").stdout.decode().splitlines()
        assert len(listed) == 2 and listed[0].split("\t") == [name, str(len(source)), "closed"]
        extracted = bytearray()
        for offset in range(0, len(source), 7):
            extracted.extend(run(executable, directory, "read", name, str(offset), "7").stdout)
        assert bytes(extracted) == source
        assert run(executable, directory, "read", name, str(len(source)), "7").stdout == b""
        run(executable, directory, "read", name, str(len(source) + 1), "7", expected=2)
        run(executable, directory, "read", "missing.bin", "0", "7", expected=2)
        run(executable, directory, "read", "../escape.bin", "0", "7", expected=2)
        recovered = run(executable, directory, "recover")
        pending = [json.loads(line) for line in recovered.stdout.splitlines()]
        assert [(row["boot"], row["sequence"]) for row in pending] == [
            ("0000000000000008", "0000000000000002"),
            ("0000000000000009", "0000000000000001"),
        ]
        assert pending[0]["monotonic_ms"] == 999 and pending[0]["alt"] == 200
        assert all(pending[0][key] is None for key in ["lat", "lon", "vel", "hdg", "ts", "count"])
        if len(sys.argv) > 2:
            reader = str(Path(sys.argv[2]).resolve())
            for filename, types in [
                (name, ["boot", "delivery_data", "delivery_data"]),
                (next_name, ["delivery_confirm", "delivery_data"]),
            ]:
                result = subprocess.run([reader, str(directory / filename)], capture_output=True, check=True)
                assert [json.loads(line)["type"] for line in result.stdout.splitlines()] == types
        # Um CONFIRM incompleto ou inválido conserva o DATA correspondente.
        broken_ack = bytearray(record(8, 2, confirmation=True))
        broken_ack[-1] ^= 1
        damaged_name = "boot_000000000000000a_00000000.bin"
        (directory / damaged_name).write_bytes(
            broken_ack + record(88, 99, confirmation=True)
            + record(8, 2, confirmation=True)[:20]
        )
        damaged = run(executable, directory, "recover", expected=1)
        assert damaged.stdout == recovered.stdout
        assert b"orphan_confirmations=1" in damaged.stderr and b"incomplete_tails=1" in damaged.stderr
        # Versão/tamanho/conteúdo inválidos continuam inválidos com CRC recalculado.
        for offset, value in [(4, 2), (6, 67), (36, 0x80), (37, 1)]:
            invalid = bytearray(first)
            invalid[offset] = value
            invalid[-4:] = struct.pack("<I", zlib.crc32(invalid[:-4]))
            (directory / damaged_name).write_bytes(invalid)
            result = run(executable, directory, "recover", expected=1)
            assert result.stdout == recovered.stdout
        # Backlog durável maior que as 32 posições do cache conserva a ordenação.
        backlog = directory / "backlog"
        backlog.mkdir()
        (backlog / name).write_bytes(b"".join(record(8, sequence) for sequence in range(1, 41)))
        result = run(executable, backlog, "recover")
        assert [int(json.loads(line)["sequence"], 16) for line in result.stdout.splitlines()] == list(range(1, 41))
    print("INT-006 independent vectors and closed-file extraction passed")


if __name__ == "__main__":
    main()
