"""Entregas identificadas: confirmação, processamento único e request_log."""

import json
import time

import pytest

from fcp_gs.deliveries import DeliveryStore
from fcp_gs.logs import LogError, download_file, list_files
from fcp_gs.protocol import delivery_id, encode_confirmation

U64_MAX = (1 << 64) - 1


def ac(n: int, delivery=None, lat=-15.1, lon=-47.1, **fields) -> str:
    packet = {"n": n, "t": "ac", "ts": None, "icao": "E4953A", "lat": lat, "lon": lon, "alt": 30000,
              "vel": None, "hdg": None, "cnt": None, **fields}
    if delivery is not None:
        packet["id"] = list(delivery)
    return json.dumps(packet)


def wait_for(condition, timeout=1.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.01)
    return condition()


def test_delivery_id_domain():
    assert delivery_id({"id": [1, U64_MAX]}) == (1, U64_MAX)
    for value in ([0, 1], [1, 0], [-1, 2], [1, U64_MAX + 1], [1.0, 2], [True, 2], [1], [1, 2, 3], "1:2", None):
        assert delivery_id({"id": value}) is None
    assert encode_confirmation((U64_MAX, U64_MAX)) == \
        b'{"t":"cf","id":[18446744073709551615,18446744073709551615]}\n'
    assert len(encode_confirmation((U64_MAX, U64_MAX))) - 1 <= 255
    with pytest.raises(ValueError):
        encode_confirmation((0, 1))


def test_new_delivery_is_processed_and_confirmed(make_link):
    link = make_link()
    events = []
    link.add_listener(events.append)
    link.handle_line(ac(1, (3, 41)))

    assert link.stream.confirmations == [[3, 41]]
    assert link.snapshot()["aircraft"][0]["id"] == [3, 41]
    assert link.tracks_snapshot() == {"E4953A": [[-15.1, -47.1, 30000, None]]}
    rx = next(event for event in events if event["dir"] == "rx")
    assert rx["delivery"] == {"id": [3, 41], "repeated": False}
    # O "cf" sai depois do registro do pacote recebido.
    assert [event["dir"] for event in events] == ["rx", "tx"]
    assert events[1]["pkt"] == {"t": "cf", "id": [3, 41]}


def test_repeated_delivery_is_reconfirmed_once_processed(make_link):
    link = make_link()
    link.handle_line(ac(1, (3, 41), lat=-15.1))
    # Retransmissão com o mesmo ID: conteúdo não é reaplicado.
    link.handle_line(ac(2, (3, 41), lat=-15.9))

    assert link.stream.confirmations == [[3, 41], [3, 41]]
    assert link.snapshot()["aircraft"][0]["lat"] == -15.1
    assert len(link.tracks_snapshot()["E4953A"]) == 1
    counts = link.snapshot()["deliveries"]
    assert counts["new"] == 1 and counts["repeated"] == 1 and counts["known"] == 1
    assert link.loss.received == 2  # O "n" continua contado.


def test_packet_without_id_is_not_confirmed(make_link):
    link = make_link()
    link.handle_line(ac(1))
    link.handle_line(ac(2, None, id=[0, 1]))
    assert link.stream.confirmations == []
    assert len(link.snapshot()["aircraft"]) == 1


def test_deduplication_survives_station_restart(tmp_path, make_link):
    path = tmp_path / "deliveries.jsonl"
    first = make_link(deliveries=DeliveryStore(path))
    first.handle_line(ac(1, (3, 41)))
    first.handle_line(ac(2, (3, 42)))
    first.deliveries.close()

    # Cauda cortada por queda de energia é ignorada.
    with path.open("a") as file:
        file.write('{"id":[3,')

    restarted = make_link(deliveries=DeliveryStore(path))
    assert restarted.deliveries.ignored_lines == 1
    restarted.handle_line(ac(1, (3, 42)))
    restarted.handle_line(ac(2, (3, 43)))

    assert restarted.stream.confirmations == [[3, 42], [3, 43]]
    counts = restarted.snapshot()["deliveries"]
    assert counts["repeated"] == 1 and counts["new"] == 1 and counts["known"] == 3


def test_confirmation_is_withheld_when_store_fails(make_link):
    class BrokenStore(DeliveryStore):
        def record(self, delivery):
            raise OSError("disco cheio")

    link = make_link(deliveries=BrokenStore())
    link.handle_line(ac(1, (3, 41)))
    assert link.stream.confirmations == []
    assert link.snapshot()["deliveries"]["confirm_errors"] == 1


def test_demo_retransmits_until_confirmed():
    from fcp_gs.demo import DemoSatellite
    from fcp_gs.link import Link

    satellite = DemoSatellite(wired=True, seed=1)
    satellite.close()  # Sem laço periódico: entregas chamadas à mão.
    satellite._deliver(satellite.aircraft[0], now=0.0)
    satellite._retransmit(now=10.0)
    lines = []
    while not satellite.lines.empty():
        lines.append(json.loads(satellite.lines.get()))
    assert len(lines) == 2 and lines[0]["id"] == lines[1]["id"] and lines[0]["ts"] == lines[1]["ts"]

    link = Link(satellite, "wired")
    for line in lines:
        link.handle_line(json.dumps(line))
    assert satellite.pending == {} and satellite.confirmed == 1


def test_list_files_follows_cursor(make_link):
    files = {f"boot_{i:016x}_00000001.bin": bytes([i]) * i for i in range(1, 6)}
    link = make_link(files=files, open_files={"boot_0000000000000005_00000001.bin"}, list_page=2)
    listed = list_files(link)
    assert [item["name"] for item in listed] == sorted(files)
    assert [item["size"] for item in listed] == [1, 2, 3, 4, 5]
    assert [item["open"] for item in listed] == [False] * 4 + [True]
    assert [request.get("after", "") for request in link.stream.log_requests] == [
        "", "boot_0000000000000002_00000001.bin", "boot_0000000000000004_00000001.bin"]


def test_download_reconstructs_file(tmp_path, make_link):
    content = bytes(range(256)) * 2  # 512 bytes: 5 blocos de 90 + 62 + EOF.
    name = "boot_0000000000000001_00000002.bin"
    link = make_link(files={name: content}, busy_once={180}, drop_log_once={360})
    path = download_file(link, name, tmp_path)

    assert path.read_bytes() == content
    assert not (tmp_path / (name + ".part")).exists()
    offsets = [request["off"] for request in link.stream.log_requests]
    # "busy" e resposta perdida repetem o mesmo offset; o fim é o pedido em 512.
    assert offsets == [0, 90, 180, 180, 270, 360, 360, 450, 512]


def test_download_errors(tmp_path, make_link):
    link = make_link(files={})
    with pytest.raises(LogError) as error:
        download_file(link, "boot_01.bin", tmp_path)
    assert error.value.code == "not_found"

    with pytest.raises(LogError) as error:
        download_file(link, "../etc/passwd", tmp_path)
    assert error.value.code == "invalid_param"

    with pytest.raises(LogError) as error:
        download_file(link, "boot_01.bin", tmp_path, to="wired")
    assert error.value.code == "destination"


def test_download_rejects_inconsistent_response(tmp_path, make_link):
    name = "boot_01.bin"
    link = make_link(files={name: b"x" * 200})
    original = link.stream._log_response

    def shifted(seq, args):
        response = original(seq, args)
        if response and args["off"] == 90:
            response["tot"] = 999  # Tamanho muda no meio da extração.
        return response

    link.stream._log_response = shifted
    with pytest.raises(LogError) as error:
        download_file(link, name, tmp_path)
    assert error.value.code == "protocol"
    assert not (tmp_path / name).exists()
