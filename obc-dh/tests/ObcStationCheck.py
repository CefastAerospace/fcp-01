#!/usr/bin/env python3
"""Verifica cliente TT&C real, TM e tempo por USB após boot novo.

--bench exige int006-ttc-check, com Payload/SD simulados. Sem --bench, usa
obc sem periféricos. Não comprova RF, cartão ou durabilidade da estação.
As entradas e expectativas dos roteiros históricos permanecem intactas.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import threading
import time
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.deliveries import DeliveryStore
from fcp_gs.link import Link, open_serial
from fcp_gs.logs import LogError, download_file, list_files
from fcp_gs.session_log import SessionLog


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--bench", action="store_true")
    parser.add_argument("--report", type=Path, default=Path("/tmp/fcp01-obc-station.json"))
    parser.add_argument("--reader", type=Path, default=Path("/tmp/fcp01-read-records"))
    args = parser.parse_args()
    artifacts = args.report.with_suffix("")
    if args.report.exists() or artifacts.exists():
        parser.error("relatório ou diretório de evidências já existe; escolha um destino novo")
    if args.bench and not args.reader.is_file():
        parser.error("compile o leitor OBC existente ou informe --reader")
    artifacts.mkdir(parents=True)
    report = {"passed": False, "bench": args.bench, "port": args.port,
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "artifacts": str(artifacts), "exchanges": [], "telemetry": [], "files": []}
    condition = threading.Condition()
    events = []
    telemetry = []
    link = session = store = stream = None
    reference_unix = reference_started = None

    def record(event):
        # O listener só copia eventos; falhas são verificadas na tarefa principal.
        with condition:
            events.append({"monotonic": time.monotonic(), **event})
            if event.get("dir") == "rx" and event.get("pkt", {}).get("t") == "tm":
                telemetry.append(event["pkt"])
            condition.notify_all()

    def command(name, parameters=None, expected="ok"):
        result = link.send_tc(name, parameters, retries=1)
        report["exchanges"].append({"cmd": name, "args": parameters, "expected": expected, **result})
        assert result["status"] == expected, result
        assert result["ok"] == (expected == "ok"), result
        assert type(result["q"]) is int and 0 <= result["q"] <= 200, result
        assert result["rtt_ms"] is not None and 0 <= result["rtt_ms"] <= 2000, result
        return result

    def sample(name="request_tm"):
        # Uma única TM pendente: o contrato não associa TM a seq de TC.
        with condition:
            before = len(telemetry)
        started = time.monotonic()
        command(name)
        with condition:
            while len(telemetry) == before:
                remaining = started + 2 - time.monotonic()
                assert remaining > 0, "TM não recebida em 2 s"
                condition.wait(remaining)
            packet = telemetry[before]
        assert time.monotonic() - started <= 2, packet
        assert packet["md"] == "bench" and packet["msg"] is None, packet
        for key in ("vb", "ib", "vs", "is", "tp", "w", "az", "rssi", "snr"):
            assert packet[key] is None, packet
        report["telemetry"].append(packet)
        return packet

    def check_reference(packet):
        expected = reference_unix + int(time.monotonic() - reference_started)
        assert type(packet["ts"]) is int and abs(packet["ts"] - expected) <= 1, packet

    def wait_aircraft(count):
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            packet = sample()
            check_reference(packet)
            if packet["ac"] == count:
                return packet
            time.sleep(0.05)
        raise AssertionError(f"TM não observou ac={count}")

    try:
        session = SessionLog(artifacts / "logs")
        store = DeliveryStore(artifacts / "state" / "deliveries.jsonl")
        stream = open_serial(args.port)
        link = Link(stream, "wired", session, store)
        link.add_listener(record)
        link.start()

        initial = sample()
        assert initial["ts"] is None and initial["ac"] is None, initial
        for value in (None, True, -1, 1.5, 4294967296, "1760000000"):
            command("set_time", {"unix": value}, expected="invalid_param")
        command("set_time", expected="invalid_param")
        command("set_time", {"unix": 1760000000, "extra": 1}, expected="invalid_param")
        assert sample("get_status")["ts"] is None

        command("set_time", {"unix": 0})
        epoch = sample()
        assert type(epoch["ts"]) is int and 0 <= epoch["ts"] <= 1, epoch
        reference_unix = int(time.time())
        reference_started = time.monotonic()
        command("set_time", {"unix": reference_unix})
        check_reference(sample())
        if args.bench:
            wait_aircraft(7)

        # Mesmos 50 pedidos alternados do roteiro temporal histórico.
        for index in range(50):
            name = ("ping", "get_status", "request_tm")[index % 3]
            if name == "ping":
                command(name)
            else:
                packet = sample(name)
                check_reference(packet)
                if not args.bench:
                    assert packet["ac"] is None, packet
            time.sleep(0.02)

        if args.bench:
            wait_aircraft(0)
            # São as funções reais utilizadas pelos comandos logs list/get.
            files = list_files(link, to="wired")
            report["listing"] = files
            closed = [entry for entry in files if not entry["open"]]
            assert len(closed) >= 3, files
            for entry in closed[:3]:
                path = download_file(link, entry["name"], artifacts / "downloads", to="wired")
                data = path.read_bytes()
                assert len(data) == entry["size"] and not path.with_name(path.name + ".part").exists(), path
                assert data[:4] == b"FCD1" and len(data) in (36, 68), data
                assert int.from_bytes(data[6:8], "little") == len(data), data
                assert int.from_bytes(data[-4:], "little") == zlib.crc32(data[:-4]), data
                interpreted = subprocess.run([str(args.reader.resolve()), str(path)],
                                             capture_output=True, text=True, timeout=10, check=True)
                records = [json.loads(line) for line in interpreted.stdout.splitlines()]
                assert len(records) == 1 and records[0]["type"] in ("delivery_data", "delivery_confirm"), records
                output = path.with_suffix(".jsonl")
                output.write_text(interpreted.stdout)
                report["files"].append({"name": path.name, "bytes": len(data), "crc": "ok",
                                        "sha256": hashlib.sha256(data).hexdigest(), "records": records,
                                        "reader_diagnostic": interpreted.stderr})
            try:
                download_file(link, "missing.bin", artifacts / "downloads", to="wired")
            except LogError as error:
                assert error.code == "not_found", error
                report["missing_file"] = error.code
            else:
                raise AssertionError("arquivo ausente não retornou not_found")

            station = link.snapshot()
            assert station["deliveries"]["new"] >= 7, station
            assert station["deliveries"]["confirmations"] >= station["deliveries"]["new"], station
            assert station["deliveries"]["confirm_errors"] == 0, station
            assert len(station["aircraft"]) == 7, station
            for aircraft in station["aircraft"]:
                assert all(aircraft[key] is None for key in ("ts", "vel", "hdg", "cnt")), aircraft
            report["station"] = station
        else:
            try:
                list_files(link, to="wired")
            except LogError as error:
                assert error.code == "no_sd", error
                report["storage"] = error.code
            else:
                raise AssertionError("firmware normal sem SD não retornou no_sd")

        with condition:
            captured = list(events)
        received = [event for event in captured if event.get("dir") == "rx" and "pkt" in event]
        for event in received:
            packet = event["pkt"]
            assert len(json.dumps(packet, separators=(",", ":")).encode()) <= 255, packet
            if packet["t"] == "ack":
                assert type(packet["q"]) is int and 0 <= packet["q"] <= 200, packet
        counters = [event["pkt"]["n"] for event in received]
        assert all(type(value) is int and 0 <= value <= 65535 for value in counters), counters
        assert all(right == (left + 1) % 65536 for left, right in zip(counters, counters[1:])), counters
        lines = [event["text"] for event in captured if "text" in event]
        assert not any("FAIL" in line for line in lines), lines
        assert not any(event.get("kind") == "error" for event in captured), captured
        diagnostics = [dict((key, int(value)) for key, value in re.findall(r"(\w+)=(\d+)", line))
                       for line in lines if line.startswith("[FLOW] received=")]
        assert diagnostics and max(item["poll_gap_ms"] for item in diagnostics) <= 10, diagnostics
        for item in diagnostics:
            for key in ("stack_rx", "stack_uart", "stack_sd") if args.bench else ("stack_rx", "stack_uart"):
                assert item[key] > 0, item
        report["diagnostics"] = diagnostics
        report["received_packets"] = len(received)
        report["session_log"] = str(session.path)
        report["passed"] = True
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        if link is not None:
            link.close()
        else:
            if stream is not None:
                stream.close()
            if store is not None:
                store.close()
        if session is not None:
            session.close()
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"ObcStationCheck passed; {args.report}")


if __name__ == "__main__":
    main()
