"""TM de componentes (energia, térmico, atitude, missão) no estado do enlace."""

import json
from pathlib import Path

TM_FULL = {"n": 41, "t": "tm", "ts": 1760000123, "md": "mission", "vb": 7.81, "ib": 420,
           "vs": 9.1, "is": 310, "tp": [21.5, 22, 23.1, 20.9, 24.3], "w": 12.4, "az": 37,
           "ac": 3, "msg": 1200, "rssi": -81, "snr": 9.5, "rx": 10, "err": 0, "rst": 0}

SAMPLES = Path(__file__).parent / "data" / "tm_samples.jsonl"
# Todos os parâmetros do contrato; alguns podem ser null nos modos degradados.
PARAMETERS = ("vb", "ib", "vs", "is", "tp", "w", "az", "ac", "msg", "rssi", "snr", "rx", "err", "rst")


def load_samples() -> list[dict]:
    return [json.loads(line) for line in SAMPLES.read_text().splitlines() if line.strip()]


def test_tm_full_updates_state(make_link):
    link = make_link()
    events = []
    link.add_listener(events.append)

    link.handle_line(json.dumps(TM_FULL) + "\n")

    assert link.latest_tm == TM_FULL
    assert len(events) == 1
    assert events[0]["dir"] == "rx" and events[0]["pkt"] == TM_FULL
    state = link.snapshot()
    assert state["loss"]["received"] == 1
    assert state["counts"]["t:tm"] == 1


def test_tm_with_gs_envelope_keeps_meta(make_link):
    link = make_link()
    line = json.dumps({"gs": {"rssi": -88, "snr": 8.1, "ms": 5}, "pkt": TM_FULL})
    link.handle_line(line)

    assert link.latest_tm == TM_FULL
    assert link.snapshot()["last_meta"]["rssi"] == -88


def test_tm_null_fields_are_kept(make_link):
    link = make_link()
    tm = {**TM_FULL, "n": 42, "vb": None, "ib": None, "tp": None, "w": None, "az": None}
    link.handle_line(json.dumps(tm))

    assert link.latest_tm["tp"] is None
    assert link.latest_tm["vb"] is None
    # O frontend exibe "—" para esses; o contrato aceita null.


def test_tm_sequence_feed_loss_tracker(make_link):
    link = make_link()
    link.handle_line(json.dumps({**TM_FULL, "n": 10}))
    link.handle_line(json.dumps({**TM_FULL, "n": 13}))
    link.handle_line(json.dumps({**TM_FULL, "n": 20}))

    state = link.snapshot()
    assert state["loss"]["received"] == 3
    # 10→13 perde 11,12; 13→20 perde 14..19.
    assert state["loss"]["lost"] == 8


def test_tm_counter_wrap_is_loss_and_backjump_is_restart(make_link):
    link = make_link()
    link.handle_line(json.dumps({**TM_FULL, "n": 65535}))
    link.handle_line(json.dumps({**TM_FULL, "n": 1}))  # Volta: só o 0 faltou.
    link.handle_line(json.dumps({**TM_FULL, "n": 0}))  # Salto para trás: reinício.

    state = link.snapshot()
    assert state["loss"]["received"] == 3
    assert state["loss"]["lost"] == 1
    assert state["loss"]["restarts"] == 1


def test_samples_cover_all_parameters_and_modes():
    packets = load_samples()
    assert len(packets) >= 50
    assert {p["md"] for p in packets} == {"bench", "mission", "safe", "lowpwr"}
    keys = set(PARAMETERS)
    # Na bancada/missão nenhum parâmetro pode faltar ou ser null.
    for packet in packets:
        if packet["md"] in ("bench", "mission"):
            assert keys <= set(packet), f"faltam campos em md={packet['md']}: {keys - set(packet)}"
            assert all(packet[name] is not None for name in ("vb", "ib", "rssi", "snr"))
    assert any(packet["tp"] is None for packet in packets)  # Há campo indisponível no percurso.
    assert [len(packet["tp"]) for packet in packets if packet["tp"]] == [5] * len([p for p in packets if p["tp"]])


def test_whole_sample_sequence_through_link(make_link):
    link = make_link()
    packets = load_samples()
    for packet in packets:
        link.handle_line(json.dumps(packet) + "\n")

    assert link.latest_tm == packets[-1]
    state = link.snapshot()
    assert state["loss"]["received"] == len(packets)
    assert state["loss"]["lost"] == 0          # Percurso sem perda no ar.
    assert state["loss"]["restarts"] == 1      # Um salto para trás no meio do percurso.
    assert state["counts"]["t:tm"] == len(packets)
