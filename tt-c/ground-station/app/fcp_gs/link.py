"""Conexão serial com a ponte da estação terrena (RF) ou com o satélite (umbilical)."""

import threading
import time
from collections.abc import Callable

import serial

from .protocol import COUNTER_MODULO, LossTracker, Received, encode_tc, parse_line
from .session_log import SessionLog

Listener = Callable[[dict], None]


OPEN_SETTLE_S = 1.5    # Boot do ESP32 até "[GS] Pronto." leva ~0,4 s.
OPEN_QUIET_S = 1.0     # Serial sem bytes novos: boot terminado.
OPEN_MAX_WAIT_S = 6.0


def open_serial(port: str, baud: int = 115200, settle_s: float = OPEN_SETTLE_S,
                quiet_s: float = OPEN_QUIET_S, max_wait_s: float = OPEN_MAX_WAIT_S) -> serial.Serial:
    """Abre a porta com DTR/RTS desligados e espera o ESP32 ficar pronto.

    No Linux, o open() aciona DTR/RTS antes de a configuração valer, e a placa
    pode reiniciar mesmo assim, às vezes mais de 1 s depois da abertura. Um TC
    enviado durante o boot se perde; por isso a porta só é devolvida após
    settle_s e quiet_s sem bytes novos (até max_wait_s). A entrada não é
    descartada: as mensagens do boot seguem para o log da sessão.
    """
    stream = serial.Serial()
    stream.port = port
    stream.baudrate = baud
    stream.timeout = 0.1
    stream.dtr = False
    stream.rts = False
    stream.open()

    start = last_change = time.monotonic()
    waiting = stream.in_waiting
    while True:
        time.sleep(0.05)
        now = time.monotonic()
        if stream.in_waiting != waiting:
            waiting = stream.in_waiting
            last_change = now
        if now - start >= max_wait_s:
            break
        if now - start >= settle_s and now - last_change >= quiet_s:
            break
    return stream


