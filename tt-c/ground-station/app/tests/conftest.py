"""Satélite simulado na serial, para testar a estação terrena sem hardware."""

import base64
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
    burst_limit: TMs do test_tm efetivamente transmitidas (simula rajada interrompida).
    ignore_cmds: comandos que não recebem ACK (simula enlace caído).
    files: arquivos do SD para o request_log (nome -> bytes); open_files ficam abertos.
    busy_once: offsets de leitura que respondem "busy" na primeira vez.
    drop_log_once: offsets cuja primeira resposta "lg" se perde (ACK chega).
    """

    def __init__(self, bridge: bool = True, drop_first: bool = False, skip_n: set[int] | None = None,
                 burst_limit: int | None = None, ignore_cmds: set[str] | None = None,
                 files: dict[str, bytes] | None = None, open_files: set[str] | None = None,
                 busy_once: set[int] | None = None, drop_log_once: set[int] | None = None,
                 list_page: int = 2) -> None:
        self.bridge = bridge
        self.drop_first = drop_first
        self.skip_n = skip_n or set()
        self.burst_limit = burst_limit
        self.ignore_cmds = ignore_cmds or set()
        self.files = files or {}
        self.open_files = open_files or set()
        self.busy_once = set(busy_once or ())
        self.drop_log_once = set(drop_log_once or ())
        self.list_page = list_page
        self.log_requests: list[dict] = []
        self.confirmations: list[list[int]] = []
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

    def _log_response(self, seq: int, args: dict) -> dict | None:
        """Responde request_log como o OBC; None simula resposta perdida."""
        self.log_requests.append(args)
        base = {"t": "lg", "rq": seq, "op": args["op"]}
        if args["op"] == "list":
            names = sorted(name for name in self.files if name > args.get("after", ""))
            page = names[:self.list_page]
            return {**base, "ls": [[name, len(self.files[name]), int(name in self.open_files)] for name in page],
                    "end": len(page) == len(names)}
        name, offset = args["name"], args["off"]
        if offset in self.drop_log_once:
            self.drop_log_once.discard(offset)
            return None
        if offset in self.busy_once:
            self.busy_once.discard(offset)
            return {**base, "name": name, "off": offset, "err": "busy"}
        if name not in self.files:
            return {**base, "name": name, "off": offset, "err": "not_found"}
        content = self.files[name]
        if offset > len(content):
            return {**base, "name": name, "off": offset, "err": "range"}
        block = content[offset:offset + args["len"]]
        return {**base, "name": name, "off": offset, "tot": len(content), "d": base64.b64encode(block).decode()}

    def write(self, data: bytes) -> int:
        tc = json.loads(data)
        if tc.get("t") == "cf":
            self.confirmations.append(tc["id"])
            return len(data)
        seq = tc["seq"]
        if self.drop_first and seq not in self.seen:
            self.seen.add(seq)
            return len(data)
        if tc["cmd"] in self.ignore_cmds:
            return len(data)
        self._downlink({"t": "ack", "seq": seq, "cmd": tc["cmd"], "st": "ok", "q": 3})
        if tc["cmd"] in ("request_tm", "get_status"):
            self._downlink(self._tm())
        if tc["cmd"] == "request_log":
            response = self._log_response(seq, tc["args"])
            if response is not None:
                self._downlink(response)
        if tc["cmd"] == "test_tm":
            count = tc["args"]["count"]
            for _ in range(count if self.burst_limit is None else min(count, self.burst_limit)):
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

    def factory(deliveries=None, **kwargs):
        link = Link(FakeSatellite(**kwargs), "rf", deliveries=deliveries)
        link.start()
        links.append(link)
        return link

    yield factory
    for link in links:
        link.close()
