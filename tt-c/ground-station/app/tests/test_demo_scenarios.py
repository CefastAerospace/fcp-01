"""Cenários do demo: cada um produz a variação prometida, e a estação a interpreta.

O satélite roda sem thread (start=False) e com relógio simulado; cada _tick()
equivale a um período de 2 s. As linhas geradas passam pelo Link real, sem
serial, para conferir o que o painel mostraria.
"""

import json

import pytest

from fcp_gs.demo import (LOW_BATTERY_V, OVERTEMP_C, PERIOD_S, RECOVERED_BATTERY_V, SCENARIOS,
                         DemoSatellite)
from fcp_gs.link import Link
from fcp_gs.server import create_app


class Clock:
    def __init__(self) -> None:
        self.t = 1000.0

    def __call__(self) -> float:
        return self.t


def make(scenario: str, wired: bool = False, seed: int = 7) -> tuple[DemoSatellite, Clock]:
    clock = Clock()
    return DemoSatellite(scenario=scenario, wired=wired, seed=seed, clock=clock, start=False), clock


def tick(satellite: DemoSatellite, clock: Clock, count: int = 1) -> None:
    for _ in range(count):
        clock.t += PERIOD_S
        satellite._tick()


def drain(satellite: DemoSatellite) -> list[dict]:
    lines = []
    while not satellite.lines.empty():
        lines.append(json.loads(satellite.lines.get()))
    return lines


def packets(lines: list[dict]) -> list[dict]:
    """Pacotes que chegaram íntegros (sem envelope gs, CRC ou corrompidos)."""
    return [line["pkt"] if "gs" in line else line for line in lines
            if "gs" not in line or "pkt" in line]


def tc(satellite: DemoSatellite, seq: int, cmd: str, args: dict | None = None) -> list[dict]:
    body = {"t": "tc", "seq": seq, "cmd": cmd}
    if args is not None:
        body["args"] = args
    satellite.write(json.dumps(body).encode())
    return packets(drain(satellite))


def ack_status(replies: list[dict], seq: int) -> str | None:
    return next((p["st"] for p in replies if p.get("t") == "ack" and p.get("seq") == seq), None)


def feed(link: Link, lines: list[dict]) -> None:
    for line in lines:
        link.handle_line(json.dumps(line))


# ─── Todos os cenários ───────────────────────────────────────────────


@pytest.mark.parametrize("name", list(SCENARIOS))
@pytest.mark.parametrize("wired", [False, True])
def test_every_scenario_runs_and_station_parses_it(name, wired):
    satellite, clock = make(name, wired=wired)
    link = Link(satellite, "wired" if wired else "rf")
    for seq, cmd in enumerate(("ping", "request_tm", "set_time", "enter_safe", "exit_safe"), start=1):
        args = {"unix": 1_800_000_000} if cmd == "set_time" else None
        satellite.write(json.dumps({"t": "tc", "seq": seq, "cmd": cmd, "args": args}).encode())
        tick(satellite, clock, 20)
        feed(link, drain(satellite))
    assert link.snapshot()["loss"]["received"] > 0


def test_describe_lists_every_scenario():
    satellite, _ = make("passagem")
    described = satellite.describe()
    assert described["current"] == "passagem"
    assert [s["name"] for s in described["scenarios"]] == list(SCENARIOS)
    assert all(s["title"] and s["description"] for s in described["scenarios"])


def test_unknown_scenario_is_rejected():
    with pytest.raises(ValueError):
        make("marte")


# ─── Variações ───────────────────────────────────────────────────────


def test_dense_traffic_and_empty_sky():
    dense, clock = make("trafego-denso")
    tick(dense, clock)
    icaos = {p["icao"] for p in packets(drain(dense)) if p["t"] == "ac"}
    assert len(icaos) == 25

    empty, clock = make("sem-aeronaves")
    tick(empty, clock, 10)
    kinds = {p["t"] for p in packets(drain(empty))}
    assert "ac" not in kinds and "tm" in kinds


def test_bad_link_shows_loss_crc_and_corruption():
    satellite, clock = make("enlace-ruim")
    link = Link(satellite, "rf")
    tick(satellite, clock, 60)
    lines = drain(satellite)
    feed(link, lines)

    assert any(line.get("err") == "crc" for line in lines)
    assert any("raw" in line for line in lines)
    assert max(line["gs"]["rssi"] for line in lines) < -100
    state = link.snapshot()
    assert state["counts"].get("crc", 0) > 0
    assert 0.03 < state["loss"]["loss_rate"] < 0.25  # 8% perdidos + CRC + corrompidos.


