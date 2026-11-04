"""Satélite demo: TM e aeronaves periódicas, ACK aos TCs."""

import json
import time

import pytest


def _wait_parse(satellite, deadline_s: float = 5.0) -> dict:
    """Lê a próxima linha em até deadline_s e a devolve como JSON."""
    deadline = time.monotonic() + deadline_s
    while time.monotonic() < deadline:
        line = satellite.readline()
        if line:
            body = json.loads(line)
            return body.get("pkt", body)  # Modo RF: envelope gs da ponte.
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

    satellite = DemoSatellite(wired=True)
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


def _ack(satellite, seq: int, cmd: str, args: dict | None = None) -> dict:
    tc = {"t": "tc", "seq": seq, "cmd": cmd}
    if args is not None:
        tc["args"] = args
    satellite.write(json.dumps(tc).encode())
    while True:
        body = _wait_parse(satellite, deadline_s=3.0)
        if body["t"] == "ack" and body["seq"] == seq:
            return body


def _next_tm(satellite) -> dict:
    while True:
        body = _wait_parse(satellite, deadline_s=3.0)
        if body["t"] == "tm":
            return body


def test_demo_rf_envelope_has_signal():
    from fcp_gs.demo import DemoSatellite

    satellite = DemoSatellite()
    satellite.write(json.dumps({"t": "tc", "seq": 1, "cmd": "ping"}).encode())
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        line = satellite.readline()
        if line:
            body = json.loads(line)
            assert set(body["gs"]) == {"rssi", "snr", "ms"} and body["pkt"]["t"] == "ack"
            return
    raise TimeoutError


def test_demo_set_mode_changes_tm_and_rejects_bad_args():
    from fcp_gs.demo import DemoSatellite

    satellite = DemoSatellite(wired=True)
    assert _ack(satellite, 10, "set_mode", {"mode": "sun"})["st"] == "ok"
    satellite.write(json.dumps({"t": "tc", "seq": 11, "cmd": "request_tm"}).encode())
    assert _next_tm(satellite)["md"] == "sun"
    assert _ack(satellite, 12, "set_mode", {"mode": "warp"})["st"] == "invalid_param"
    assert _ack(satellite, 13, "set_time", {"unix": "ontem"})["st"] == "invalid_param"
    assert _ack(satellite, 14, "test_tm", {"count": 0})["st"] == "invalid_param"


def test_demo_safe_rejects_mission_until_exit():
    from fcp_gs.demo import DemoSatellite

    satellite = DemoSatellite(wired=True)
    assert _ack(satellite, 20, "enter_safe")["st"] == "ok"
    assert _ack(satellite, 21, "start_mission")["st"] == "state_rejected"
    assert _ack(satellite, 22, "exit_safe")["st"] == "ok"
    assert _ack(satellite, 23, "start_mission")["st"] == "ok"
    assert satellite.mode == "mission"


def test_demo_aircraft_move_along_heading():
    import math

    from fcp_gs.demo import KT_TO_DEG_S, _Aircraft
    import random

    aircraft = _Aircraft(random.Random(1), (-15.79, -47.88))
    lat, lon, heading = aircraft.lat, aircraft.lon, aircraft.hdg
    aircraft.advance(60)
    moved = math.degrees(math.atan2((aircraft.lon - lon) * math.cos(math.radians(lat)), aircraft.lat - lat)) % 360
    assert abs((moved - heading + 180) % 360 - 180) < 1
    assert math.hypot(aircraft.lat - lat, (aircraft.lon - lon) * math.cos(math.radians(lat))) == \
        pytest.approx(aircraft.vel * KT_TO_DEG_S * 60, rel=0.01)


def test_demo_replaces_aircraft_leaving_area():
    import random

    from fcp_gs.demo import _Aircraft

    center = (-15.79, -47.88)
    aircraft = _Aircraft(random.Random(2), center)
    for _ in range(200):
        aircraft.advance(60)
    assert aircraft.outside(center)
