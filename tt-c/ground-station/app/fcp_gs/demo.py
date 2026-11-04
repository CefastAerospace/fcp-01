"""Satélite simulado na memória, para a opção --demo do serve.

Emite TM e aeronaves em movimento e responde TCs com ACK como o link-test
na placa. Serve para desenvolver a interface sem serial nem hardware.
"""

import json
import math
import queue
import threading
import time

PERIOD_S = 2.0
BURST_LIMIT = 50

# Rotas circulares ao redor do centro do mapa (Brasília, ajustável).
AIRLINES = [
    {"icao": "E4890D", "lat": -15.79, "lon": -47.88, "radius": 0.05, "period": 90.0,
     "alt": 30000, "vel": 450, "hdg": 90, "phase": 0.00, "cnt": 120},
    {"icao": "E79012", "lat": -15.45, "lon": -48.10, "radius": 0.03, "period": 60.0,
     "alt": 22000, "vel": 380, "hdg": 270, "phase": 0.25, "cnt": 61},
    {"icao": "A3B200", "lat": -16.10, "lon": -47.60, "radius": 0.04, "period": 120.0,
     "alt": 9000, "vel": 220, "hdg": 180, "phase": 0.50, "cnt": 14},
    {"icao": "F51611", "lat": -15.60, "lon": -47.40, "radius": 0.08, "period": 180.0,
     "alt": 500, "vel": 90, "hdg": 45, "phase": 0.75, "cnt": 3},
]


class DemoSatellite:
    def __init__(self) -> None:
        self.lines: queue.Queue[bytes] = queue.Queue()
        self.n = 0
        self.aircraft = [dict(airline) for airline in AIRLINES]
        self.t0 = time.monotonic()
        self._closed = threading.Event()
        self._thread = threading.Thread(target=self._loop, name="demo-satellite", daemon=True)
        self._thread.start()

    def _downlink(self, packet: dict) -> None:
        body = {"n": self.n, **packet}
        self.n += 1
        self.lines.put((json.dumps(body, separators=(",", ":")) + "\n").encode())

    def _tm(self) -> dict:
        seconds = int(time.time())
        return {"t": "tm", "ts": seconds, "md": "bench", "vb": 7.8, "ib": 420,
                "vs": 9.1, "is": 310, "tp": [21.5, 22.0, 23.1, 20.9, 24.3],
                "w": 12.4, "az": 37, "ac": len(self.aircraft), "msg": 1200,
                "rssi": -81, "snr": 9.5, "rx": 10, "err": 0, "rst": 0}

    def _loop(self) -> None:
        while not self._closed.wait(PERIOD_S):
            self._downlink(self._tm())
            seconds = time.monotonic() - self.t0
            for aircraft in self.aircraft:
                angle = 2 * math.pi * (seconds / aircraft["period"] + aircraft["phase"])
                latitude = aircraft["lat"] + aircraft["radius"] * math.sin(angle)
                longitude = aircraft["lon"] + aircraft["radius"] * math.cos(angle)
                self._downlink({
                    "t": "ac", "ts": int(time.time()), "icao": aircraft["icao"],
                    "lat": round(latitude, 5), "lon": round(longitude, 5),
                    "alt": aircraft["alt"], "vel": aircraft["vel"], "hdg": aircraft["hdg"],
                    "cnt": aircraft["cnt"],
                })
                aircraft["cnt"] += 1

    def write(self, data: bytes) -> int:
        try:
            command = json.loads(data)
        except ValueError:
            return len(data)
        sequence = command.get("seq")
        name = command.get("cmd")
        if not isinstance(sequence, int) or not isinstance(name, str):
            return len(data)
        self._downlink({"t": "ack", "seq": sequence, "cmd": name, "st": "ok", "q": 3})
        if name in ("request_tm", "get_status"):
            self._downlink(self._tm())
        if name == "test_tm":
            count = (command.get("args") or {}).get("count", 1)
            for _ in range(min(int(count), BURST_LIMIT)):
                self._downlink(self._tm())
        return len(data)

    def readline(self) -> bytes:
        try:
            return self.lines.get(timeout=0.05)
        except queue.Empty:
            return b""

    def close(self) -> None:
        self._closed.set()