def test_bad_link_drops_some_telecommands():
    satellite, clock = make("enlace-ruim")
    unanswered = sum(ack_status(tc(satellite, seq, "ping"), seq) is None for seq in range(1, 201))
    assert 10 < unanswered < 70  # ~15% de subida perdida + descida perdida.


def test_bad_link_does_not_apply_to_umbilical():
    satellite, clock = make("enlace-ruim", wired=True)
    link = Link(satellite, "wired")
    tick(satellite, clock, 30)
    feed(link, drain(satellite))
    assert link.snapshot()["loss"]["lost"] == 0


def test_pass_has_contact_then_silence():
    satellite, clock = make("passagem")
    contact = []
    for _ in range(int(90 / PERIOD_S)):
        tick(satellite, clock)
        contact.append([line["gs"]["rssi"] for line in drain(satellite)])
    tick(satellite, clock, 2)  # Já fora do contato (90 s).
    drain(satellite)
    tick(satellite, clock, 10)
    assert drain(satellite) == []
    assert ack_status(tc(satellite, 1, "ping"), 1) is None  # Sem contato, sem ACK.

    peak = max(max(r) for r in contact if r)
    edge = max(contact[0] + contact[-1])
    assert peak > -90 and edge < peak - 15  # RSSI sobe e desce com a elevação.

    tick(satellite, clock, int(45 / PERIOD_S))  # Próxima passagem.
    assert drain(satellite)


def test_pass_gap_appears_as_loss_after_reacquisition():
    satellite, clock = make("passagem")
    link = Link(satellite, "rf")
    tick(satellite, clock, int(135 / PERIOD_S) + 3)
    feed(link, drain(satellite))
    assert link.snapshot()["loss"]["lost"] > 20


def test_low_battery_enters_low_power_and_recovers():
    satellite, clock = make("bateria-baixa")
    seen = []
    for _ in range(60):
        tick(satellite, clock)
        seen += packets(drain(satellite))
        if satellite.mode == "lowpwr":
            break
    assert {"t": "ev", "ev": "low_power"}.items() <= next(p for p in seen if p["t"] == "ev").items()
    assert min(p["vb"] for p in seen if p["t"] == "tm") < LOW_BATTERY_V + 0.05

    tick(satellite, clock, 3)
    after = packets(drain(satellite))
    assert all(p["md"] == "lowpwr" and p["ac"] == 0 for p in after if p["t"] == "tm")
    assert ack_status(tc(satellite, 1, "start_mission"), 1) == "state_rejected"
    assert ack_status(tc(satellite, 2, "exit_safe"), 2) == "state_rejected"  # Ainda abaixo de 7,0 V.

    while satellite.battery < RECOVERED_BATTERY_V:
        tick(satellite, clock)
    drain(satellite)
    assert ack_status(tc(satellite, 3, "exit_safe"), 3) == "ok"
    assert satellite.mode == "bench"


def test_low_power_stops_aircraft_deliveries():
    satellite, clock = make("bateria-baixa")
    while satellite.mode != "lowpwr":
        tick(satellite, clock)
    drain(satellite)
    sequence = satellite.sequence
    tick(satellite, clock, 5)
    assert satellite.sequence == sequence


def test_overheat_forces_safe_with_beacon_and_cools_down():
    satellite, clock = make("superaquecimento")
    seen = []
    for _ in range(80):
        tick(satellite, clock)
        seen += packets(drain(satellite))
        if satellite.mode == "safe":
            break
    assert any(p["t"] == "ev" and p["ev"] == "overtemp" for p in seen)
    assert max(p["tp"][2] for p in seen if p["t"] == "tm") > OVERTEMP_C - 2

    tick(satellite, clock, 10)
    later = packets(drain(satellite))
    assert any(p["t"] == "bcn" for p in later)
    temps = [p["tp"][2] for p in later if p["t"] == "tm"]
    assert temps[-1] < temps[0]  # Esfria em SAFE.
    assert ack_status(tc(satellite, 1, "exit_safe"), 1) == "ok"


def test_reboots_reset_counter_boot_and_time():
    satellite, clock = make("reinicios", wired=True)
    link = Link(satellite, "wired")
    tick(satellite, clock, 10)
    before = packets(drain(satellite))
    feed(link, before)
    boot = next(p["id"][0] for p in before if p["t"] == "ac")

    tick(satellite, clock, 25)  # Passa dos 60 s.
    after = packets(drain(satellite))
    feed(link, after)
    assert link.snapshot()["loss"]["restarts"] >= 1
    assert any(p["t"] == "ac" and p["id"][0] > boot for p in after)
    last_tm = [p for p in after if p["t"] == "tm"][-1]
    assert last_tm["rst"] >= 1 and last_tm["ts"] is None and last_tm["md"] == "bench"

    assert ack_status(tc(satellite, 50, "set_time", {"unix": 1_800_000_000}), 50) == "ok"
    tick(satellite, clock)
    assert [p for p in packets(drain(satellite)) if p["t"] == "tm"][-1]["ts"] is not None


