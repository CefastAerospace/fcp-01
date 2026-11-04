"""Satélite simulado na memória, para `serve --demo` (e testes) sem hardware.

Emite TM e aeronaves em voo e responde TCs como o OBC faria, para exercitar
toda a interface:

- aeronaves em rotas retas, com rumo coerente com o deslocamento; quem sai da
  área é trocado por uma nova (um avião faz órbita de espera);
- o tempo das aeronaves corre SPEED_UP vezes mais rápido, para o rastro
  crescer em segundos no mapa;
- `set_mode`, `enter_safe`, `exit_safe`, `start_mission`, `stop_mission`,
  `adcs_start`, `adcs_stop` e `set_time` mudam a TM seguinte;
- `args` inválidos geram NACK `invalid_param`; `start_mission` em safe gera
  `state_rejected`;
- `deploy_antenna` gera o evento `antenna_deployed`; `reset_obc` reinicia o
  contador `n` (o painel conta como reinício);
- em safe, beacon a cada BEACON_PERIOD_S;
- no modo RF (padrão), cada quadro vem no envelope `gs` da ponte, com RSSI e
  SNR variando; com wired=True, sem envelope, como no umbilical.
"""

import json
import math
import queue
import random
import threading
import time

PERIOD_S = 2.0
BEACON_PERIOD_S = 10.0
BURST_LIMIT = 50
SPEED_UP = 20.0             # Aceleração do voo das aeronaves.
AREA_RADIUS_DEG = 1.5       # Raio da área de cobertura simulada.
ACTIVE_AIRCRAFT = 6
KT_TO_DEG_S = 1.852 / 3600 / 111.32
MODES = ("bench", "safe", "detumb", "sun", "mission", "lowpwr")
COUNTER_MODULO = 65536


class _Aircraft:
    def __init__(self, rng: random.Random, center: tuple[float, float], orbit: bool = False) -> None:
        self.icao = f"{rng.randrange(0xE00000, 0xF00000):06X}"
        self.orbit = orbit
        self.cnt = rng.randrange(1, 50)
        lat0, lon0 = center
        if orbit:
            # Órbita de espera perto do centro, como em aproximação.
            self.center = (lat0 + rng.uniform(-0.3, 0.3), lon0 + rng.uniform(-0.3, 0.3))
            self.radius = 0.12
            self.angle = rng.uniform(0, 2 * math.pi)
            self.alt, self.vel = rng.choice((6000, 8000, 10000)), 210
            self.lat, self.lon, self.hdg = self.center[0], self.center[1], 0.0
            self._advance_orbit(0.0)
        else:
            # Entra pela borda, rumo ao centro com desvio de até 40°.
            bearing = rng.uniform(0, 2 * math.pi)
            self.lat = lat0 + AREA_RADIUS_DEG * math.cos(bearing)
            self.lon = lon0 + AREA_RADIUS_DEG * math.sin(bearing) / math.cos(math.radians(lat0))
            inbound = (math.degrees(bearing) + 180 + rng.uniform(-40, 40)) % 360
            self.hdg = inbound
            self.alt = rng.choice((3500, 12000, 24000, 33000, 37000, 39000))
            self.vel = rng.randrange(180, 300) if self.alt < 10000 else rng.randrange(380, 490)

    def _advance_orbit(self, dt: float) -> None:
        step = self.vel * KT_TO_DEG_S * dt / self.radius   # Velocidade angular (rad).
        self.angle = (self.angle + step) % (2 * math.pi)
        self.lat = self.center[0] + self.radius * math.cos(self.angle)
        self.lon = self.center[1] + self.radius * math.sin(self.angle) / math.cos(math.radians(self.center[0]))
        self.hdg = (math.degrees(self.angle) + 90) % 360   # Tangente, sentido horário.

    def advance(self, dt: float) -> None:
        if self.orbit:
            self._advance_orbit(dt)
            return
        distance = self.vel * KT_TO_DEG_S * dt
        self.lat += distance * math.cos(math.radians(self.hdg))
        self.lon += distance * math.sin(math.radians(self.hdg)) / math.cos(math.radians(self.lat))

    def outside(self, center: tuple[float, float]) -> bool:
        dlat = self.lat - center[0]
        dlon = (self.lon - center[1]) * math.cos(math.radians(center[0]))
        return math.hypot(dlat, dlon) > AREA_RADIUS_DEG * 1.05


