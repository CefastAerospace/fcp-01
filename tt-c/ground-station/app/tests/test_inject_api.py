"""Injeção de quadros fictícios pela API (bancada, sem hardware)."""

import json

from fcp_gs.server import create_app

TM = {"n": 41, "t": "tm", "ts": 1760000123, "md": "mission", "vb": 7.81, "ib": 420,
      "vs": 9.1, "is": 310, "tp": [21.5, 22, 23.1, 20.9, 24.3], "w": 12.4, "az": 37,
      "ac": 3, "msg": 1200, "rssi": -81, "snr": 9.5, "rx": 10, "err": 0, "rst": 0}


def test_inject_packet_updates_state(make_link, tmp_path):
    app = create_app(make_link(), tmp_path)
    client = app.test_client()

    response = client.post("/api/inject", json={"packet": TM})
    assert response.status_code == 200
    assert response.get_json() == {"ok": True}

    state = client.get("/api/state").get_json()
    assert state["latest_tm"] == TM


def test_inject_gs_line_updates_meta(make_link, tmp_path):
    app = create_app(make_link(), tmp_path)
    client = app.test_client()
    line = json.dumps({"gs": {"rssi": -77, "snr": 10.0, "ms": 1}, "pkt": TM})

    response = client.post("/api/inject", json={"line": line})
    assert response.status_code == 200

    state = client.get("/api/state").get_json()
    assert state["latest_tm"] == TM
    assert state["last_meta"]["rssi"] == -77


def test_inject_aircraft(make_link, tmp_path):
    app = create_app(make_link(), tmp_path)
    client = app.test_client()
    aircraft = {"n": 42, "t": "ac", "icao": "E4890D", "lat": -15.9, "lon": -47.8,
                "alt": 30000, "vel": 450, "hdg": 37, "cnt": 5, "ts": 1760000120}

    response = client.post("/api/inject", json={"packet": aircraft})
    assert response.status_code == 200

    state = client.get("/api/state").get_json()
    assert state["aircraft"][0]["icao"] == "E4890D"


def test_inject_rejects_invalid_body(make_link, tmp_path):
    app = create_app(make_link(), tmp_path)
    client = app.test_client()

    assert client.post("/api/inject", json={}).status_code == 400
    assert client.post("/api/inject", json={"line": 123}).status_code == 400
    assert client.post("/api/inject", json={"packet": [1, 2]}).status_code == 400
    assert client.post("/api/inject", data="não é json", content_type="application/json").status_code == 400