def test_partial_data_has_nulls_and_aircraft_without_position():
    satellite, clock = make("dados-parciais", wired=True)
    link = Link(satellite, "wired")
    tick(satellite, clock, 20)
    lines = drain(satellite)
    feed(link, lines)

    tms = [p for p in lines if p["t"] == "tm"]
    assert any(tm["vb"] is None for tm in tms) and any(tm["vb"] is not None for tm in tms)
    assert any(tm["tp"] is None for tm in tms)
    no_position = {p["icao"] for p in lines if p["t"] == "ac" and p["lat"] is None}
    assert no_position and all(p["lon"] is None for p in lines if p["t"] == "ac" and p["lat"] is None)
    # Na tabela, sim; no rastro, só quem já teve posição.
    tracked = link.tracks_snapshot()
    assert {a["icao"] for a in link.snapshot()["aircraft"]} >= no_position
    assert all(point[0] is not None for track in tracked.values() for point in track)


def test_bench_obc_matches_current_firmware():
    satellite, clock = make("bancada-obc")
    tick(satellite, clock, 5)
    periodic = packets(drain(satellite))
    assert "tm" not in {p["t"] for p in periodic}  # TM só sob demanda.
    assert all(p["vel"] is None and p["hdg"] is None and p["cnt"] is None and p["ts"] is None
               for p in periodic if p["t"] == "ac")

    replies = tc(satellite, 1, "request_tm")
    assert ack_status(replies, 1) == "ok"
    tm = next(p for p in replies if p["t"] == "tm")
    assert tm["md"] == "bench" and tm["ts"] is None
    assert tm["vb"] is None and tm["tp"] is None and tm["msg"] is None
    assert tm["rssi"] is not None  # Depois de um TC pelo RF.

    assert ack_status(tc(satellite, 2, "set_mode", {"mode": "safe"}), 2) == "invalid_cmd"
    assert ack_status(tc(satellite, 3, "request_tm", {"x": 1}), 3) == "invalid_param"
    assert ack_status(tc(satellite, 4, "set_time", {"unix": True}), 4) == "invalid_param"
    assert ack_status(tc(satellite, 5, "set_time", {"unix": 0}), 5) == "ok"  # Zero é válido.
    tm = next(p for p in tc(satellite, 6, "get_status") if p["t"] == "tm")
    assert isinstance(tm["ts"], int)


def test_repeated_seq_gets_same_ack_without_reexecuting():
    satellite, _ = make("nominal", wired=True)
    first = tc(satellite, 9, "enter_safe")
    satellite.mode = "bench"  # Se reexecutasse, voltaria para safe.
    second = tc(satellite, 9, "enter_safe")
    assert ack_status(first, 9) == ack_status(second, 9) == "ok"
    assert satellite.mode == "bench" and satellite.rx == 1


def test_switching_scenario_live_resets_its_state():
    satellite, clock = make("bateria-baixa")
    while satellite.mode != "lowpwr":
        tick(satellite, clock)
    satellite.set_scenario("trafego-denso")
    assert satellite.mode == "bench" and len(satellite.aircraft) == 25
    tick(satellite, clock)
    assert any(p["t"] == "ac" for p in packets(drain(satellite)))


# ─── API ─────────────────────────────────────────────────────────────


def test_api_switches_scenario(tmp_path):
    satellite, _ = make("nominal")
    link = Link(satellite, "rf")
    events = []
    link.add_listener(events.append)
    client = create_app(link, tmp_path, demo=satellite).test_client()

    assert client.get("/api/config").get_json()["demo"]["current"] == "nominal"
    response = client.post("/api/demo/scenario", json={"name": "enlace-ruim"})
    assert response.status_code == 200 and response.get_json()["current"] == "enlace-ruim"
    assert satellite.scenario.name == "enlace-ruim"
    assert any("enlace-ruim" in e.get("text", "") for e in events)
    assert client.post("/api/demo/scenario", json={"name": "marte"}).status_code == 400


def test_api_scenario_absent_without_demo(make_link, tmp_path):
    client = create_app(make_link(), tmp_path).test_client()
    assert client.get("/api/config").get_json()["demo"] is None
    assert client.post("/api/demo/scenario", json={"name": "nominal"}).status_code == 404
