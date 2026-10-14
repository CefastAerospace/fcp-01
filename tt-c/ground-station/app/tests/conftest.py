"""Satélite simulado na serial, para testar a estação terrena sem hardware."""

import json
import queue
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))


class FakeSatellite:
    """Stream serial que responde TCs como o link-test, opcionalmente pela ponte RF.

    drop_first: descarta a primeira tentativa de cada seq (testa retentativa).
    skip_n: contadores "n" a pular (simula perdas no ar).
    """

    def __init__(self, bridge: bool = True, drop_first: bool = False, skip_n: set[int] | None = None) -> None:
        self.bridge = bridge
        self.drop_first = drop_first
        self.skip_n = skip_n or set()
        self.n = 0
        self.seen: set[int] = set()
        self.lines: queue.Queue[bytes] = queue.Queue()

    def _downlink(self, packet: dict) -> None:
        n = self.n
        self.n += 1
        if n in self.skip_n:
            return
        body = {"n": n, **packet}
        if self.bridge:
            body = {"gs": {"rssi": -80, "snr": 9.0, "ms": 1}, "pkt": body}
        self.lines.put((json.dumps(body, separators=(",", ":")) + "\n").encode())

    def _tm(self) -> dict:
        return {"t": "tm", "ts": 1760000000, "md": "bench", "vb": 7.8, "tp": [21, 22, 23, 24, 25]}

    def write(self, data: bytes) -> int:
        tc = json.loads(data)
        seq = tc["seq"]
        if self.drop_first and seq not in self.seen:
            self.seen.add(seq)
            return len(data)
        self._downlink({"t": "ack", "seq": seq, "cmd": tc["cmd"], "st": "ok", "q": 3})
        if tc["cmd"] in ("request_tm", "get_status"):
            self._downlink(self._tm())
        if tc["cmd"] == "test_tm":
            for _ in range(tc["args"]["count"]):
                self._downlink(self._tm())
        return len(data)

    def flush(self) -> None:
        pass

    def readline(self) -> bytes:
        try:
            return self.lines.get(timeout=0.05)
        except queue.Empty:
            return b""

    def close(self) -> None:
        pass


@pytest.fixture
def make_link():
    from fcp_gs.link import Link

    links = []

    def factory(**kwargs):
        link = Link(FakeSatellite(**kwargs), "rf")
        link.start()
        links.append(link)
        return link

    yield factory
    for link in links:
        link.close()
