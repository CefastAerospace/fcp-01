import json

import pytest

from fcp_gs.protocol import MAX_FRAME_LEN, LossTracker, encode_tc, parse_line


def test_encode_tc_compact_line():
    line = encode_tc(12, "set_time", {"unix": 1760000000})
    assert line == b'{"t":"tc","seq":12,"cmd":"set_time","args":{"unix":1760000000}}\n'
    assert json.loads(encode_tc(1, "ping")) == {"t": "tc", "seq": 1, "cmd": "ping"}


def test_encode_tc_rejects_oversized():
    with pytest.raises(ValueError):
        encode_tc(1, "ping", {"pad": "x" * MAX_FRAME_LEN})


def test_parse_bridge_packet():
    received = parse_line('{"gs":{"rssi":-80,"snr":9.2,"ms":5},"pkt":{"n":3,"t":"tm"}}\n')
    assert received.kind == "packet"
    assert received.packet == {"n": 3, "t": "tm"}
    assert received.meta["rssi"] == -80


def test_parse_bridge_errors():
    assert parse_line('{"gs":{"rssi":-120},"err":"crc"}').kind == "crc"
    raw = parse_line('{"gs":{},"raw":"7b22"}')
    assert raw.kind == "raw" and raw.text == "7b22"


def test_parse_wired_packet_and_logs():
    assert parse_line('{"n":0,"t":"ack","seq":1}').packet["t"] == "ack"
    assert parse_line("[TT&C] SX1276 inicializado.").kind == "log"
    assert parse_line("{nao e json").kind == "log"


def test_loss_tracker_counts_gaps():
    tracker = LossTracker()
    for n in (10, 11, 13, 14, 17):
        tracker.feed(n)
    assert tracker.lost == 3
    assert tracker.received == 5
    assert tracker.loss_rate == pytest.approx(3 / 8)


def test_loss_tracker_wraps_and_restarts():
    tracker = LossTracker()
    for n in (65534, 65535, 0, 1):
        tracker.feed(n)
    assert tracker.lost == 0

    tracker.feed(1)  # Duplicado.
    assert tracker.duplicates == 1

    tracker.feed(500)
    tracker.feed(0)  # Satélite reiniciou.
    assert tracker.restarts == 1
    assert tracker.lost == 498
