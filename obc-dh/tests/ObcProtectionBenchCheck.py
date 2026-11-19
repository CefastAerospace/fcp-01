#!/usr/bin/env python3
"""Verifica proteção EPS/Térmico e redução de descidas por USB na INT-010."""
import argparse
import json
from pathlib import Path
import sys
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tt-c/ground-station/app"))
from fcp_gs.link import Link, open_serial
from fcp_gs.protocol import encode_confirmation


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.report.exists():
        parser.error("escolha um caminho de relatório novo")

    report = {"passed": False, "port": args.port, "exchanges": [], "events": []}
    events = report["events"]
    condition = threading.Condition()
    link = None

    def receive(event):
        with condition:
            events.append(event)
            condition.notify_all()

    def wait_for(matches, seconds, start=0):
        deadline = time.monotonic() + seconds
        with condition:
            while True:
                found = next((event for event in events[start:] if matches(event)), None)
                if found is not None:
                    return found
                remaining = deadline - time.monotonic()
                assert remaining > 0, events[-10:]
                condition.wait(remaining)

    def wait_log(prefix, suffix, seconds=6):
        return wait_for(lambda event: event.get("text", "").startswith(prefix)
                        and event["text"].endswith(suffix), seconds)

    def command(name, params=None, expected="ok", mode=None):
        start = len(events)
        result = link.send_tc(name, params, retries=1)
        report["exchanges"].append({"expected": expected, **result})
        assert result["status"] == expected and 0 <= result["q"] <= 200, result
        assert 0 <= result["rtt_ms"] <= 2000, result
        if mode is not None:
            event = wait_for(lambda entry: entry.get("pkt", {}).get("t") == "tm", 2, start)
            tm = event["pkt"]
            assert tm["md"] == mode, tm
            report["exchanges"][-1]["tm"] = tm
        return result

    def has_delivery(event):
        return event.get("dir") == "rx" and event.get("pkt", {}).get("t") == "ac"

    try:
        link = Link(open_serial(args.port), "wired")
        link.confirmation_filter = lambda delivery, repeated: False
        link.add_listener(receive)
        link.start()

        command("get_status", mode="safe")
        command("adcs_start")
        command("adcs_stop")
        wait_log("[MODES-SD] event", "detumb>safe stop_adcs")
        command("set_time", {"unix": int(time.time())})
        first = wait_for(has_delivery, 8)["pkt"]["id"]

        command("set_mode", {"mode": "detumb"})
        command("get_status", mode="lowpwr")
        command("request_log", {"op": "list"}, "state_rejected")
        command("adcs_start", expected="state_rejected")
        command("start_mission", expected="state_rejected")
        command("exit_safe", expected="state_rejected")

        # Retira a primeira entrega da fila durante a proteção e verifica a pausa da próxima.
        link.stream.write(encode_confirmation(tuple(first)))
        link.stream.flush()
        report["confirmed_during_protection"] = first
        wait_for(lambda event: event.get("text", "").startswith("[DELIVERY] ")
                 and " attempts=1 " in event["text"] and " confirmed=1 " in event["text"], 4)
        wait_log("[MODES-SD] event", "detumb>lowpwr protection")
        wait_log("[PROTECTION-SD] fault", "active unavailable=00 low=01 high=00")
        wait_log("[PROTECTION-SD] fault", "active unavailable=00 low=00 high=04")
        command("exit_safe", expected="state_rejected")

        wait_log("[PROTECTION-SD] event", "recovery_allowed unavailable=00 low=00 high=00", 12)
        command("get_status", mode="lowpwr")
        assert [event["pkt"]["id"] for event in events if has_delivery(event)] == [first], events[-10:]
        resume_from = len(events)
        command("exit_safe")
        command("get_status", mode="safe")
        resumed = wait_for(has_delivery, 4, resume_from)["pkt"]["id"]
        assert resumed != first, resumed
        report["first_after_recovery"] = resumed
        wait_log("[MODES-SD] event", "lowpwr>safe recovery")

        request = command("request_log", {"op": "list"})
        listing = wait_for(lambda event: event.get("pkt", {}).get("t") == "lg"
                           and event["pkt"].get("rq") == request["seq"], 2)["pkt"]
        assert listing.get("err") is None, listing
        report["passed"] = True
    finally:
        if link is not None:
            link.close()
        with args.report.open("x") as output:
            json.dump(report, output, indent=2)
    print(f"INT-010 USB passou. Relatório em {args.report}")


if __name__ == "__main__":
    main()