class Link:
    """Lê pacotes em uma thread, envia TCs com retentativa e mantém o estado do enlace.

    source: "rf" (ponte ESP32 + SX1276) ou "wired" (USB do satélite, umbilical).
    """

    def __init__(self, stream, source: str = "rf", log: SessionLog | None = None) -> None:
        self.stream = stream
        self.source = source
        self.log = log
        self.loss = LossTracker()
        self.counts: dict[str, int] = {}
        self.latest_tm: dict | None = None
        self.aircraft: dict[str, dict] = {}
        self.last_meta: dict = {}

        self._listeners: list[Listener] = []
        self._acks: dict[int, dict] = {}
        self._cond = threading.Condition()
        self._write_lock = threading.Lock()
        self._next_seq = int(time.time()) % COUNTER_MODULO  # Não repete seq entre sessões.
        self._running = False
        self._thread: threading.Thread | None = None

    # ─── Ciclo de vida ────────────────────────────────────────────────

    def start(self) -> None:
        self._running = True
        self._thread = threading.Thread(target=self._read_loop, name="gs-reader", daemon=True)
        self._thread.start()

    def close(self) -> None:
        self._running = False
        if self._thread is not None:
            self._thread.join(timeout=1.0)
        self.stream.close()

    def add_listener(self, listener: Listener) -> None:
        with self._cond:
            self._listeners.append(listener)

    def remove_listener(self, listener: Listener) -> None:
        with self._cond:
            if listener in self._listeners:
                self._listeners.remove(listener)

    # ─── Recepção ─────────────────────────────────────────────────────

    def _read_loop(self) -> None:
        while self._running:
            try:
                raw = self.stream.readline()
            except serial.SerialException as error:
                self._emit({"dir": "log", "kind": "error", "text": f"serial: {error}"})
                time.sleep(1.0)
                continue
            if raw:
                self.handle_line(raw.decode("utf-8", errors="replace"))

    def handle_line(self, line: str) -> None:
        """Interpreta e distribui uma linha (público para testes e reprodução)."""
        received = parse_line(line)
        if received.kind == "log" and not received.text:
            return
        self._update_state(received)
        self._emit(self._event(received))
        # O ACK só é entregue a send_tc() depois dos listeners: quem remove o
        # listener ao receber o ACK (ex.: tm-loss) ainda vê o "n" dele.
        self._deliver_ack(received)

    def _update_state(self, received: Received) -> None:
        with self._cond:
            self.counts[received.kind] = self.counts.get(received.kind, 0) + 1
            if received.meta:
                self.last_meta = received.meta
            packet = received.packet
            if received.kind != "packet" or packet is None:
                return

            kind = str(packet.get("t", "?"))
            self.counts[f"t:{kind}"] = self.counts.get(f"t:{kind}", 0) + 1
            if isinstance(packet.get("n"), int):
                self.loss.feed(packet["n"])
            if kind == "tm":
                self.latest_tm = packet
            elif kind == "ac" and "icao" in packet:
                self.aircraft[str(packet["icao"])] = packet

    def _deliver_ack(self, received: Received) -> None:
        packet = received.packet
        if received.kind != "packet" or packet is None or packet.get("t") != "ack":
            return
        if not isinstance(packet.get("seq"), int):
            return
        with self._cond:
            self._acks[packet["seq"]] = packet
            self._cond.notify_all()

    def _event(self, received: Received) -> dict:
        event: dict = {"dir": "log" if received.kind == "log" else "rx", "kind": received.kind, "link": self.source}
        if received.packet is not None:
            event["pkt"] = received.packet
        if received.meta:
            event["gs"] = received.meta
        if received.text:
            event["text"] = received.text
        return event

    def _emit(self, event: dict) -> None:
        if self.log is not None:
            self.log.write(event)
        with self._cond:
            listeners = list(self._listeners)
        for listener in listeners:
            listener(event)

    # ─── Envio ────────────────────────────────────────────────────────

    def send_tc(self, cmd: str, args: dict | None = None, retries: int = 3, timeout: float = 2.0) -> dict:
        """Envia um TC e espera o ACK, retransmitindo com o mesmo seq.

        rtt_ms mede do primeiro envio ao ACK (ponta a ponta, HLR-COMM-01).
        """
        with self._cond:
            seq = self._next_seq
            self._next_seq = (seq + 1) % COUNTER_MODULO
            self._acks.pop(seq, None)
        line = encode_tc(seq, cmd, args)

        first_send = time.monotonic()
        for attempt in range(1, retries + 1):
            self._write(line, {"dir": "tx", "kind": "tc", "link": self.source, "attempt": attempt,
                               "pkt": {"t": "tc", "seq": seq, "cmd": cmd, **({"args": args} if args else {})}})
            ack = self._wait_ack(seq, timeout)
            if ack is not None:
                rtt_ms = (time.monotonic() - first_send) * 1000
                return {"seq": seq, "cmd": cmd, "ok": ack.get("st") == "ok", "status": ack.get("st"),
                        "q": ack.get("q"), "rtt_ms": round(rtt_ms, 1), "attempts": attempt}

        return {"seq": seq, "cmd": cmd, "ok": False, "status": "timeout", "q": None,
                "rtt_ms": None, "attempts": retries}

    def _write(self, data: bytes, event: dict) -> None:
        with self._write_lock:
            self.stream.write(data)
            self.stream.flush()
        self._emit(event)

    def _wait_ack(self, seq: int, timeout: float) -> dict | None:
        deadline = time.monotonic() + timeout
        with self._cond:
            while seq not in self._acks:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                self._cond.wait(remaining)
            return self._acks.pop(seq)

    # ─── Estado ───────────────────────────────────────────────────────

    def snapshot(self) -> dict:
        with self._cond:
            return {
                "source": self.source,
                "counts": dict(self.counts),
                "loss": self.loss.as_dict(),
                "latest_tm": self.latest_tm,
                "aircraft": list(self.aircraft.values()),
                "last_meta": self.last_meta,
                "log_file": str(self.log.path) if self.log else None,
            }
