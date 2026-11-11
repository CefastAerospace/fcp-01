#!/usr/bin/env python3
"""Verifica int006-capacity-check ou int006-memory-check por USB.

Somente Payload/SD simulados. Não comprova RF, cartão ou retomada após reset.
Conserva os roteiros anteriores e recusa substituir relatório existente.
"""
import argparse
import base64
import json
from pathlib import Path
import re
import struct
import sys
import time
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial
from fcp_gs.protocol import encode_confirmation, encode_tc, parse_line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--case", choices=("capacity", "memory"), required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    output = args.report.open("x")
    report = {"passed": False, "case": args.case, "lines": [], "sent": [], "packets": [],
              "payload": [], "acks": [], "files": []}
    stream = None
    sequence = int(time.time() * 1000) & 0xFFFF
    started, acknowledgments, responses, delivered = {}, {}, {}, {}
    payload, flow, delivery_statistics, memory = [], [], [], []
    last_counter = None
    auto_confirm = False
    closed_reads_during_memory = 0
    origin = time.monotonic()

    def fields(line):
        return {key: int(value) if value.isdigit() else value
                for key, value in re.findall(r"(\w+)=([\w]+)", line)}

    def send(command, parameters=None):
        nonlocal sequence
        sequence = (sequence + 1) & 0xFFFF
        started[sequence] = time.monotonic()
        report["sent"].append({"seq": sequence, "cmd": command, "args": parameters})
        stream.write(encode_tc(sequence, command, parameters))
        stream.flush()
        return sequence

    def confirm(key):
        report["sent"].append({"cf": list(key)})
        stream.write(encode_confirmation(key))
        stream.flush()

    def receive():
        nonlocal last_counter
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return
        report["lines"].append(line)
        assert "FAIL" not in line, line
        if line.startswith("[LIMITS-PAYLOAD]"):
            event = fields(line)
            assert event["case"] == len(payload) + 1, event
            assert event["valid"] == 1, event
            payload.append(event)
            report["payload"].append(event)
        elif line.startswith("[FLOW] received="):
            flow.append(fields(line))
        elif line.startswith("[DELIVERY] occupied="):
            delivery_statistics.append(fields(line))
        elif line.startswith("[FLOW-MEMORY]"):
            memory.append(fields(line))
        parsed = parse_line(line)
        if parsed.kind != "packet":
            return
        packet = parsed.packet
        now = time.monotonic()
        report["packets"].append({"at_ms": round((now - origin) * 1000, 3), "packet": packet})
        assert len(line.encode()) <= 255, packet
        counter = packet["n"]
        assert type(counter) is int and 0 <= counter <= 65535, packet
        if last_counter is not None:
            assert counter == (last_counter + 1) % 65536, (last_counter, packet)
        last_counter = counter
        if packet["t"] == "ack":
            seq = packet["seq"]
            assert seq in started and seq not in acknowledgments, packet
            elapsed = (now - started[seq]) * 1000
            assert packet["st"] == "ok" and packet["q"] <= 200 and elapsed <= 2000, (packet, elapsed)
            acknowledgments[seq] = packet
            report["acks"].append({"packet": packet, "elapsed_ms": round(elapsed, 3)})
        elif packet["t"] == "lg":
            seq = packet["rq"]
            assert seq in started and seq not in responses and now - started[seq] <= 2, packet
            responses[seq] = packet
        elif packet["t"] == "ac":
            key = tuple(packet["id"])
            content = {name: value for name, value in packet.items() if name != "n"}
            if key in delivered:
                assert delivered[key] == content, (delivered[key], content)
            else:
                if delivered:
                    previous = next(reversed(delivered))
                    assert key == (previous[0], previous[1] + 1), (previous, key)
                else:
                    assert key[0] > 0 and key[1] == 1, key
                delivered[key] = content
            assert all(packet[name] is None for name in ("ts", "vel", "hdg", "cnt")), packet
            if args.case == "capacity":
                expected_altitude = 99 + key[1] if key[1] <= 32 else 140
                assert key[1] <= 33 and packet["icao"] == "000001", packet
                assert packet["lat"] == packet["lon"] == 0 and packet["alt"] == expected_altitude, packet
                if auto_confirm:
                    confirm(key)
            else:
                assert key[1] == 1 and packet["alt"] == 100, packet

    def until(predicate, seconds=2):
        end = time.monotonic() + seconds
        while not predicate() and time.monotonic() < end:
            receive()
        assert predicate(), f"prazo de {seconds}s esgotado"

    def wait(seq, log=False):
        until(lambda: seq in acknowledgments and (not log or seq in responses),
              max(0, started[seq] + 2 - time.monotonic()))
        return responses[seq] if log else acknowledgments[seq]

    def work():
        wait(send("ping"))
        wait(send("request_tm"))
        until_time = time.monotonic() + 0.1
        while time.monotonic() < until_time:
            receive()

    def request(parameters):
        for _ in range(12):
            seq = send("request_log", {**parameters, "to": "wired"})
            ping = send("ping")
            wait(ping)
            response = wait(seq, log=True)
            if response.get("err") != "busy":
                return response
            end = time.monotonic() + 0.1
            while time.monotonic() < end:
                receive()
        raise AssertionError("request_log permanece busy")

    try:
        stream = open_serial(args.port)
        wait(send("ping"))
        assert any(f"[LIMITS] scenario={args.case} " in line for line in report["lines"]), "bancada incorreta"
        wait(send("set_time", {"unix": int(time.time())}))
        deadline = time.monotonic() + (180 if args.case == "capacity" else 30)
        if args.case == "capacity":
            while len(payload) < 40 and time.monotonic() < deadline:
                work()
            assert len(payload) == 40, payload
            assert all(row["status"] == "READY" and row["id"] == index
                       for index, row in enumerate(payload, 1)), payload
            saturated = payload[39]
            assert (saturated["accepted"], saturated["lost"], saturated["occupied"], saturated["peak"]) == (32, 8, 32, 32), saturated
            until(lambda: bool(delivered))
            auto_confirm = True
            confirm(next(iter(delivered)))
            while time.monotonic() < deadline:
                if delivery_statistics and delivery_statistics[-1]["confirmed"] == 33 and delivery_statistics[-1]["occupied"] == 0:
                    break
                work()
            assert len(payload) == 41 and payload[-1]["id"] == 41 and payload[-1]["status"] == "READY", payload
            assert len(delivered) == 33, len(delivered)
            final = delivery_statistics[-1]
            assert (final["accepted"], final["lost"], final["occupied"], final["confirmed"], final["peak"]) == (33, 8, 0, 33, 32), final
            assert final["storage_failures"] == final["evicted"] == 0, final
        else:
            while len(payload) < 5 and time.monotonic() < deadline:
                work()
                if delivery_statistics and delivery_statistics[-1]["accepted"]:
                    # Leitura de um arquivo fechado durante a sequência de memória.
                    name = f"boot_{next(iter(delivered))[0]:016x}_00000000.bin" if delivered else "missing.bin"
                    response = request({"op": "read", "name": name, "off": 0, "len": 90})
                    assert response.get("err") in (None, "not_found"), response
                    if "err" not in response:
                        data = base64.b64decode(response["d"], validate=True)
                        assert len(data) == 68 and data[:4] == b"FCD1", response
                        assert int.from_bytes(data[-4:], "little") == zlib.crc32(data[:-4]), response
                        closed_reads_during_memory += 1
            assert len(payload) == 5, payload
            assert [row["status"] for row in payload[:2]] == ["READY", "READY"], payload
            assert [(row["id"], row["count"]) for row in payload[:2]] == [(1, 1), (2, 1)], payload
            dense = payload[2]
            assert dense["status"] in ("READY", "NO_MEMORY"), dense
            if dense["status"] == "READY":
                assert (dense["id"], dense["count"], dense["extras"], dense["identification"], dense["tick"]) == (3, 20, 32, 16, 40), dense
            else:
                assert (dense["id"], dense["count"], dense["crc"]) == tuple(payload[1][key] for key in ("id", "count", "crc")), dense
            exhausted = payload[3]
            assert exhausted["status"] == "NO_MEMORY", exhausted
            assert (exhausted["id"], exhausted["count"], exhausted["crc"]) == tuple(dense[key] for key in ("id", "count", "crc")), exhausted
            assert (payload[4]["status"], payload[4]["id"], payload[4]["count"]) == ("READY", 5, 1), payload[4]
            assert all(row["lost"] == 0 for row in payload), payload
            report["dense_result"] = dense["status"]
            assert closed_reads_during_memory > 0
            report["closed_reads_during_memory"] = closed_reads_during_memory

        # Extração e integridade dos DATA efetivamente fechados.
        files, after = [], ""
        while True:
            response = request({"op": "list", "after": after})
            assert "err" not in response, response
            for name, size, opened in response["ls"]:
                assert name > after and not opened, response
                after = name
                files.append((name, size))
            if response["end"]:
                break
            assert response["ls"], response
        data_files = [(name, size) for name, size in files if size == 68]
        assert data_files, files
        selected = [data_files[0], data_files[-1]] if args.case == "capacity" else [data_files[0]]
        for name, size in selected:
            response = request({"op": "read", "name": name, "off": 0, "len": 90})
            assert "err" not in response and response["tot"] == size and response["off"] == 0, response
            data = base64.b64decode(response["d"], validate=True)
            assert len(data) == size and data[:4] == b"FCD1", data.hex()
            assert int.from_bytes(data[-4:], "little") == zlib.crc32(data[:-4]), data.hex()
            boot, identity = struct.unpack_from("<QQ", data, 8)
            assert (boot, identity) in delivered, (boot, identity)
            if args.case == "capacity":
                assert identity == (1 if name == selected[0][0] else 33), (name, identity)
            # Layout FCD1 independente: ICAO em 32 e altitude double em 56.
            assert int.from_bytes(data[32:36], "little") == 1, data.hex()
            assert struct.unpack_from("<d", data, 56)[0] == delivered[(boot, identity)]["alt"], data.hex()
            report["files"].append({"name": name, "bytes": size, "id": [boot, identity], "crc": "ok", "hex": data.hex()})
        assert len(acknowledgments) == len(started)
        assert flow and max(row["poll_gap_ms"] for row in flow) <= 10, flow
        assert all(row[key] > 0 for row in flow for key in ("stack_rx", "stack_sd", "stack_uart")), flow
        assert all(row["SD"] == 1 and row["failures"] == 0 for row in flow), flow
        assert memory and all(row["parser_peak_bytes"] <= 98304 for row in memory), memory
        report["deliveries"] = len(delivered)
        report["passed"] = True
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
        raise
    finally:
        if stream is not None:
            stream.close()
        with output:
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(f"ObcLimitsCheck {args.case} passed; {args.report}")


if __name__ == "__main__":
    main()
