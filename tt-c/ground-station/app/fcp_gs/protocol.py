"""Contrato JSON do enlace, lado da estação terrena.

Espelha tt-c/include/TtcProtocol.h e common/protocol/ttc-json.md.
"""

import json
from dataclasses import dataclass, field

MAX_FRAME_LEN = 255  # FIFO do SX1276.
COUNTER_MODULO = 65536

COMMANDS = (
    "ping", "set_mode", "start_mission", "stop_mission", "get_status",
    "enter_safe", "exit_safe", "request_tm", "set_time", "adcs_start",
    "adcs_stop", "deploy_antenna", "request_log", "test_tm", "reset_obc",
)

MODES = ("bench", "safe", "detumb", "sun", "mission", "lowpwr")


@dataclass
class Received:
    """Linha recebida da serial, já interpretada.

    kind: "packet" (pacote JSON), "crc" (quadro descartado pelo rádio),
    "raw" (quadro que não é JSON) ou "log" (texto de depuração).
    """

    kind: str
    packet: dict | None = None
    meta: dict = field(default_factory=dict)  # rssi/snr/ms da ponte, se houver.
    text: str = ""


def encode_tc(seq: int, cmd: str, args: dict | None = None) -> bytes:
    """Monta a linha de um TC, terminada em '\\n'."""
    tc = {"t": "tc", "seq": seq, "cmd": cmd}
    if args:
        tc["args"] = args
    line = json.dumps(tc, separators=(",", ":"))
    if len(line.encode()) > MAX_FRAME_LEN:
        raise ValueError(f"TC com {len(line)} bytes excede {MAX_FRAME_LEN}")
    return (line + "\n").encode()


def parse_line(line: str) -> Received:
    """Interpreta uma linha da ponte da estação terrena ou do umbilical.

    Ponte: {"gs":{...},"pkt":{...}} / {"gs":{...},"raw":"hex"} / {"gs":{...},"err":"crc"}.
    Umbilical: o próprio pacote. Linhas sem '{' inicial são logs.
    """
    line = line.strip()
    if not line.startswith("{"):
        return Received("log", text=line)

    try:
        obj = json.loads(line)
    except json.JSONDecodeError:
        return Received("log", text=line)
    if not isinstance(obj, dict):
        return Received("log", text=line)

    if "gs" not in obj:
        return Received("packet", packet=obj)

    meta = obj.get("gs") or {}
    if "pkt" in obj and isinstance(obj["pkt"], dict):
        return Received("packet", packet=obj["pkt"], meta=meta)
    if obj.get("err") == "crc":
        return Received("crc", meta=meta)
    return Received("raw", meta=meta, text=str(obj.get("raw", "")))


class LossTracker:
    """Conta perdas pelos saltos do contador "n" (16 bits, com volta).

    Salto para trás grande (mais de meia volta) é tratado como reinício do
    satélite, sem perdas.
    """

    def __init__(self) -> None:
        self.last: int | None = None
        self.received = 0
        self.lost = 0
        self.duplicates = 0
        self.restarts = 0

    def feed(self, n: int) -> int:
        """Registra um contador recebido e devolve quantos pacotes faltaram."""
        self.received += 1
        if self.last is None:
            self.last = n
            return 0

        delta = (n - self.last) % COUNTER_MODULO
        if delta == 0:
            self.duplicates += 1
            return 0
        self.last = n
        if delta >= COUNTER_MODULO // 2:
            self.restarts += 1
            return 0
        self.lost += delta - 1
        return delta - 1

    @property
    def loss_rate(self) -> float:
        total = self.received - self.duplicates + self.lost
        return self.lost / total if total else 0.0

    def as_dict(self) -> dict:
        return {
            "received": self.received,
            "lost": self.lost,
            "duplicates": self.duplicates,
            "restarts": self.restarts,
            "loss_rate": self.loss_rate,
        }
