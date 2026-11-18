#!/usr/bin/env python3
"""Ensaio USB adicional para int006-stress-check; requer apenas ESP32.

Payload e SD simulados. Conserva testes anteriores e não comprova RF,
durabilidade física, estação persistente ou fila TT&C completamente cheia.
"""
import argparse
import base64
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
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    report = {"passed": False, "lines": [], "packets": [], "sent": [], "checks": []}
    # Uma execução nunca substitui evidência anterior, inclusive se falhar.
    output = args.report.open("x")
    stream = None
    sequence = int(time.time() * 1000) & 0xFFFF
    started, acknowledgments, responses, deliveries = {}, {}, {}, {}
    observations = []
    last_counter = None
    origin = time.monotonic()

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
            assert packet["st"] == "ok" and packet["q"] <= 200, packet
            elapsed = (now - started[seq]) * 1000
            assert elapsed <= 2000, (packet, elapsed)
            acknowledgments[seq] = packet
            report.setdefault("ack_ms", []).append(round(elapsed, 3))
        elif packet["t"] == "lg":
            seq = packet["rq"]
            assert seq in started and seq not in responses, ("lg repetido/tardio", packet)
            assert now - started[seq] <= 2, packet
            responses[seq] = packet
        elif packet["t"] == "ac":
            key = tuple(packet["id"])
            content = {name: value for name, value in packet.items() if name != "n"}
            if key in deliveries:
                assert deliveries[key] == content, (deliveries[key], content)
            else:
                if deliveries:
                    previous = next(reversed(deliveries))
                    assert key == (previous[0], previous[1] + 1), (previous, key)
                deliveries[key] = content
            assert all(packet[name] is None for name in ("ts", "vel", "hdg", "cnt")), packet
            observations.append((key, now))

    def until(predicate, seconds=2):
        deadline = time.monotonic() + seconds
        while not predicate() and time.monotonic() < deadline:
            receive()
        assert predicate(), f"prazo de {seconds}s esgotado"

    def wait(seq, log=False):
        remaining = max(0, started[seq] + 2 - time.monotonic())
        until(lambda: seq in acknowledgments and (not log or seq in responses), remaining)
        return responses[seq] if log else acknowledgments[seq]

    def read(name):
        return send("request_log", {"op": "read", "name": name, "off": 0, "len": 90, "to": "wired"})

    def marker(value):
        return any(value in line for line in report["lines"])

    def diagnostics(prefix):
        return [dict((key, int(value)) for key, value in re.findall(r"(\w+)=(\d+)", line))
                for line in report["lines"] if line.startswith(prefix)]

    try:
        stream = open_serial(args.port)
        wait(send("ping"))
        assert marker("[STRESS] ativo:"), "firmware int006-stress-check necessario"

        slow = read("slow.bin")
        until(lambda: marker("[STRESS] slow=begin"))
        occupied = read("ok.bin")
        burst = [send("ping") for _ in range(6)]
        for seq in burst:
            wait(seq)
        assert wait(occupied, log=True)["err"] == "busy"
        assert wait(slow, log=True)["err"] == "busy"
        assert not marker("[STRESS] slow=end"), "janela de abandono nao observada"
        abandoned = read("ok.bin")
        assert wait(abandoned, log=True)["err"] == "busy"
        until(lambda: marker("[STRESS] slow=end"))
        healthy = wait(read("ok.bin"), log=True)
        assert base64.b64decode(healthy["d"], validate=True) == b"OBC-STRESS", healthy
        assert healthy["off"] == 0 and healthy["tot"] == 10, healthy
        report["checks"].append("timeout de 1s, caixa retida ate fim, resultado tardio descartado e nova leitura integra")

        assert wait(read("busy.bin"), log=True)["err"] == "busy"
        assert marker("[STRESS] read=busy")
        failed = wait(read("io.bin"), log=True)
        assert failed["err"] == "io" and "d" not in failed, failed
        assert marker("[STRESS] read=io")
        report["checks"].append("BUSY e READ_FAILED do backend; bytes parciais nao publicados")

        wait(send("set_time", {"unix": int(time.time())}))
        until(lambda: bool(deliveries), 5)
        first = next(iter(deliveries))
        confirm((first[0], first[1] + 100))  # ID futuro desconhecido.
        deadline = time.monotonic() + 15
        while len(observations) < 2 and time.monotonic() < deadline:
            wait(send("ping"))
            end = time.monotonic() + 0.25
            while time.monotonic() < end:
                receive()
        assert len(observations) >= 2 and set(deliveries) == {first}, observations
        retry_ms = (observations[1][1] - observations[0][1]) * 1000
        assert 9500 <= retry_ms <= 12500, retry_ms
        report["retry_ms"] = round(retry_ms, 3)
        confirm(first)
        confirm(first)  # Repetida antes do avanco da FIFO.
        until(lambda: len(deliveries) == 2)
        second = next(reversed(deliveries))
        confirm(first)  # Atrasada quando B ja aguarda a sua confirmacao.
        end = time.monotonic() + 1.5
        while time.monotonic() < end:
            wait(send("ping"))
            receive()
            assert len(deliveries) == 2, deliveries
        confirm(second)
        for count in range(3, 8):
            until(lambda: len(deliveries) >= count, 3)
            assert len(deliveries) == count, deliveries
            confirm(next(reversed(deliveries)))
        until(lambda: marker("[STRESS] append=NO_SPACE files=8")
              and any(row["confirmed"] == 7 and row["storage_failures"] > 0
                      for row in diagnostics("[DELIVERY] occupied=")), 3)
        until(lambda: any(row["SD"] == 0 and row["failures"] > 0
                          for row in diagnostics("[FLOW] received=")), 3)
        wait(send("ping"))
        healthy = wait(read("ok.bin"), log=True)
        assert base64.b64decode(healthy["d"], validate=True) == b"OBC-STRESS", healthy
        assert len(deliveries) == 7, deliveries
        assert any(row["unknown"] >= 1 for row in diagnostics("[DELIVERY] occupied="))
        report["checks"].append("7 entregas FIFO; cf desconhecido, repetido e atrasado; retentativa com ID/conteudo preservados")
        report["checks"].append("NO_SPACE injetado apos 8 FCD1; fluxo USB e confirmacoes continuam, falha contabilizada")

        flow = diagnostics("[FLOW] received=")
        assert flow and max(row["poll_gap_ms"] for row in flow) <= 10, flow
        assert all(row[key] > 0 for row in flow for key in ("stack_rx", "stack_sd", "stack_uart")), flow
        assert len(acknowledgments) == len(started), (len(acknowledgments), len(started))
        report["deliveries"] = [list(key) for key in deliveries]
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
    print(f"ObcStressCheck passed; {args.report}")


if __name__ == "__main__":
    main()
