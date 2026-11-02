"""Verifica o leitor C++ com arquivos independentes gerados por struct/zlib.

Uso: python3 tests/ReadRecordsCheck.py /tmp/fcp01-read-records
O interpretador de produção é C++; Python fornece somente o oráculo de teste.
"""

import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


def Frame(kind, body, boot_id=1, monotonic_ms=5000, version=1):
    """Produz o enquadramento documentado sem chamar o encoder sob teste."""
    raw = struct.pack("<4sBBHQQ", b"FCP1", version, kind, 28 + len(body), boot_id, monotonic_ms) + body
    return raw + struct.pack("<I", zlib.crc32(raw))


def Event(text=b"startup", boot_id=1, monotonic_ms=5000):
    """Evento com texto fixo, inclusive bytes que não são UTF-8."""
    return Frame(3, struct.pack("<BHB64s", 1, 1, 0, text), boot_id, monotonic_ms)


def Reference(monotonic_ms, unix_s, delay_ms, boot_id=1):
    """Referência separada do horário do Payload, como no contrato v1."""
    return Frame(7, struct.pack("<II", unix_s, delay_ms), boot_id, monotonic_ms)


def Main():
    """Exercita todos os layouts, recuperação, tempos e saída JSON real."""
    assert len(sys.argv) == 2, "Informe o executável read-records."
    executable = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="fcp01-records-") as directory:
        source = Path(directory) / "input.bin"

        def Run(raw, expected_code=0):
            """Confere saída/código e que a leitura não altera nenhum byte."""
            source.write_bytes(raw)
            result = subprocess.run([executable, str(source)], capture_output=True)
            assert result.returncode == expected_code, result.stderr.decode("utf-8", errors="replace")
            assert source.read_bytes() == raw
            rows = [json.loads(line) for line in result.stdout.decode("utf-8").splitlines()]
            return rows, result.stderr.decode("utf-8")

        mission = Frame(1, struct.pack("<QI3sBB5d20s", 42, 12, bytes.fromhex("3a95e4"), 3, 0x27,
                                      -19.95438, -43.6116, 39000, 0, 0, b"2026-09-08T00:21:44Z"))
        telemetry = Frame(2, struct.pack("<IIB4f5f2fHIhf3H", 0x3FFF, 1760000123, 4,
                                        7.81, 420, 9.1, 310, 21.5, 22, 23.1, 20.9, 24.3,
                                        12.4, 37, 3, 1200, -81, 9.5, 10, 0, 0))
        fault = Frame(4, struct.pack("<BHB64s", 7, 1, 2, b"sd unavailable"))
        statistics = Frame(5, struct.pack("<7Q", 100, 2, 3, 1, 96, 1, 1))
        boot = Frame(6, struct.pack("<QI", 0, 0), monotonic_ms=0)
        reference = Reference(5000, 1760000000, 162)
        raw = mission + telemetry + Event() + fault + statistics + boot + reference
        rows, diagnostics = Run(raw)
        assert [row["type"] for row in rows] == ["mission", "telemetry", "event", "fault", "statistics", "boot", "time_reference"]
        assert [row["file_offset"] for row in rows] == [0, 105, 204, 300, 396, 480, 520]
        assert rows[0]["unix_ms"] == 1760000000162
        assert rows[0]["reference_offset"] == 520 and rows[0]["time_status"] == "estimated"
        assert rows[0]["data"]["icao"] == "E4953A" and rows[0]["data"]["lat"] == -19.95438
        assert rows[0]["data"]["timestamp"] == "2026-09-08T00:21:44Z"
        assert rows[0]["data"]["vel"] is None and rows[0]["data"]["heading"] is None
        assert rows[1]["data"]["ts"] == 1760000123 and rows[1]["data"]["rssi"] == -81
        assert struct.pack("<f", rows[1]["data"]["vb"]) == struct.pack("<f", 7.81)
        assert rows[3]["data"]["description"] == "sd unavailable"
        assert rows[4]["data"]["gaps"] == 3 and rows[5]["unix_ms"] == 1759999995162
        assert "bytes descartados: 0" in diagnostics

        # Uma assinatura no corpo íntegro não cria outro registro.
        rows, _ = Run(Event(b"texto FCP1 dentro do corpo"))
        assert len(rows) == 1 and rows[0]["data"]["description"] == "texto FCP1 dentro do corpo"
        assert rows[0]["unix_ms"] is None and rows[0]["reference_offset"] is None
        assert rows[0]["time_status"] == "no_reference"

        # Assinatura atravessa a fronteira da janela de 512 bytes.
        rows, diagnostics = Run(b"x" * 510 + boot, 1)
        assert len(rows) == 1 and rows[0]["file_offset"] == 510
        assert "[0,510)" in diagnostics and "bytes descartados: 510" in diagnostics
        rows, _ = Run(b"x" * 2000 + Event(), 1)
        assert len(rows) == 1 and rows[0]["file_offset"] == 2000

        damaged = bytearray(mission)
        damaged[60] ^= 1
        rows, diagnostics = Run(bytes(damaged) + boot, 1)
        assert len(rows) == 1 and rows[0]["file_offset"] == 105 and "CRC" in diagnostics
        # Cabeçalho inválido não pode ordenar um salto que esconda o próximo.
        bad_header = struct.pack("<4sBBHQQ", b"FCP1", 1, 1, 512, 1, 5000)
        rows, _ = Run(bad_header + boot + reference, 1)
        assert [row["file_offset"] for row in rows] == [24, 64]
        rows, _ = Run(Frame(6, struct.pack("<QI", 0, 0), version=2) + boot, 1)
        assert len(rows) == 1 and rows[0]["file_offset"] == 40
        rows, _ = Run(Frame(255, b"unknown") + boot, 1)
        assert len(rows) == 1 and rows[0]["type"] == "boot"
        bad_body = struct.pack("<QI3sBB5d20s", 42, 12, b"abc", 3, 0x80, 0, 0, 0, 0, 0, b"")
        rows, _ = Run(Frame(1, bad_body) + boot, 1)
        assert len(rows) == 1 and rows[0]["type"] == "boot"

        rows, diagnostics = Run(boot + mission[:-1], 1)
        assert len(rows) == 1 and "incompleto" in diagnostics
        rows, diagnostics = Run(boot + b"FCP", 1)
        assert len(rows) == 1 and "incompleto" in diagnostics
        rows, _ = Run(b"")
        assert rows == []

        # Referências fora da ordem de instante e empate resolvido pelo arquivo.
        events = b"".join(Event(monotonic_ms=value) for value in [100, 1000, 1800, 2500, 3000])
        refs = Reference(2000, 2000, 20) + Reference(1000, 1000, 10) + Reference(2000, 3000, 30)
        rows, _ = Run(events + refs)
        assert [row["unix_ms"] for row in rows[:5]] == [999110, 1000010, 1000810, 3000530, 3001030]
        assert [row["reference_offset"] for row in rows[:5]] == [516, 516, 516, 552, 552]
        # Empates na primeira referência também escolhem a última ocorrência.
        rows, _ = Run(Event(monotonic_ms=0) + Reference(1000, 1000, 10) + Reference(1000, 2000, 20))
        assert rows[0]["unix_ms"] == 1999020 and rows[0]["reference_offset"] == 132
        rows, _ = Run(Event(boot_id=2, monotonic_ms=100) + refs)
        assert rows[0]["time_status"] == "no_reference" and rows[0]["unix_ms"] is None
        rows, _ = Run(Event(boot_id=2, monotonic_ms=100) + refs + Reference(5000, 5000, 0, boot_id=2))
        assert rows[0]["unix_ms"] == 4995100
        rows, _ = Run(Event(monotonic_ms=0) + Reference(5000, 0, 0))
        assert rows[0]["time_status"] == "out_of_range" and rows[0]["unix_ms"] is None

        # Metadados/contagens de 64 bits não podem passar por double.
        maximum = (1 << 64) - 1
        rows, _ = Run(Event(boot_id=maximum, monotonic_ms=maximum)
                      + Frame(5, struct.pack("<7Q", *([maximum] * 7))))
        assert rows[0]["boot_id"] == maximum and rows[0]["monotonic_ms"] == maximum
        assert all(value == maximum for value in rows[1]["data"].values())

        for text in [b'quote" slash\\ newline\n tab\t control\x01', "órbita 🚀".encode("utf-8")]:
            rows, _ = Run(Event(text))
            assert rows[0]["data"]["description"] == text.decode("utf-8")
        for text in [b"\xff", b"\xed\xa0\x80", b"\xe0\x80\x80", b"\xf4\x90\x80\x80", b"\xc2"]:
            rows, _ = Run(Event(text))
            body = rows[0]["data"]
            assert body["description"] is None and body["description_encoding"] == "invalid_utf8"
            assert bytes.fromhex(body["description_hex"]) == text

        body = struct.pack("<QI3sBB5d20s", maximum, (1 << 32) - 1, b"\xff" * 3, 4, 8,
                           float("nan"), 0, 0, -0.0, 0, b"X" * 20)
        rows, _ = Run(Frame(1, body))
        assert rows[0]["data"]["session_id"] == maximum and rows[0]["data"]["icao"] == "FFFFFF"
        assert rows[0]["data"]["lat"] is None and rows[0]["data"]["timestamp"] is None
        assert math.copysign(1, rows[0]["data"]["vel"]) == -1

        assert subprocess.run([executable], capture_output=True).returncode == 2
        assert subprocess.run([executable, "--help"], capture_output=True).returncode == 0
        assert subprocess.run([executable, str(Path(directory) / "missing.bin")], capture_output=True).returncode == 2
    print("Leitor C++: layouts, recuperação, JSON, referências e preservação da origem aprovados.")


if __name__ == "__main__":
    Main()
