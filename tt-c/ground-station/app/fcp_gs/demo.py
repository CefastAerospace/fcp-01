"""Satélite simulado na memória, para `serve --demo` (e testes) sem hardware.

Emite TM e aeronaves em voo e responde TCs como o OBC faria, para exercitar
toda a interface. O comportamento base:

- aeronaves em rotas retas, com rumo coerente com o deslocamento; quem sai da
  área é trocado por uma nova (algumas fazem órbita de espera);
- o tempo das aeronaves corre SPEED_UP vezes mais rápido, para o rastro
  crescer em segundos no mapa;
- `set_mode`, `enter_safe`, `exit_safe`, `start_mission`, `stop_mission`,
  `adcs_start`, `adcs_stop` e `set_time` mudam a TM seguinte;
- `args` inválidos geram NACK `invalid_param`; `start_mission` em safe gera
  `state_rejected`;
- TC repetido (mesmo `seq`) recebe o ACK anterior, sem reexecutar;
- `deploy_antenna` gera o evento `antenna_deployed`; `reset_obc` reinicia o
  contador `n` (o painel conta como reinício);
- em safe, beacon a cada BEACON_PERIOD_S;
- no modo RF (padrão), cada quadro vem no envelope `gs` da ponte, com RSSI e
  SNR; com wired=True, sem envelope, como no umbilical;
- cada aeronave sai como entrega identificada ("id":[boot,seq]), sem
  velocidade, rumo e contagem a cada PARTIAL_EVERY entregas (null); sem
  confirmação "cf" em RETRY_S, a mesma entrega é retransmitida.

Os cenários (SCENARIOS) variam isso para mostrar situações diferentes:
enlace ruim, passagem com perda de sinal, bateria baixa, superaquecimento,
reinícios, dados parciais, o firmware de bancada atual do OBC, tráfego denso
e céu vazio. O cenário é escolhido em `serve --demo --scenario NOME` e pode
ser trocado ao vivo pela interface (POST /api/demo/scenario).
"""

import json
import math
import queue
import random
import threading
import time
from collections import OrderedDict
from dataclasses import dataclass

PERIOD_S = 2.0
BEACON_PERIOD_S = 10.0
BURST_LIMIT = 50
SPEED_UP = 20.0             # Aceleração do voo das aeronaves.
AREA_RADIUS_DEG = 1.5       # Raio da área de cobertura simulada.
KT_TO_DEG_S = 1.852 / 3600 / 111.32
MODES = ("bench", "safe", "detumb", "sun", "mission", "lowpwr")
COUNTER_MODULO = 65536
RETRY_S = 6.0               # Retransmissão de entrega sem confirmação.
PENDING_LIMIT = 32          # Como o cache do OBC.
PARTIAL_EVERY = 5           # Entregas com campos indisponíveis (null).
ACK_MEMORY = 8              # Deduplicação de TCs, como o TT&C.

LOW_BATTERY_V = 6.6         # Abaixo disso o OBC entra em LOW-POWER.
RECOVERED_BATTERY_V = 7.0   # exit_safe só é aceito a partir daqui.
OVERTEMP_C = 60.0           # Acima disso o OBC entra em SAFE.
BENCH_COMMANDS = ("ping", "get_status", "request_tm", "set_time")


@dataclass(frozen=True)
class Scenario:
    name: str
    title: str
    description: str
    aircraft: int = 6               # Aeronaves simultâneas.
    holdings: int = 1               # Quantas delas em órbita de espera.
    rssi: float = -85.0             # RSSI médio no solo (dBm).
    snr: float = 8.0                # SNR médio no solo (dB).
    loss: float = 0.0               # Fração de quadros de descida perdidos (só RF).
    crc: float = 0.0                # Fração com CRC inválido na ponte (só RF).
    raw: float = 0.0                # Fração de quadros corrompidos não-JSON (só RF).
    uplink_loss: float = 0.0        # Fração de TCs e "cf" que não chegam (só RF).
    contact_s: float | None = None  # Passagem: segundos com contato...
    gap_s: float = 0.0              # ...seguidos de segundos sem contato (só RF).
    battery_drain: bool = False     # Eclipse longo: bateria cai até LOW-POWER.
    overheat: bool = False          # Uma placa aquece até forçar SAFE.
    reboot_s: float | None = None   # OBC reinicia sozinho a cada N segundos.
    partial: float = 0.0            # Fração de campos da TM e posições indisponíveis.
    bench_obc: bool = False         # Firmware de bancada INT-004/INT-006 do OBC.
    start_mode: str = "bench"


