#!/usr/bin/env python3
"""Verifica o OBC atual por USB; --bench usa int006-ttc-check.

O solo deste roteiro é simulado em RAM. Não valida a persistência da estação
TT&C, RF, cartão físico ou UART entre placas. Preserva os roteiros históricos.
"""
import argparse
import base64
import json
from pathlib import Path
import re
import sys
import time
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial
from fcp_gs.protocol import encode_confirmation, encode_tc, parse_line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--bench", action="store_true")
    parser.add_argument("--report", type=Path, default=Path("/tmp/fcp01-obc-downlink.json"))
    args = parser.parse_args()
    report = {"passed": False, "bench": args.bench, "lines": [], "exchanges": [], "files": []}
    stream = None
    sequence = int(time.time() * 1000) & 0xFFFF
    last_counter = None
    acknowledgments, responses, started, delivered = {}, {}, {}, {}
    first_delivery = None
    retried = False

    def send(command, parameters=None):
        nonlocal sequence
        sequence = (sequence + 1) & 0xFFFF
        started[sequence] = time.monotonic()
        stream.write(encode_tc(sequence, command, parameters))
        stream.flush()
        return sequence

    def receive():
        nonlocal last_counter, first_delivery, retried
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return
        report["lines"].append(line)
        assert "FAIL" not in line, line
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
            assert packet["q"] <= 200, packet
            acknowledgments[packet["seq"]] = packet
            elapsed = time.monotonic() - started[packet["seq"]]
            assert elapsed <= 2, (packet, elapsed)
            report["exchanges"].append({"ack": packet, "response_ms": round(elapsed * 1000, 3)})
        elif packet["t"] == "lg":
            responses[packet["rq"]] = packet
        elif packet["t"] == "ac":
            assert args.bench, packet
            key = tuple(packet["id"])
            content = {name: value for name, value in packet.items() if name != "n"}
            original = delivered.setdefault(key, content)
            assert original == content, (original, content)
            assert all(packet[name] is None for name in ("ts", "vel", "hdg", "cnt")), packet
            if first_delivery is None:
                first_delivery = key
                return  # Perda deliberada do primeiro cf.
            if key == first_delivery:
                retried = True
            stream.write(encode_confirmation(key))
            stream.flush()

    def wait(seq, log=False, status="ok"):
        deadline = started[seq] + 2
        while time.monotonic() < deadline:
            if seq in acknowledgments and (not log or seq in responses):
                break
            receive()
        assert seq in acknowledgments, seq
        assert acknowledgments[seq]["st"] == status, acknowledgments[seq]
        if log:
            assert seq in responses, seq
            return responses.pop(seq)

    def request(parameters):
        for _ in range(12):
            seq = send("request_log", parameters)
            ping = send("ping")  # Atendimento TC precisa continuar durante I/O.
            wait(ping)
            response = wait(seq, log=True)
            if response.get("err") != "busy":
                return response
            time.sleep(0.1)
        raise AssertionError("request_log permanece busy")

    try:
        stream = open_serial(args.port)
        wait(send("ping"))
        wait(send("request_log", {"op": "read", "name": "../a", "off": 0, "len": 90}), status="invalid_param")
        wait(send("request_log", {"op": "read", "name": "a", "off": 0, "len": 91}), status="invalid_param")
        wait(send("set_time", {"unix": int(time.time())}))
        if args.bench:
            deadline = time.monotonic() + 25
            while not retried and time.monotonic() < deadline:
                wait(send("ping"))
                until = time.monotonic() + 0.5
                while time.monotonic() < until:
                    receive()
            assert retried and delivered, delivered
        files, after = [], ""
        while True:
            parameters = {"op": "list", "to": "wired"}
            if after:
                parameters["after"] = after
            response = request(parameters)
            if not args.bench and response.get("err") == "no_sd":
                report["storage"] = "no_sd"
                break
            assert "err" not in response, response
            for name, size, opened in response["ls"]:
                assert name > after and type(size) is int and opened in (0, 1), response
                after = name
                files.append((name, size, opened))
            if response["end"]:
                break
            assert response["ls"], response
        if args.bench:
            assert files, files
            for name, size, opened in files[:3]:
                assert not opened
                content = bytearray()
                while True:
                    response = request({"op": "read", "name": name, "off": len(content), "len": 90, "to": "wired"})
                    assert "err" not in response and response["name"] == name, response
                    assert response["off"] == len(content) and response["tot"] == size, response
                    block = base64.b64decode(response["d"], validate=True)
                    assert len(block) <= 90 and len(content) + len(block) <= size
                    if not block:
                        assert len(content) == size
                        break
                    content.extend(block)
                assert content[:4] == b"FCD1" and len(content) in (36, 68), content
                assert int.from_bytes(content[-4:], "little") == zlib.crc32(content[:-4]), content
                report["files"].append({"name": name, "bytes": len(content), "crc": "ok"})
            missing = request({"op": "read", "name": "missing.bin", "off": 0, "len": 90})
            assert missing["err"] == "not_found", missing
            name, size, _ = files[0]
            beyond = request({"op": "read", "name": name, "off": size + 1, "len": 90})
            assert beyond["err"] == "range", beyond
            assert len(delivered) > 1, delivered
            diagnostics = [dict(re.findall(r"(\w+)=(\d+)", line)) for line in report["lines"]
                           if line.startswith("[FLOW] received=")]
            assert diagnostics and max(int(item["poll_gap_ms"]) for item in diagnostics) <= 10, diagnostics
            assert all(int(item[key]) > 0 for item in diagnostics for key in ("stack_rx", "stack_sd", "stack_uart")), diagnostics
        report["deliveries"] = len(delivered)
        report["retried"] = retried
        report["passed"] = True
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        if stream is not None:
            stream.close()
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"ObcDownlinkCheck passed; {args.report}")


if __name__ == "__main__":
    main()
