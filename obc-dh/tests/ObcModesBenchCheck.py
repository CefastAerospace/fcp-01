#!/usr/bin/env python3
"""Verifica a INT-007 por USB no ambiente int007-modes-check."""
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
    if args.report.exists():
        parser.error("escolha um caminho de relatório novo")

    report = {"passed": False, "port": args.port, "exchanges": [], "lines": []}
    stream = None
    sequence = int(time.time() * 1000) & 0xffff
    packets = []

    def receive():
        line = stream.readline().decode(errors="replace").strip()
        if not line:
            return
        report["lines"].append(line)
        assert "[FLOW-CHECK] FAIL" not in line, line
        parsed = parse_line(line)
        if parsed.kind == "packet":
            packets.append(parsed.packet)
            if parsed.packet["t"] == "ac":
                stream.write(encode_confirmation(tuple(parsed.packet["id"])))

    def wait_for(find, deadline):
        while True:
            found = find()
            if found is not None:
                return found
            assert time.monotonic() < deadline, report["lines"][-10:]
            receive()

    def command(name, params=None, expected="ok", mode=None, repeat=None):
        nonlocal sequence
        if repeat is None:
            sequence = (sequence + 1) & 0xffff
        sent_seq = sequence if repeat is None else repeat
        first = len(packets)
        started = time.monotonic()
        stream.write(encode_tc(sent_seq, name, params))
        stream.flush()
        ack = wait_for(lambda: next((p for p in packets[first:]
                                     if p["t"] == "ack" and p["seq"] == sent_seq), None), started + 2)
        elapsed = time.monotonic() - started
        report["exchanges"].append({"ack": ack, "expected": expected, "response_ms": elapsed * 1000})
        assert ack["cmd"] == name and ack["st"] == expected and 0 <= ack["q"] <= 200, ack
        assert elapsed <= 2, elapsed
        if mode is not None:
            tm = wait_for(lambda: next((p for p in packets[first:] if p["t"] == "tm"), None), started + 2)
            assert tm["md"] == mode, tm
            report["exchanges"][-1]["tm"] = tm
        return sent_seq

    def journal_event(description):
        pattern = re.compile(r"\[MODES-SD\] event ms=(\d+) " + re.escape(description) + r"$")
        entry = wait_for(lambda: next((match for line in report["lines"]
                                       if (match := pattern.fullmatch(line))), None), time.monotonic() + 5)
        return int(entry[1])

    def journal_snapshot(after_ms=-1):
        pattern = re.compile(r"\[MODES-SD\] snapshot ms=(\d+)$")
        entry = wait_for(lambda: next((match for line in report["lines"]
                                       if (match := pattern.fullmatch(line)) and int(match[1]) > after_ms),
                                      None), time.monotonic() + 8)
        return int(entry[1])

    try:
        stream = open_serial(args.port)
        command("get_status", mode="safe")
        command("start_mission", expected="state_rejected")
        command("stop_mission", expected="state_rejected")
        command("start_mission", {"extra": 1}, "invalid_param")
        for params in (None, {"mode": 1}, {"mode": "safe\0mission"}, {"mode": "safe", "extra": 1}):
            command("set_mode", params, "invalid_param")
        for blocked in ("bench", "sun", "lowpwr"):
            command("set_mode", {"mode": blocked}, "state_rejected")
        command("set_time", {"unix": int(time.time())})
        before = journal_snapshot()

        start = command("set_mode", {"mode": "mission"})
        command("set_mode", {"mode": "mission"}, repeat=start)
        command("start_mission", expected="state_rejected")
        command("request_tm", mode="mission")
        command("stop_mission")
        command("get_status", mode="safe")
        started_ms = journal_event("safe>mission start_mission")
        stopped_ms = journal_event("mission>safe stop_mission")
        assert before < started_ms < stopped_ms < journal_snapshot(stopped_ms), report["lines"]
        assert sum("[MODES]" in line and line.endswith("safe>mission start_mission")
                   for line in report["lines"]) == 1, report["lines"]
        wait_for(lambda: next((line for line in report["lines"] if line == "[MODES-SD] tm md=mission"), None),
                 time.monotonic() + 5)

        command("adcs_start")
        command("request_tm", mode="detumb")
        command("adcs_stop")
        command("get_status", mode="safe")
        # A gravação da parada confirma que o escritor consumiu a fila até esse evento.
        journal_event("detumb>safe stop_adcs")
        command("set_mode", {"mode": "detumb"})
        command("get_status", mode="lowpwr")
        command("enter_safe", expected="state_rejected")
        command("exit_safe", expected="state_rejected")
        journal_event("detumb>lowpwr protection")
        time.sleep(5)
        command("exit_safe")
        command("get_status", mode="safe")
        journal_event("lowpwr>safe recovery")
        report["passed"] = True
    finally:
        if stream is not None:
            stream.close()
        with args.report.open("x") as output:
            json.dump(report, output, indent=2)
    print(f"INT-007 USB passou. Relatório em {args.report}")


if __name__ == "__main__":
    main()
