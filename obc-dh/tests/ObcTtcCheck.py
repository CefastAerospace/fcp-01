#!/usr/bin/env python3
"""Verifica INT-004 por USB no OBC de bancada, sem TC RF concorrente.

Reutiliza abertura serial e protocolo da estação terrena. Requer pyserial,
também disponível no Python do PlatformIO. Não substitui TES-008.
"""

import argparse
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial
from fcp_gs.protocol import COMMANDS, MAX_FRAME_LEN, encode_tc, parse_line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--report", type=Path, default=Path("/tmp/fcp01-int004-usb.json"))
    args = parser.parse_args()
    report = {"port": args.port, "passed": False, "exchanges": [], "lines": []}
    sequence = int(time.time() * 1000) & 0xFFFF
    last_counter = None
    stream = None

    def exchange(raw, command=None, status=None, telemetry=False, seq=None):
        nonlocal last_counter
        started = time.monotonic()
        stream.write(raw)
        stream.flush()
        packets = []
        ack = None
        tm = None
        settled = None
        while time.monotonic() - started < 2:
            line = stream.readline().decode("utf-8", errors="replace").rstrip()
            if line:
                report["lines"].append(line)
                parsed = parse_line(line)
                if parsed.kind == "packet":
                    packet = parsed.packet
                    assert len(line.encode()) <= MAX_FRAME_LEN, packet
                    counter = packet["n"]
                    assert type(counter) is int and 0 <= counter <= 65535, packet
                    if last_counter is not None:
                        assert counter == (last_counter + 1) % 65536, packet
                    last_counter = counter
                    packets.append(packet)
                    if packet["t"] == "ack":
                        assert ack is None, packets
                        assert packet["seq"] == seq and packet["cmd"] == command, packet
                        assert packet["st"] == status, packet
                        if "q" in packet:
                            assert 0 <= packet["q"] <= 200, packet
                        if status in ("ok", "invalid_cmd"):
                            assert "q" in packet, packet
                        ack = packet
                    elif packet["t"] == "tm":
                        assert telemetry and tm is None, packet
                        tm = packet
                    else:
                        raise AssertionError(packet)
            complete = (ack is not None and (not telemetry or tm is not None))
            if complete and settled is None:
                settled = time.monotonic()
            if settled is not None and time.monotonic() - settled >= 0.15:
                break
            if status is None and time.monotonic() - started >= 0.3:
                break
        elapsed_ms = ((settled or time.monotonic()) - started) * 1000
        report["exchanges"].append({"input": raw.decode(errors="replace"),
            "packets": packets, "response_ms": round(elapsed_ms, 3)})
        if status is None:
            assert not packets, packets
        else:
            assert ack is not None and elapsed_ms <= 2000, packets
            assert (tm is not None) == telemetry, packets
        if tm is not None:
            assert tm["md"] == "bench", tm
            for key in ("ts", "vb", "ib", "vs", "is", "tp", "w", "az",
                        "ac", "msg", "rssi", "snr"):
                assert key in tm and tm[key] is None, tm
            for key in ("rx", "err", "rst"):
                assert type(tm[key]) is int and 0 <= tm[key] <= 65535, tm
            body = {key: value for key, value in tm.items() if key != "n"}
            assert len(json.dumps(body, separators=(",", ":")).encode()) <= 245, tm
        return raw, seq

    def command(name, status="ok", telemetry=False, params=None):
        nonlocal sequence
        sequence = (sequence + 1) % 65536
        return exchange(encode_tc(sequence, name, params), name, status, telemetry, sequence)

    try:
        stream = open_serial(args.port)
        # Verifica comandos suportados, inclusive objeto args vazio explícito.
        command("ping")
        command("get_status", telemetry=True)
        duplicate, duplicate_seq = command("request_tm", telemetry=True)
        exchange(duplicate, "request_tm", "ok", seq=duplicate_seq)
        for name in ("ping", "get_status", "request_tm"):
            command(name, "invalid_param", params={"unexpected": 1})
            sequence = (sequence + 1) % 65536
            raw = json.dumps({"t": "tc", "seq": sequence, "cmd": name, "args": {}})
            exchange((raw + "\n").encode(), name, "ok", name != "ping", sequence)
        for name in COMMANDS:
            if name not in ("ping", "get_status", "request_tm"):
                command(name, "invalid_cmd", params={"unix": 1760000000} if name == "set_time" else None)
        command("unknown_int004", "unknown_cmd")
        # Envelopes ilegíveis/longos não devem impedir o próximo TC válido.
        exchange(b'{"t":"tc","cmd":"ping"}\n')
        exchange(b'{"t":"tc",\n')
        exchange(b'{"padding":"' + b'x' * 260 + b'"}\n')
        command("ping")
        # Amostra de latência do caminho central, sem habilitar outros comandos.
        for index in range(50):
            name = ("ping", "get_status", "request_tm")[index % 3]
            command(name, telemetry=name != "ping")
        report["passed"] = True
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        if stream is not None:
            stream.close()
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"INT-004 USB: {len(report['exchanges'])} cenários aprovados; relatório: {args.report}")


if __name__ == "__main__":
    main()