class DemoSatellite:
    def __init__(self, center: tuple[float, float] = (-15.79, -47.88), wired: bool = False,
                 seed: int | None = None) -> None:
        self.center = (float(center[0]), float(center[1]))
        self.wired = wired
        self.lines: queue.Queue[bytes] = queue.Queue()
        self.n = 0
        self.mode = "bench"
        self.mode_before_safe = "bench"
        self.time_offset = 0
        self.adcs = False
        self.rate = 12.4            # Taxa angular (°/s).
        self.messages = 0
        self.rx = 0
        self.rng = random.Random(seed)
        self.aircraft = [_Aircraft(self.rng, self.center, orbit=(i == 0)) for i in range(ACTIVE_AIRCRAFT)]
        # Espalha as aeronaves ao longo das rotas, para o mapa não começar vazio no centro.
        for aircraft in self.aircraft[1:]:
            aircraft.advance(self.rng.uniform(0, 400))
        self.t0 = time.monotonic()
        self._lock = threading.Lock()
        self._closed = threading.Event()
        self._thread = threading.Thread(target=self._loop, name="demo-satellite", daemon=True)
        self._thread.start()

    # ─── Descida ──────────────────────────────────────────────────────

    def _now(self) -> int:
        return int(time.time()) + self.time_offset

    def _downlink(self, packet: dict) -> None:
        with self._lock:
            body = {"n": self.n, **packet}
            self.n = (self.n + 1) % COUNTER_MODULO
            if self.wired:
                line = body
            else:
                elapsed = time.monotonic() - self.t0
                line = {"gs": {"rssi": round(-85 + 6 * math.sin(elapsed / 30) + self.rng.uniform(-2, 2)),
                               "snr": round(8 + 2 * math.sin(elapsed / 30) + self.rng.uniform(-1, 1), 1),
                               "ms": int(elapsed * 1000)},
                        "pkt": body}
            self.lines.put((json.dumps(line, separators=(",", ":")) + "\n").encode())

    def _tm(self) -> dict:
        elapsed = time.monotonic() - self.t0
        # Bateria em ciclo de órbita (~95 min comprimido em 5 min), temperaturas acompanham.
        phase = math.sin(2 * math.pi * elapsed / 300)
        sunlit = phase > -0.3
        return {"t": "tm", "ts": self._now(), "md": self.mode,
                "vb": round(7.6 + 0.3 * phase, 2), "ib": round(420 - 300 * phase),
                "vs": round(9.1 if sunlit else 0.4, 2), "is": round(310 * max(phase, 0)),
                "tp": [round(22 + 8 * phase + offset, 1) for offset in (-0.5, 0, 1.1, -1.1, 2.3)],
                "w": round(self.rate, 1), "az": round((elapsed * 3) % 360),
                "ac": len(self.aircraft), "msg": self.messages,
                "rssi": -81, "snr": 9.5, "rx": self.rx, "err": 0, "rst": 0}

    def _loop(self) -> None:
        last = time.monotonic()
        last_beacon = 0.0
        while not self._closed.wait(PERIOD_S):
            now = time.monotonic()
            dt = (now - last) * SPEED_UP
            last = now
            self.rate = max(0.3, self.rate * 0.9) if self.adcs else min(15.0, self.rate + 0.05)
            self._downlink(self._tm())
            if self.mode == "safe" and now - last_beacon >= BEACON_PERIOD_S:
                last_beacon = now
                self._downlink({"t": "bcn", "ts": self._now(), "md": "safe", "vb": 7.2})
            for index, aircraft in enumerate(self.aircraft):
                aircraft.advance(dt)
                if aircraft.outside(self.center):
                    self.aircraft[index] = aircraft = _Aircraft(self.rng, self.center)
                received = self.rng.randrange(5, 30)
                aircraft.cnt += received
                self.messages += received
                self._downlink({
                    "t": "ac", "ts": self._now(), "icao": aircraft.icao,
                    "lat": round(aircraft.lat, 5), "lon": round(aircraft.lon, 5),
                    "alt": aircraft.alt, "vel": aircraft.vel, "hdg": round(aircraft.hdg),
                    "cnt": aircraft.cnt,
                })

    # ─── Subida (TCs) ─────────────────────────────────────────────────

    def _execute(self, name: str, args: dict) -> str:
        """Executa o TC como o OBC e devolve o status do ACK."""
        if name == "set_mode":
            mode = args.get("mode")
            if mode not in MODES:
                return "invalid_param"
            if mode == "safe" and self.mode != "safe":
                self.mode_before_safe = self.mode
            self.mode = mode
        elif name == "enter_safe":
            if self.mode != "safe":
                self.mode_before_safe = self.mode
            self.mode = "safe"
        elif name == "exit_safe":
            if self.mode not in ("safe", "lowpwr"):
                return "state_rejected"
            self.mode = self.mode_before_safe if self.mode_before_safe not in ("safe", "lowpwr") else "bench"
        elif name == "start_mission":
            if self.mode in ("safe", "lowpwr"):
                return "state_rejected"
            self.mode = "mission"
        elif name == "stop_mission":
            if self.mode != "mission":
                return "state_rejected"
            self.mode = "bench"
        elif name in ("adcs_start", "adcs_stop"):
            self.adcs = name == "adcs_start"
        elif name == "set_time":
            unix = args.get("unix")
            if not isinstance(unix, int) or isinstance(unix, bool) or unix < 1_600_000_000:
                return "invalid_param"
            self.time_offset = unix - int(time.time())
        elif name == "test_tm":
            count = args.get("count", 1)
            if not isinstance(count, int) or isinstance(count, bool) or not 1 <= count <= 2000:
                return "invalid_param"
        return "ok"

    def write(self, data: bytes) -> int:
        try:
            command = json.loads(data)
        except ValueError:
            return len(data)
        if not isinstance(command, dict):
            return len(data)
        sequence = command.get("seq")
        name = command.get("cmd")
        args = command.get("args") or {}
        if not isinstance(sequence, int) or not isinstance(name, str):
            return len(data)
        self.rx += 1
        status = self._execute(name, args) if isinstance(args, dict) else "invalid_param"
        self._downlink({"t": "ack", "seq": sequence, "cmd": name, "st": status, "q": 3})
        if status != "ok":
            return len(data)
        if name in ("request_tm", "get_status"):
            self._downlink(self._tm())
        elif name == "test_tm":
            for _ in range(min(args.get("count", 1), BURST_LIMIT)):
                self._downlink(self._tm())
        elif name == "deploy_antenna":
            self._downlink({"t": "ev", "ts": self._now(), "ev": "antenna_deployed"})
        elif name == "reset_obc":
            with self._lock:
                self.n = 0      # Reinício: o contador recomeça.
            self.mode, self.adcs, self.time_offset = "bench", False, 0
        return len(data)

    def readline(self) -> bytes:
        try:
            return self.lines.get(timeout=0.05)
        except queue.Empty:
            return b""

    def flush(self) -> None:
        pass

    def close(self) -> None:
        self._closed.set()
