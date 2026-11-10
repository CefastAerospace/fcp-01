#!/usr/bin/env python3
"""Verifica INT-006 após boot novo; --bench usa int006-check, com ADS-B/SD simulados.

Requer pyserial. Não substitui o roteiro da versão INT-004 ou ensaios físicos.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial
from fcp_gs.protocol import COMMANDS, encode_tc, parse_line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--bench", action="store_true")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    destination = args.report or Path("/tmp/fcp01-int006-" + ("bench" if args.bench else "obc") + ".json")
    report = {"passed": False, "bench": args.bench, "port": args.port, "exchanges": [], "lines": []}
    stream = None
    seq = int(time.time() * 1000) & 0xFFFF
    last_counter = None

    def read_packet():
        nonlocal last_counter
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return None
        report["lines"].append(line)
        assert "[FLOW-CHECK] FAIL:" not in line, line
        parsed = parse_line(line)
        if parsed.kind != "packet":
            return None
        packet = parsed.packet
        assert len(line.encode()) <= 255, packet
        counter = packet["n"]
        assert type(counter) is int and 0 <= counter <= 65535, packet
        if last_counter is not None:
            assert counter == (last_counter + 1) % 65536, packet
        last_counter = counter
        return packet

    def collect(seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            packet = read_packet()
            assert packet is None, packet

    def command(name, status="ok", params=None, tm=False):
        nonlocal seq
        seq = (seq + 1) % 65536
        raw = encode_tc(seq, name, params)
        started = time.monotonic()
        stream.write(raw)
        stream.flush()
        ack = telemetry = None
        packets = []
        while time.monotonic() - started < 2:
            packet = read_packet()
            if packet is None:
                continue
            packets.append(packet)
            if packet["t"] == "ack":
                assert ack is None, packets
                ack = packet
                assert packet["seq"] == seq and packet["cmd"] == name and packet["st"] == status, packet
                if "q" in packet:
                    assert 0 <= packet["q"] <= 200, packet
                if status in ("ok", "invalid_cmd"):
                    assert "q" in packet, packet
            elif packet["t"] == "tm":
                assert tm and telemetry is None, packets
                telemetry = packet
                assert packet["md"] == "bench" and packet["msg"] is None, packet
                for key in ("vb", "ib", "vs", "is", "tp", "w", "az", "rssi", "snr"):
                    assert packet[key] is None, packet
            else:
                raise AssertionError(packet)
            if ack is not None and (not tm or telemetry is not None):
                break
        elapsed = (time.monotonic() - started) * 1000
        report["exchanges"].append({"input": raw.decode(), "packets": packets, "response_ms": round(elapsed, 3)})
        assert ack is not None and (telemetry is not None) == tm and elapsed <= 2000, packets
        return telemetry

    try:
        stream = open_serial(args.port)
        initial = command("request_tm", tm=True)
        assert initial["ts"] is None and initial["ac"] is None, initial
        for value in (None, True, -1, 1.5, 4294967296, "1760000000"):
            command("set_time", "invalid_param", {"unix": value})
        command("set_time", "invalid_param")
        command("set_time", "invalid_param", {"unix": 1760000000, "extra": 1})
        assert command("get_status", tm=True)["ts"] is None
        for name in COMMANDS:
            if name not in ("ping", "get_status", "request_tm", "set_time"):
                command(name, "invalid_cmd")

        unix_s = int(time.time())
        reference_started = time.monotonic()
        command("set_time", params={"unix": unix_s})
        current = command("request_tm", tm=True)
        assert type(current["ts"]) is int and abs(current["ts"] - unix_s) <= 1, current
        if args.bench:
            collect(1.05)
            for _ in range(8):
                current = command("request_tm", tm=True)
                if current["ac"] == 7:
                    break
                collect(0.05)
            assert current["ac"] == 7 and current["ts"] is not None, current
        for index in range(50):
            name = ("ping", "get_status", "request_tm")[index % 3]
            current = command(name, tm=name != "ping")
            if current is not None:
                expected = unix_s + int(time.monotonic() - reference_started)
                assert type(current["ts"]) is int and abs(current["ts"] - expected) <= 1, current
                if not args.bench:
                    assert current["ac"] is None, current
            time.sleep(0.02)
        collect(1.2)
        if args.bench:
            assert command("request_tm", tm=True)["ac"] == 0
            collect(1.2)
        diagnostics = [dict(re.findall(r"(\w+)=(\d+)", line)) for line in report["lines"] if line.startswith("[FLOW] received=")]
        assert diagnostics, report["lines"]
        latest = {key: int(value) for key, value in diagnostics[-1].items()}
        report["diagnostics"] = latest
        assert latest["poll_gap_ms"] <= 10 and latest["stack_rx"] > 0, latest
        if args.bench:
            assert latest["SD"] == 1 and latest["written"] > 0 and latest["dropped"] > 0, latest
            assert latest["invalid"] >= 1 and latest["duplicates"] >= 1 and latest["received"] >= 24, latest
            assert any("v3 ts_available=1" in line and "ac_available=1 ac=7 msg_available=0 stable=1" in line for line in report["lines"]), report["lines"]
        else:
            assert latest["SD"] == 0 and latest["failures"] >= 1 and latest["dropped"] > 0, latest
        # Época Unix zero é valor válido, diferente de falta de referência.
        command("set_time", params={"unix": 0})
        epoch = command("request_tm", tm=True)
        assert type(epoch["ts"]) is int and epoch["ts"] <= 1, epoch
        report["passed"] = True
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        if stream is not None:
            stream.close()
        destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"INT-006 {'bancada' if args.bench else 'OBC'}: {len(report['exchanges'])} cenários aprovados; {destination}")


if __name__ == "__main__":
    main()
