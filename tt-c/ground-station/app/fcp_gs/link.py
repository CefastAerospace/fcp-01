"""Conexão serial com a ponte da estação terrena (RF) ou com o satélite (umbilical)."""

import math
import threading
import time
from collections import deque
from collections.abc import Callable

import serial

from .deliveries import DeliveryStore
from .protocol import (COUNTER_MODULO, DeliveryId, LossTracker, Received, delivery_id, encode_confirmation,
                       encode_tc, parse_line)
from .session_log import SessionLog

Listener = Callable[[dict], None]


OPEN_SETTLE_S = 1.5    # Boot do ESP32 até "[GS] Pronto." leva ~0,4 s.
OPEN_QUIET_S = 1.0     # Serial sem bytes novos: boot terminado.
OPEN_MAX_WAIT_S = 6.0
TRACK_POINTS = 200     # Posições guardadas por aeronave para o rastro no mapa.
LOG_RESPONSES_KEPT = 64  # Respostas "lg" guardadas à espera de request_log().


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
    deliveries: IDs já processados; sem ele, um conjunto só na memória.
    """

    def __init__(self, stream, source: str = "rf", log: SessionLog | None = None,
                 deliveries: DeliveryStore | None = None) -> None:
        self.stream = stream
        self.source = source
        self.log = log
        self.deliveries = deliveries if deliveries is not None else DeliveryStore()
        self.delivery_counts = {"new": 0, "repeated": 0, "confirmations": 0, "confirm_errors": 0}
        self.loss = LossTracker()
        self.counts: dict[str, int] = {}
        self.latest_tm: dict | None = None
        self.aircraft: dict[str, dict] = {}
        self.tracks: dict[str, deque] = {}  # ICAO -> [[lat, lon, alt, ts], ...]
        self.last_meta: dict = {}

        self._listeners: list[Listener] = []
        self._acks: dict[int, dict] = {}
        self._log_responses: dict[int, dict] = {}
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
        self.deliveries.close()

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
        delivery, repeated = self._classify_delivery(received)
        self._update_state(received, repeated)
        event = self._event(received)
        if delivery is not None:
            event["delivery"] = {"id": list(delivery), "repeated": repeated}
        self._emit(event)  # Grava o pacote no log da sessão antes de confirmar.
        if delivery is not None:
            self._confirm_delivery(delivery, repeated)
        # O ACK só é entregue a send_tc() depois dos listeners: quem remove o
        # listener ao receber o ACK (ex.: tm-loss) ainda vê o "n" dele.
        self._deliver_ack(received)

    def _classify_delivery(self, received: Received) -> tuple[DeliveryId | None, bool]:
        """ID da entrega de aeronave e se já foi processada (inclusive em sessão anterior)."""
        packet = received.packet
        if received.kind != "packet" or packet is None or packet.get("t") != "ac":
            return None, False
        delivery = delivery_id(packet)
        if delivery is None:
            return None, False
        return delivery, delivery in self.deliveries

    def _confirm_delivery(self, delivery: DeliveryId, repeated: bool) -> None:
        """Registra o ID com fsync e confirma; repetida só recebe a confirmação de novo."""
        try:
            if not repeated and not self.deliveries.record(delivery):
                repeated = True  # Outra thread (ex.: /api/inject) registrou antes.
        except OSError as error:
            # Sem registro durável, não confirma: o OBC retransmite depois.
            with self._cond:
                self.delivery_counts["confirm_errors"] += 1
            self._emit({"dir": "log", "kind": "error", "text": f"registro de entregas: {error}"})
            return
        with self._cond:
            self.delivery_counts["repeated" if repeated else "new"] += 1
            self.delivery_counts["confirmations"] += 1
        self._write(encode_confirmation(delivery),
                    {"dir": "tx", "kind": "cf", "link": self.source, "pkt": {"t": "cf", "id": list(delivery)}})

    def _update_state(self, received: Received, repeated: bool = False) -> None:
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
            if repeated:
                return  # Entrega já processada: só a confirmação é repetida.
            if kind == "tm":
                self.latest_tm = packet
            elif kind == "ac" and "icao" in packet:
                icao = str(packet["icao"])
                self.aircraft[icao] = packet
                self._extend_track(icao, packet)

    def _extend_track(self, icao: str, packet: dict) -> None:
        """Acrescenta a posição ao rastro, ignorando inválidas e repetidas."""
        lat, lon = packet.get("lat"), packet.get("lon")
        if not all(isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)
                   for v in (lat, lon)):
            return
        if not (-90 <= lat <= 90 and -180 <= lon <= 180):
            return
        track = self.tracks.setdefault(icao, deque(maxlen=TRACK_POINTS))
        if track and track[-1][0] == lat and track[-1][1] == lon:
            return
        track.append([lat, lon, packet.get("alt"), packet.get("ts")])

    def _deliver_ack(self, received: Received) -> None:
        packet = received.packet
        if received.kind != "packet" or packet is None:
            return
        if packet.get("t") == "lg" and isinstance(packet.get("rq"), int):
            # A resposta pode chegar antes do ACK: guarda até request_log() pedir.
            with self._cond:
                self._log_responses[packet["rq"]] = packet
                while len(self._log_responses) > LOG_RESPONSES_KEPT:
                    self._log_responses.pop(next(iter(self._log_responses)))
                self._cond.notify_all()
            return
        if packet.get("t") != "ack" or not isinstance(packet.get("seq"), int):
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
            self._log_responses.pop(seq, None)
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

    def request_log(self, args: dict, attempts: int = 3, timeout: float = 3.0) -> dict:
        """Envia request_log e espera a resposta "lg" do mesmo pedido.

        Resposta perdida (ACK sem "lg"): repete o mesmo pedido com seq novo,
        pois o seq repetido só recebe o ACK guardado, sem nova execução.
        Devolve {"ok": True, "response": {...}} ou {"ok": False, "status": ...}.
        """
        result: dict = {"ok": False, "status": "timeout"}
        for _ in range(attempts):
            ack = self.send_tc("request_log", args, timeout=timeout)
            if not ack["ok"]:
                return {"ok": False, "status": ack["status"], "seq": ack["seq"]}
            response = self._wait_log_response(ack["seq"], timeout)
            if response is not None:
                return {"ok": True, "response": response, "seq": ack["seq"]}
            result = {"ok": False, "status": "no_response", "seq": ack["seq"]}
        return result

    def _wait_log_response(self, seq: int, timeout: float) -> dict | None:
        deadline = time.monotonic() + timeout
        with self._cond:
            while seq not in self._log_responses:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                self._cond.wait(remaining)
            return self._log_responses.pop(seq)

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

    def tracks_snapshot(self) -> dict[str, list]:
        with self._cond:
            return {icao: list(track) for icao, track in self.tracks.items()}

    def snapshot(self) -> dict:
        with self._cond:
            return {
                "source": self.source,
                "counts": dict(self.counts),
                "loss": self.loss.as_dict(),
                "latest_tm": self.latest_tm,
                "aircraft": list(self.aircraft.values()),
                "deliveries": {**self.delivery_counts, "known": len(self.deliveries)},
                "last_meta": self.last_meta,
                "log_file": str(self.log.path) if self.log else None,
            }
