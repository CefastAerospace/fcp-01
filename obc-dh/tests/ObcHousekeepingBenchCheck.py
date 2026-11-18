#!/usr/bin/env python3
"""Verifica INT-009 conjunta por USB e interpreta os bytes aceitos pelo SD simulado.

--source acquisition exige ESP32 sem sensores; fixtures usa o ambiente próprio.
Não altera firmware ou valida sensores, cartão, RF, proteção ou durabilidade.
"""
import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial
from fcp_gs.protocol import encode_confirmation, encode_tc, parse_line


def expected_values(scenario):
    """Valores esperados independentes da conversão C++ e das suas flags."""
    expected = {"vb": 7.8, "ib": 420, "vs": 9.1, "is": 310,
                "tp": [21.5, 22, 23.1, 20.9, 24.3]}
    if scenario == "zero_negative":
        expected.update(vb=0, ib=0, **{"is": -310, "tp": [0, -12.5, 23.1, 20.9, 24.3]})
    elif scenario == "battery_missing":
        expected.update(vb=None, ib=None)
    elif scenario == "solar_missing":
        expected.update(vs=None, **{"is": None})
    elif scenario == "thermal_partial":
        expected["tp"][2] = None
    elif scenario in ("unavailable", "no_sensors"):
        expected = {"vb": None, "ib": None, "vs": None, "is": None, "tp": [None] * 5}
    else:
        assert scenario in ("normal", "recovered"), scenario
    return expected


