import csv
import json

from fcp_gs.analyze import analyze


def test_analyze_session_log(tmp_path):
    events = [
        {"dir": "tx", "attempt": 1, "pkt": {"t": "tc", "seq": 1, "cmd": "ping"}},
        {"dir": "rx", "kind": "packet", "link": "rf", "gs": {"rssi": -80}, "pkt": {"n": 0, "t": "ack", "seq": 1, "q": 4}},
        {"dir": "rx", "kind": "packet", "link": "rf", "gs": {"rssi": -81, "snr": 9}, "pkt": {"n": 1, "t": "tm", "vb": 7.8, "tp": [20, 21, 22, 23, 24]}},
        {"dir": "rx", "kind": "crc", "link": "rf"},
        {"dir": "rx", "kind": "packet", "link": "rf", "pkt": {"n": 3, "t": "ac", "icao": "E4953A", "lat": -34.5}},
    ]
    log = tmp_path / "session.jsonl"
    log.write_text("\n".join(json.dumps(e) for e in events) + "\nlixo\n")

    summary = analyze(log)
    assert summary["tc_sent"] == 1
    assert summary["loss"]["rf"]["lost"] == 1
    assert summary["forward_ms_max"] == 4
    assert summary["aircraft_seen"] == 1
    assert summary["counts"]["crc"] == 1

    with open(tmp_path / "session" / "telemetry.csv") as handle:
        row = next(csv.DictReader(handle))
    assert row["vb"] == "7.8" and row["tp4"] == "24" and row["gs_rssi"] == "-81"
