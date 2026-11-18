"""Roteiros de bancada (dedup, delivery, congestion, health) contra o satélite do demo."""

import pytest

from fcp_gs import demo as demo_module
from fcp_gs.deliveries import DeliveryStore
from fcp_gs.demo import DemoSatellite
from fcp_gs.link import Link
from fcp_gs.verification import run_congestion, run_dedup, run_delivery, run_health, summarize


@pytest.fixture
def demo_link():
    links = []

    def factory(scenario="nominal", wired=True, deliveries=None, stream=None):
        stream = stream or DemoSatellite(wired=wired, scenario=scenario, seed=3)
        link = Link(stream, "wired" if wired else "rf", deliveries=deliveries)
        link.start()
        links.append(link)
        return link

    yield factory
    for link in links:
        link.close()


def stop_reader(link: Link) -> None:
    """Para a leitura sem fechar o satélite do demo (simula a queda só da estação)."""
    link._running = False
    link._thread.join(timeout=1.0)
    link.deliveries.close()


def test_dedup_passes_when_repeats_are_not_reexecuted(demo_link):
    report = run_dedup(demo_link(scenario="bancada-obc"))
    assert report["passed"], summarize(report)
    assert report["rx_delta"] == 1
    assert all(r["status"] == "ok" for r in report["repeats"])


class ReexecutingSatellite(DemoSatellite):
    """Defeito: esquece os ACKs e reexecuta o TC repetido."""

    def write(self, data: bytes) -> int:
        with self._lock:
            self.acks.clear()
        return super().write(data)


def test_dedup_fails_when_satellite_reexecutes(demo_link):
    stream = ReexecutingSatellite(wired=True, scenario="bancada-obc", seed=3)
    report = run_dedup(demo_link(stream=stream))
    assert not report["passed"]
    assert report["rx_delta"] == 1 + 3


def test_delivery_lost_confirmation_is_retransmitted_and_processed_once(demo_link, monkeypatch):
    monkeypatch.setattr(demo_module, "RETRY_S", 1.0)
    link = demo_link(scenario="bancada-obc")
    report = run_delivery(link, wait_s=10, settle_s=3)
    assert report["passed"], summarize(report)
    assert report["repeated_flags"] == [False, True]
    assert report["counts"]["withheld"] == 1


def test_delivery_survives_station_restart(tmp_path, demo_link, monkeypatch):
    monkeypatch.setattr(demo_module, "RETRY_S", 1.0)
    path = tmp_path / "deliveries.jsonl"
    first = demo_link(scenario="bancada-obc", deliveries=DeliveryStore(path))
    crash = run_delivery(first, wait_s=10, stop_after_withhold=True)
    assert crash["passed"], summarize(crash)
    stop_reader(first)

    second = demo_link(stream=first.stream, deliveries=DeliveryStore(path))
    resume = run_delivery(second, wait_s=10, settle_s=3, target=tuple(crash["id"]))
    assert resume["passed"], summarize(resume)
    assert resume["repeated_flags"] == [True]
    assert resume["counts"]["withheld"] == 0


def test_congestion_ping_is_acked_during_burst(demo_link):
    report = run_congestion(demo_link(), count=50, ping_gap_s=0.2)
    assert report["passed"], summarize(report)
    assert report["pings"] >= 1


def test_health_requires_wired(demo_link):
    report = run_health(demo_link(wired=False), duration_s=1, announce=lambda _: None)
    assert not report["passed"]
    assert report["samples"] == []
