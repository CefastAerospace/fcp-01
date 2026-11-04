"""Satélite demo: TM e aeronaves periódicas, ACK aos TCs."""

import json
import time


def _wait_parse(satellite, deadline_s: float = 5.0) -> dict:
    """Lê a próxima linha em até deadline_s e a devolve como JSON."""
    deadline = time.monotonic() + deadline_s
    while time.monotonic() < deadline:
        line = satellite.readline()
        if line:
            return json.loads(line)
        time.sleep(0.05)
    raise TimeoutError("demo não emitiu linha em tempo")


def test_demo_emits_tm_and_aircraft():
    from fcp_gs.demo import DemoSatellite

    satellite = DemoSatellite()
    seen = set()
    while seen != {"tm", "ac"}:
        body = _wait_parse(satellite)
        seen.add(body["t"])
        if body["t"] == "ac":
            assert set(body) >= {"n", "ts", "icao", "lat", "lon", "alt", "vel", "hdg", "cnt"}
        if body["t"] == "tm":
            assert body["vb"] and body["tp"]


def test_demo_answers_tc():
    from fcp_gs.demo import DemoSatellite

    satellite = DemoSatellite()
    satellite.write(json.dumps({"t": "tc", "seq": 5, "cmd": "ping"}).encode())
    body = _wait_parse(satellite, deadline_s=3.0)
    assert body == {"n": body["n"], "t": "ack", "seq": 5, "cmd": "ping", "st": "ok", "q": 3}


def test_demo_burst_is_limited():
    from fcp_gs.demo import DemoSatellite

    satellite = DemoSatellite()
    satellite.write(json.dumps({"t": "tc", "seq": 6, "cmd": "test_tm", "args": {"count": 1000}}).encode())
    expect = _wait_parse(satellite, deadline_s=3.0)
    assert expect["t"] == "ack"
    for _ in range(10):
        assert _wait_parse(satellite, deadline_s=1.0)["t"] == "tm"