def same_value(actual, expected):
    """Tolera somente o arredondamento documentado do enlace e o float binário."""
    if expected is None:
        assert actual is None, (actual, expected)
    elif isinstance(expected, list):
        assert isinstance(actual, list) and len(actual) == len(expected), (actual, expected)
        for value, wanted in zip(actual, expected):
            same_value(value, wanted)
    else:
        assert type(actual) in (int, float) and math.isfinite(actual), actual
        assert math.isclose(actual, expected, rel_tol=0, abs_tol=0.0001), (actual, expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--source", choices=("acquisition", "fixtures"), required=True)
    parser.add_argument("--reader", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    reader = args.reader.resolve(strict=True)
    binary_path = args.report.with_suffix(".records.bin")
    jsonl_path = args.report.with_suffix(".records.jsonl")
    report = {"passed": False, "source": args.source, "port": args.port,
              "started_utc": datetime.now(timezone.utc).isoformat(), "lines": [],
              "exchanges": [], "telemetry": [], "health": [], "producer_health": {},
              "samples": [], "record_count": 0, "reader": str(reader)}
    sequence = int(time.time() * 1000) & 0xffff
    sent, acknowledgments, records = {}, {}, []
    ready = None
    pending_sample = None
    last_counter = None
    stream = None

    def receive():
        nonlocal ready, pending_sample, last_counter
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return
        report["lines"].append(line)
        assert "Guru Meditation" not in line and "Backtrace:" not in line and "FAIL" not in line, line
        row = dict(re.findall(r"(\w+)=([^\s]+)", line))
        if line.startswith("[HK-CHECK]"):
            assert row["source"] == args.source, row
            if "ready" in row:
                assert ready is None, "Novo boot durante a bancada"
                assert row["ready"] == "1" and row["voltage"] == "vin_minus", row
                assert row["current"] == "A_to_mA", row
                started = "1" if args.source == "acquisition" else "0"
                assert row["eps_started"] == row["thermal_started"] == started, row
                ready = row
            else:
                pending_sample = {"scenario": row["scenario"], "sample_ms": int(row["sample_ms"])}
                report["samples"].append(pending_sample)
        elif line.startswith("[HK-RECORD]"):
            raw = bytes.fromhex(row["hex"])
            assert raw[:4] in (b"FCP1", b"FCP3") and len(raw) in (36, 40, 99), raw.hex()
            assert zlib.crc32(raw[:-4]) == struct.unpack_from("<I", raw, len(raw) - 4)[0], raw.hex()
            records.append(raw)
        elif line.startswith("[HK-HEALTH]"):
            assert int(row["heap_bytes"]) > 0 and int(row["block_bytes"]) > 0, row
            assert int(row["loop_stack_bytes"]) >= 1024, row
            report["health"].append(row)
        elif line.startswith(("[EPS-HEALTH]", "[THERMAL-HEALTH]")):
            assert args.source == "acquisition", "Fixture iniciou produtor"
            assert row["active"] == "1" and row["service"] == "1", row
            assert int(row["stack_free_bytes"]) >= 1024 and int(row["age_ms"]) <= 1000, row
            name = "eps" if line.startswith("[EPS-HEALTH]") else "thermal"
            previous = report["producer_health"].get(name)
            if previous:
                assert int(row["progress_ms"]) > int(previous["progress_ms"]), row
            assert int(row["operations"]) == int(row["failures"]), row
            report["producer_health"][name] = row
        elif line.startswith(("[EPS] channel=", "[THERMAL] channel=")):
            assert args.source == "acquisition" and row["available"] == row["has_valid"] == "0", row

        received = parse_line(line)
        if received.kind != "packet":
            return
        packet = received.packet
        assert len(line.encode()) <= 255, packet
        counter = packet["n"]
        assert type(counter) is int and 0 <= counter <= 65535, packet
        if last_counter is not None:
            assert counter == (last_counter + 1) % 65536, packet
        last_counter = counter
        if packet["t"] == "ack":
            seq = packet["seq"]
            assert seq in sent and packet["cmd"] == sent[seq]["cmd"], packet
            assert packet["st"] == "ok" and type(packet["q"]) is int and 0 <= packet["q"] <= 200, packet
            elapsed = (time.monotonic() - sent[seq]["started"]) * 1000
            assert elapsed <= 2000, (packet, elapsed)
            acknowledgments[seq] = packet
            report["exchanges"].append({"ack": packet, "response_ms": elapsed})
        elif packet["t"] == "tm":
            assert ready is not None and pending_sample is not None, packet
            expected = expected_values(pending_sample["scenario"])
            for key in ("vb", "ib", "vs", "is"):
                same_value(packet[key], expected[key])
            temperatures = expected["tp"]
            same_value(packet["tp"], None if None in temperatures else temperatures)
            assert packet["md"] == "bench" and packet["w"] is None and packet["az"] is None, packet
            report["telemetry"].append({"sample": pending_sample, "packet": packet})
            pending_sample = None
        elif packet["t"] == "ac":
            stream.write(encode_confirmation(tuple(packet["id"])))
            stream.flush()
        else:
            raise AssertionError(f"Pacote inesperado: {packet}")

    def exchange(command, arguments=None):
        nonlocal sequence
        sequence = (sequence + 1) & 0xffff
        seq = sequence
        before = len(report["telemetry"])
        sent[seq] = {"cmd": command, "started": time.monotonic()}
        stream.write(encode_tc(seq, command, arguments))
        stream.flush()
        needs_tm = command in ("request_tm", "get_status")
        while time.monotonic() - sent[seq]["started"] < 2:
            if seq in acknowledgments and (not needs_tm or len(report["telemetry"]) > before):
                return report["telemetry"][-1]["packet"] if needs_tm else None
            receive()
        raise AssertionError(f"Sem ACK/TM em 2 s: {command}, seq={seq}")

    # Todos os destinos são exclusivos, antes de tocar na porta da placa.
    with args.report.open("x", encoding="utf-8") as output:
        try:
            with binary_path.open("xb") as binary, jsonl_path.open("x", encoding="utf-8") as jsonl:
                stream = open_serial(args.port, settle_s=0.5, quiet_s=0.1, max_wait_s=0.8)
                deadline = time.monotonic() + 12
                while time.monotonic() < deadline and ready is None:
                    receive()
                assert ready is not None, "Marcador da bancada ausente"
                first = exchange("get_status")
                assert first["ts"] is None, first
                exchange("set_time", {"unix": 1760000000})
                required = {"normal", "zero_negative", "battery_missing", "solar_missing",
                            "thermal_partial", "unavailable", "recovered"}
                started = time.monotonic()
                while time.monotonic() - started < (45 if args.source == "fixtures" else 30):
                    exchange("ping")
                    packet = exchange("request_tm")
                    assert type(packet["ts"]) is int and packet["ts"] >= 1760000000, packet
                    seen = {row["sample"]["scenario"] for row in report["telemetry"]}
                    if args.source == "fixtures" and required <= seen:
                        break
                    until = time.monotonic() + 0.8
                    while time.monotonic() < until:
                        receive()
                report["observed_s"] = time.monotonic() - started
                if args.source == "fixtures":
                    assert required <= {row["sample"]["scenario"] for row in report["telemetry"]}, "Cenario ausente"
                    assert not report["producer_health"], "Aquisicao em ambiente de fixtures"
                else:
                    assert set(report["producer_health"]) == {"eps", "thermal"}, "Produtor sem progresso"
                assert report["health"], "Memoria/pilha sem diagnostico"

                # O escritor é assíncrono; espera todos os registros da TM recebida.
                deadline = time.monotonic() + 4
                while time.monotonic() < deadline and sum(raw[:4] == b"FCP3" for raw in records) < len(report["telemetry"]):
                    receive()
                assert sum(raw[:4] == b"FCP3" for raw in records) == len(report["telemetry"]), "TM nao capturada"
                binary.write(b"".join(records))
                binary.flush()
                result = subprocess.run([str(reader), str(binary_path)], text=True, capture_output=True)
                report["reader_stderr"] = result.stderr
                jsonl.write(result.stdout)
                jsonl.flush()
                assert result.returncode == 0, (result.returncode, result.stderr)
                decoded = [json.loads(line) for line in result.stdout.splitlines()]
                assert any(row["type"] == "boot" for row in decoded), "BOOT nao capturado"
                assert any(row["type"] == "time_reference" for row in decoded), "Referencia nao capturada"
                telemetry = {row["monotonic_ms"]: row["data"] for row in decoded if row["type"] == "telemetry"}
                for row in report["telemetry"]:
                    stored = telemetry[row["sample"]["sample_ms"]]
                    expected = expected_values(row["sample"]["scenario"])
                    for key, value in expected.items():
                        same_value(stored[key], value)
                    for key in ("ts", "md", "w", "az", "ac", "msg", "rssi", "snr", "rx", "err", "rst"):
                        assert stored[key] == row["packet"][key], (key, stored, row)
                report["record_count"] = len(records)
                report["artifacts"] = {"binary": binary_path.name, "jsonl": jsonl_path.name}
                report["passed"] = True
        except BaseException as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            if stream is not None:
                stream.close()
            report["finished_utc"] = datetime.now(timezone.utc).isoformat()
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(f"Housekeeping USB ({args.source}): TC/TM e registros simulados aprovados; {args.report}")


if __name__ == "__main__":
    main()
