#!/usr/bin/env python3
"""Verifica int008-storage-check em um ESP32/USB, com SD e Payload simulados."""
import argparse
import json
from pathlib import Path
import re
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import open_serial
from fcp_gs.protocol import encode_confirmation, encode_tc, parse_line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    report = {"passed": False, "lines": [], "exchanges": [], "storage": [], "checks": []}
    stream = None
    sequence = int(time.time() * 1000) & 0xFFFF
    started, acknowledgments, deliveries, log_requests = {}, {}, {}, set()
    mount_times, fault_times, normal_times, references = [], [], [], []
    expected_time = None
    last_counter = None
    exhausted_at = None
    names = []
    boot = None

    def send(command, parameters=None):
        nonlocal sequence
        sequence = (sequence + 1) & 0xFFFF
        started[sequence] = time.monotonic()
        stream.write(encode_tc(sequence, command, parameters))
        stream.flush()
        return sequence

    def receive():
        nonlocal last_counter, expected_time, exhausted_at, boot
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return
        report["lines"].append(line)
        assert "FAIL" not in line and "Guru Meditation" not in line, line
        values = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)}
        if line.startswith("[STORAGE-CHECK] fault="):
            fault_times.append(values["monotonic_ms"])
        if line.startswith("[STORAGE-CHECK] mount_begin="):
            mount_times.append(values["monotonic_ms"])
            expected_time = 1700000000 + len(mount_times)
            send("set_time", {"unix": expected_time})
        if line.startswith("[STORAGE-CHECK] reference_unix="):
            references.append(values["reference_unix"])
        if line.startswith("[STORAGE-CHECK] normal_write="):
            assert expected_time in references, (expected_time, references)
            normal_times.append(values["monotonic_ms"])
        if line.startswith("[STORAGE] "):
            report["storage"].append(values)
            if values["state"] == 3 and exhausted_at is None:
                exhausted_at = time.monotonic()
        if line.startswith("[FLOW] received="):
            assert values["poll_gap_ms"] <= 10, values
            assert all(values[key] > 0 for key in ("stack_rx", "stack_sd", "stack_uart")), values
        if line.startswith("[FLOW-CHECK] Arquivo simulado:"):
            name = line.rsplit(" ", 1)[1]
            assert name not in names, name
            names.append(name)
            current_boot = name.split("_")[1]
            if boot is None:
                boot = current_boot
            assert current_boot == boot, (boot, current_boot)
        received = parse_line(line)
        if received.kind != "packet":
            return
        packet = received.packet
        assert len(line.encode()) <= 255, packet
        counter = packet["n"]
        if last_counter is not None:
            assert counter == (last_counter + 1) % 65536, packet
        last_counter = counter
        if packet["t"] == "ack":
            seq = packet["seq"]
            assert seq in started and seq not in acknowledgments, packet
            elapsed_ms = (time.monotonic() - started[seq]) * 1000
            assert elapsed_ms <= 2000 and packet["q"] <= 200 and packet["st"] == "ok", packet
            acknowledgments[seq] = packet
            report["exchanges"].append({"ack": packet, "response_ms": round(elapsed_ms, 3)})
        elif packet["t"] == "ac":
            delivery_id = tuple(packet["id"])
            content = {key: value for key, value in packet.items() if key != "n"}
            assert deliveries.setdefault(delivery_id, content) == content
            stream.write(encode_confirmation(delivery_id))
            stream.flush()
        elif packet["t"] == "lg":
            assert packet["rq"] in log_requests, packet

    with args.report.open("x", encoding="utf-8") as output:
        try:
            stream = open_serial(args.port)
            send("set_time", {"unix": 1600000000})
            deadline = time.monotonic() + 100
            next_command = 0
            next_list = 0
            while time.monotonic() < deadline:
                now = time.monotonic()
                if now >= next_command:
                    send("ping")
                    send("request_tm")
                    next_command = now + 0.25
                if now >= next_list:
                    log_requests.add(send("request_log", {"op": "list"}))
                    next_list = now + 2
                receive()
                overdue = [seq for seq, at in started.items() if seq not in acknowledgments and time.monotonic() - at > 2]
                assert not overdue, overdue
                if exhausted_at is not None and time.monotonic() - exhausted_at >= 6:
                    break
            assert exhausted_at is not None, "limite de recuperacao nao esgotou"
            # Drena respostas já enviadas sem criar novos pedidos.
            until = time.monotonic() + 2
            while len(acknowledgments) < len(started) and time.monotonic() < until:
                receive()
            assert len(acknowledgments) == len(started)
            assert len(deliveries) == 7, deliveries
            final = report["storage"][-1]
            assert final["state"] == 3 and final["attempt"] == 3, final
            assert final["episodes"] == 3 and final["attempts"] == 6, final
            assert final["recovered"] == 2 and final["exhausted"] == 1, final
            assert len(mount_times) == 6 and len(fault_times) == 3 and len(normal_times) == 2
            assert mount_times[0] - fault_times[0] >= 5000
            assert mount_times[1] - mount_times[0] >= 5300
            assert mount_times[2] - fault_times[1] >= 5000
            assert mount_times[3] - fault_times[2] >= 5000
            assert all(mount_times[index + 1] - mount_times[index] >= 5300 for index in (3, 4))
            assert len(set(names)) == len(names) and boot is not None
            report["mount_times_ms"] = mount_times
            report["fault_times_ms"] = fault_times
            report["normal_times_ms"] = normal_times
            report["boot"] = boot
            report["checks"] = [
                "tres episodios, seis tentativas, duas retomadas e um esgotamento",
                "intervalos >=5s apos falha/retorno e nenhuma quarta tentativa",
                "set_time atendido durante montagem; referencia atual antes dos dados",
                "sete entregas confirmadas; nomes exclusivos e mesmo boot",
                "ping/TM/request_log ativos; q<=200ms, ACK<=2s, UART<=10ms e pilhas positivas",
            ]
            report["passed"] = True
        except Exception as error:
            report["error"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            if stream is not None:
                stream.close()
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
    print(f"ObcStorageRecoveryBenchCheck passed; {args.report}")


if __name__ == "__main__":
    main()
