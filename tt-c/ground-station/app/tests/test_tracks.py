"""Rastro das aeronaves guardado no servidor (mapa sobrevive ao recarregar a página)."""

import json

from fcp_gs.link import TRACK_POINTS
from fcp_gs.server import create_app


def ac(n: int, lat, lon, icao: str = "E4953A") -> str:
    return json.dumps({"n": n, "t": "ac", "ts": 1760000000 + n, "icao": icao,
                       "lat": lat, "lon": lon, "alt": 30000, "vel": 450, "hdg": 37, "cnt": n})


def test_repeated_position_is_not_duplicated(make_link):
    link = make_link()
    link.handle_line(ac(1, -15.1, -47.1))
    link.handle_line(ac(2, -15.1, -47.1))
    link.handle_line(ac(3, -15.2, -47.2))

    assert link.tracks_snapshot() == {"E4953A": [[-15.1, -47.1, 30000, 1760000001],
                                                 [-15.2, -47.2, 30000, 1760000003]]}


def test_invalid_positions_are_ignored(make_link):
    link = make_link()
    for n, (lat, lon) in enumerate([(None, -47.0), ("x", 1), (95.0, 0.0), (0.0, 181.0), (True, 1.0)]):
        link.handle_line(ac(n, lat, lon))
    link.handle_line('{"n":9,"t":"ac","ts":1,"icao":"E4953A","lat":NaN,"lon":1}')

    assert link.tracks_snapshot() == {}
    assert "E4953A" in link.snapshot()["aircraft"][0]["icao"]  # A tabela ainda mostra a aeronave.


def test_track_is_capped(make_link):
    link = make_link()
    for n in range(TRACK_POINTS + 50):
        link.handle_line(ac(n, -15 - n / 1000, -47.0))

    track = link.tracks_snapshot()["E4953A"]
    assert len(track) == TRACK_POINTS
    assert track[-1][0] == -15 - (TRACK_POINTS + 49) / 1000


def test_api_tracks_and_config(make_link, tmp_path):
    link = make_link()
    link.handle_line(ac(1, -15.1, -47.1, "AAA111"))
    link.handle_line(ac(2, -34.5, -58.5, "BBB222"))
    client = create_app(link, tmp_path, map_center=(-34.57, -58.52, 9)).test_client()

    tracks = client.get("/api/tracks").get_json()
    assert set(tracks) == {"AAA111", "BBB222"}
    assert tracks["BBB222"][0][:2] == [-34.5, -58.5]

    config = client.get("/api/config").get_json()
    assert config["map_center"] == [-34.57, -58.52] and config["map_zoom"] == 9
    assert "set_mode" in config["commands"]