SCENARIOS: dict[str, Scenario] = {s.name: s for s in (
    Scenario("nominal", "Nominal",
             "Enlace bom, energia e temperaturas em ciclo de órbita, 6 aeronaves."),
    Scenario("trafego-denso", "Tráfego denso",
             "25 aeronaves, 3 em órbita de espera: mapa e tabela cheios.",
             aircraft=25, holdings=3),
    Scenario("sem-aeronaves", "Céu vazio",
             "Nenhuma aeronave recebida: mapa e tabela vazios, TM normal.",
             aircraft=0, holdings=0),
    Scenario("enlace-ruim", "Enlace ruim",
             "RSSI perto da sensibilidade: 8% de perda, CRC inválido, quadros corrompidos "
             "e 15% dos TCs sem resposta (o solo retenta com o mesmo seq).",
             rssi=-114.0, snr=-6.0, loss=0.08, crc=0.03, raw=0.01, uplink_loss=0.15),
    Scenario("passagem", "Passagem",
             "90 s de contato com o RSSI subindo e descendo com a elevação, depois 45 s "
             "sem sinal: TCs ficam sem ACK e a perda aparece pelo salto do n.",
             contact_s=90.0, gap_s=45.0),
    Scenario("bateria-baixa", "Bateria baixa",
             "Eclipse longo: a bateria cai até 6,6 V, o OBC entra em LOW-POWER (evento "
             "low_power) e para a missão; exit_safe só é aceito acima de 7,0 V.",
             battery_drain=True),
    Scenario("superaquecimento", "Superaquecimento",
             "A temperatura 3 sobe até 60 °C, o OBC entra em SAFE (evento overtemp) e "
             "passa a mandar beacon; esfria em SAFE até exit_safe.",
             overheat=True),
    Scenario("reinicios", "Reinícios",
             "O OBC reinicia a cada 60 s: n volta a 0, boot novo nos IDs, rst sobe e o "
             "horário fica null até um set_time.",
             reboot_s=60.0),
    Scenario("dados-parciais", "Dados parciais",
             "Sensores falhando: campos da TM e posições de aeronaves chegam como null "
             "(a aeronave entra na tabela, mas não no mapa).",
             partial=0.35),
    Scenario("bancada-obc", "Firmware de bancada do OBC",
             "Como o firmware integrado hoje: TM só sob demanda, energia e sensores null, "
             "ts null até set_time e só ping/get_status/request_tm/set_time aceitos "
             "(demais: invalid_cmd).",
             bench_obc=True),
)}
DEFAULT_SCENARIO = "nominal"


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
    """clock e start existem para os testes avançarem o tempo à mão (_tick)."""

    def __init__(self, center: tuple[float, float] = (-15.79, -47.88), wired: bool = False,
                 seed: int | None = None, scenario: str = DEFAULT_SCENARIO,
                 clock=time.monotonic, start: bool = True) -> None:
        self.center = (float(center[0]), float(center[1]))
        self.wired = wired
        self.clock = clock
        self.lines: queue.Queue[bytes] = queue.Queue()
        self.rng = random.Random(seed)
        self.n = 0
        self.rx = 0
        self.rst = 0
        self.messages = 0
        self.rate = 12.4            # Taxa angular (°/s).
        self.adcs = False
        self.boot = int(time.time())  # Boot novo a cada execução do demo.
        self.sequence = 0
        self.pending: dict[tuple[int, int], list] = {}  # ID -> [pacote, último envio].
        self.confirmed = 0
        self.acks: OrderedDict[int, dict] = OrderedDict()  # seq -> ACK, deduplicação.
        self.uplink_rf = False      # Já chegou TC pelo RF (bancada: RSSI/SNR deixam de ser null).
        self.dropped = {"loss": 0, "crc": 0, "raw": 0, "uplink": 0, "no_contact": 0}
        self._lock = threading.Lock()         # n, IDs, pendências e ACKs.
        self._state_lock = threading.RLock()  # _tick contra troca de cenário ao vivo.
        self.set_scenario(scenario)
        # Espalha as aeronaves ao longo das rotas, para o mapa não começar vazio no centro.
        for aircraft in self.aircraft:
            if not aircraft.orbit:
                aircraft.advance(self.rng.uniform(0, 400))
        self._closed = threading.Event()
        self._thread = threading.Thread(target=self._loop, name="demo-satellite", daemon=True)
        if start:
            self._thread.start()

    # ─── Cenário ──────────────────────────────────────────────────────

    def set_scenario(self, name: str) -> None:
        """Troca o cenário e recomeça o estado que ele controla."""
        if name not in SCENARIOS:
            raise ValueError(f"cenário desconhecido: {name}; opções: {', '.join(SCENARIOS)}")
        scenario = SCENARIOS[name]
        now = self.clock()
        with self._state_lock:
            self.scenario = scenario
            self.t0 = now
            self.last_tick = now
            self.last_beacon = -math.inf
            self.last_reboot = now
            self.mode = scenario.start_mode
            self.mode_before_safe = "bench"
            # Bancada: sem horário até o primeiro set_time (ts null).
            self.time_offset = None if scenario.bench_obc else 0
            self.battery = 7.4
            self.heat = 0.0
            self.overtemp_reported = False
            self.aircraft = [_Aircraft(self.rng, self.center, orbit=(i < scenario.holdings))
                             for i in range(scenario.aircraft)]
            self.last_snapshot = None if scenario.bench_obc else len(self.aircraft)

    def describe(self) -> dict:
        return {"current": self.scenario.name,
                "scenarios": [{"name": s.name, "title": s.title, "description": s.description}
                              for s in SCENARIOS.values()]}

    # ─── Descida ──────────────────────────────────────────────────────

    def _now(self) -> int | None:
        return None if self.time_offset is None else int(time.time()) + self.time_offset

    def in_contact(self, now: float | None = None) -> bool:
        """Passagem: contato nos primeiros contact_s de cada ciclo (só RF)."""
        scenario = self.scenario
        if self.wired or scenario.contact_s is None:
            return True
        elapsed = (self.clock() if now is None else now) - self.t0
        return elapsed % (scenario.contact_s + scenario.gap_s) < scenario.contact_s

    def _signal(self) -> tuple[float, float]:
        """RSSI e SNR no solo; na passagem acompanham a elevação."""
        scenario = self.scenario
        elapsed = self.clock() - self.t0
        if scenario.contact_s is not None:
            arc = math.sin(math.pi * (elapsed % (scenario.contact_s + scenario.gap_s)) / scenario.contact_s)
            rssi, snr = -121 + 41 * max(arc, 0), -8 + 18 * max(arc, 0)
        else:
            rssi = scenario.rssi + 6 * math.sin(elapsed / 30)
            snr = scenario.snr + 2 * math.sin(elapsed / 30)
        return rssi + self.rng.uniform(-2, 2), snr + self.rng.uniform(-1, 1)

    def _downlink(self, packet: dict) -> None:
        scenario = self.scenario
        with self._lock:
            body = {"n": self.n, **packet}
            self.n = (self.n + 1) % COUNTER_MODULO
            if self.wired:
                self.lines.put((json.dumps(body, separators=(",", ":")) + "\n").encode())
                return
            # O "n" já foi consumido: o que se perde aqui aparece como salto no solo.
            if not self.in_contact():
                self.dropped["no_contact"] += 1
                return
            if self.rng.random() < scenario.loss:
                self.dropped["loss"] += 1
                return
            rssi, snr = self._signal()
            gs = {"rssi": round(rssi), "snr": round(snr, 1), "ms": int((self.clock() - self.t0) * 1000)}
            draw = self.rng.random()
            if draw < scenario.crc:
                self.dropped["crc"] += 1
                line = {"gs": gs, "err": "crc"}
            elif draw < scenario.crc + scenario.raw:
                self.dropped["raw"] += 1
                text = json.dumps(body, separators=(",", ":")).encode()
                cut = self.rng.randrange(1, len(text))
                line = {"gs": gs, "raw": (text[:cut] + bytes([text[cut] ^ 0x5A]) + text[cut + 1:]).hex()}
            else:
                line = {"gs": gs, "pkt": body}
            self.lines.put((json.dumps(line, separators=(",", ":")) + "\n").encode())

    def _maybe_null(self, value):
        return None if self.rng.random() < self.scenario.partial else value

    def _tm(self) -> dict:
        scenario = self.scenario
        elapsed = self.clock() - self.t0
        if scenario.bench_obc:
            # INT-004: só modo e contadores; RSSI/SNR depois do primeiro TC pelo RF.
            rssi, snr = (self._signal() if self.uplink_rf else (None, None))
            return {"t": "tm", "ts": self._now(), "md": self.mode, "vb": None, "ib": None,
                    "vs": None, "is": None, "tp": None, "w": None, "az": None,
                    "ac": self.last_snapshot, "msg": None,
                    "rssi": None if rssi is None else round(rssi), "snr": None if snr is None else round(snr, 1),
                    "rx": self.rx, "err": 0, "rst": self.rst}
        # Ciclo de órbita (~95 min comprimido em 5 min).
        phase = self._phase(self.clock())
        if scenario.battery_drain:
            vb, ib = self.battery, (-150 if self.mode == "lowpwr" else 520)
            sunlit = self.mode == "lowpwr"  # Recarrega depois de cortar as cargas.
        else:
            vb, ib = 7.6 + 0.3 * phase, 420 - 300 * phase
            sunlit = phase > -0.3
        temps = [22 + 8 * phase + offset for offset in (-0.5, 0, 1.1, -1.1, 2.3)]
        temps[2] += self.heat
        rssi, snr = self._signal()
        tm = {"t": "tm", "ts": self._now(), "md": self.mode,
              "vb": round(vb, 2), "ib": round(ib),
              "vs": round(9.1 if sunlit else 0.4, 2), "is": round(310 * max(phase, 0.2) if sunlit else 0),
              "tp": [round(t, 1) for t in temps],
              "w": round(self.rate, 1), "az": round((elapsed * 3) % 360),
              "ac": len(self.aircraft) if self.mode != "lowpwr" else 0, "msg": self.messages,
              "rssi": round(rssi), "snr": round(snr, 1), "rx": self.rx, "err": 0, "rst": self.rst}
        if scenario.partial:
            for key in ("vb", "ib", "vs", "is", "tp", "w", "az", "rssi", "snr"):
                tm[key] = self._maybe_null(tm[key])
        return tm

    def _event(self, name: str) -> None:
        self._downlink({"t": "ev", "ts": self._now(), "ev": name})

    def _reboot(self) -> None:
        """Reinício do OBC: n e seq zerados, boot novo, horário perdido."""
        with self._lock:
            self.n = 0
            self.boot += 1
            self.sequence = 0
            self.pending.clear()
            self.acks.clear()
        self.rst += 1
        self.mode, self.adcs = "bench", False
        self.time_offset = None

    def _loop(self) -> None:
        while not self._closed.wait(PERIOD_S):
            self._tick()

    def _tick(self, now: float | None = None) -> None:
        """Um período: dinâmica do cenário, TM, beacon, retransmissões e aeronaves."""
        with self._state_lock:
            self._tick_locked(self.clock() if now is None else now)

    def _phase(self, now: float) -> float:
        return math.sin(2 * math.pi * (now - self.t0) / 300)

    def _tick_locked(self, now: float) -> None:
        scenario = self.scenario
        dt = (now - self.last_tick) * SPEED_UP
        self.last_tick = now

        if scenario.reboot_s and now - self.last_reboot >= scenario.reboot_s:
            self.last_reboot = now
            self._reboot()

        self.rate = max(0.3, self.rate * 0.9) if self.adcs else min(15.0, self.rate + 0.05)

        if scenario.battery_drain:
            if self.mode == "lowpwr":
                self.battery = min(7.6, self.battery + 0.02)
            else:
                self.battery = max(6.2, self.battery - 0.03)
                if self.battery < LOW_BATTERY_V:
                    self.mode_before_safe = self.mode
                    self.mode = "lowpwr"
                    self._event("low_power")

        if scenario.overheat:
            if self.mode == "safe":
                self.heat = max(0.0, self.heat - 1.0)
            else:
                self.heat = min(50.0, self.heat + 1.5)
                hottest = 22 + 8 * self._phase(now) + 1.1 + self.heat  # tp[2] da TM.
                if hottest > OVERTEMP_C and not self.overtemp_reported:
                    self.overtemp_reported = True
                    self.mode_before_safe, self.mode = self.mode, "safe"
                    self._event("overtemp")

        if not scenario.bench_obc:
            self._downlink(self._tm())  # A bancada só manda TM sob demanda.
        if self.mode == "safe" and now - self.last_beacon >= BEACON_PERIOD_S:
            self.last_beacon = now
            self._downlink({"t": "bcn", "ts": self._now(), "md": "safe",
                            "vb": None if scenario.bench_obc else round(self.battery if scenario.battery_drain else 7.2, 2)})
        self._retransmit(now)
        if self.mode == "lowpwr":
            return  # Payload desligado em LOW-POWER: nada de aeronaves.
        for index, aircraft in enumerate(self.aircraft):
            aircraft.advance(dt)
            if aircraft.outside(self.center):
                self.aircraft[index] = aircraft = _Aircraft(self.rng, self.center)
            received = self.rng.randrange(5, 30)
            aircraft.cnt += received
            self.messages += received
            self._deliver(aircraft, now)
        if scenario.bench_obc:
            self.last_snapshot = len(self.aircraft)

    def _deliver(self, aircraft: _Aircraft, now: float) -> None:
        """Nova entrega identificada; algumas sem velocidade, rumo e contagem."""
        with self._lock:
            self.sequence += 1
            delivery = (self.boot, self.sequence)
        scenario = self.scenario
        partial = scenario.bench_obc or delivery[1] % PARTIAL_EVERY == 0
        no_position = scenario.partial and self.rng.random() < scenario.partial
        packet = {
            "t": "ac", "id": list(delivery), "ts": self._now(), "icao": aircraft.icao,
            "lat": None if no_position else round(aircraft.lat, 5),
            "lon": None if no_position else round(aircraft.lon, 5),
            "alt": aircraft.alt,
            "vel": None if partial else aircraft.vel, "hdg": None if partial else round(aircraft.hdg),
            "cnt": None if partial else aircraft.cnt,
        }
        with self._lock:
            self.pending[delivery] = [packet, now]
            while len(self.pending) > PENDING_LIMIT:
                self.pending.pop(next(iter(self.pending)))
        self._downlink(packet)

    def _retransmit(self, now: float) -> None:
        """Mesmo ID, conteúdo e instante original, como o OBC."""
        with self._lock:
            due = [entry for entry in self.pending.values() if now - entry[1] >= RETRY_S]
            for entry in due:
                entry[1] = now
        for packet, _ in due:
            self._downlink(packet)

    # ─── Subida (TCs) ─────────────────────────────────────────────────

    def _execute(self, name: str, args: dict) -> str:
        """Executa o TC como o OBC e devolve o status do ACK."""
        if self.scenario.bench_obc:
            return self._execute_bench(name, args)
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
            if self.mode == "lowpwr" and self.battery < RECOVERED_BATTERY_V:
                return "state_rejected"  # Bateria ainda baixa.
            self.mode = self.mode_before_safe if self.mode_before_safe not in ("safe", "lowpwr") else "bench"
            self.overtemp_reported = False
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

    def _execute_bench(self, name: str, args: dict) -> str:
        """Firmware de bancada (contrato, INT-004/INT-006 primeira etapa)."""
        if name not in BENCH_COMMANDS:
            return "invalid_cmd"
        if name == "set_time":
            unix = args.get("unix")
            if set(args) != {"unix"} or not isinstance(unix, int) or isinstance(unix, bool) \
                    or not 0 <= unix <= 0xFFFFFFFF:
                return "invalid_param"
            self.time_offset = unix - int(time.time())
        elif args:
            return "invalid_param"  # ping/get_status/request_tm: args ausente ou vazio.
        return "ok"

    def _uplink_lost(self) -> bool:
        if self.wired:
            return False
        if not self.in_contact() or self.rng.random() < self.scenario.uplink_loss:
            self.dropped["uplink"] += 1
            return True
        return False

    def write(self, data: bytes) -> int:
        try:
            command = json.loads(data)
        except ValueError:
            return len(data)
        if not isinstance(command, dict):
            return len(data)
        if self._uplink_lost():
            return len(data)
        if command.get("t") == "cf" and isinstance(command.get("id"), list) and len(command["id"]) == 2:
            with self._lock:
                if self.pending.pop(tuple(command["id"]), None) is not None:
                    self.confirmed += 1
            return len(data)
        sequence = command.get("seq")
        name = command.get("cmd")
        args = command.get("args") or {}
        if not isinstance(sequence, int) or not isinstance(name, str):
            return len(data)
        if not self.wired:
            self.uplink_rf = True
        with self._lock:
            previous = self.acks.get(sequence)
        if previous is not None:
            self._downlink(previous)  # Retentativa: mesmo ACK, sem reexecutar.
            return len(data)
        self.rx += 1
        status = self._execute(name, args) if isinstance(args, dict) else "invalid_param"
        ack = {"t": "ack", "seq": sequence, "cmd": name, "st": status, "q": 3}
        with self._lock:
            self.acks[sequence] = ack
            while len(self.acks) > ACK_MEMORY:
                self.acks.popitem(last=False)
        self._downlink(ack)
        if status != "ok":
            return len(data)
        if name in ("request_tm", "get_status"):
            self._downlink(self._tm())
        elif name == "test_tm":
            for _ in range(min(args.get("count", 1), BURST_LIMIT)):
                self._downlink(self._tm())
        elif name == "deploy_antenna":
            self._event("antenna_deployed")
        elif name == "reset_obc":
            self._reboot()
            self.time_offset = 0 if not self.scenario.bench_obc else None
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
