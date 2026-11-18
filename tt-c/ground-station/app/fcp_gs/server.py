"""Interface web da estação terrena: painel ao vivo (SSE), envio de TCs e roteiros de teste."""

import json
import queue
import threading
from pathlib import Path

from flask import Flask, Response, jsonify, request, send_from_directory

from .link import Link
from .protocol import COMMANDS
from .tiles import TileCache
from .verification import run_congestion, run_dedup, run_tc_latency, run_tm_loss, summarize, write_report

# Roteiros disponíveis na interface: nome -> (função, valor padrão do campo numérico).
# delivery e health exigem ações do operador (reiniciar a estação, desligar o rádio): só na linha de comando.
TESTS = {
    "tm-loss": (run_tm_loss, 1000),
    "tc-latency": (run_tc_latency, 50),
    "dedup": (run_dedup, 3),
    "congestion": (run_congestion, 200),
}

STATIC_DIR = Path(__file__).parent / "static"
KEEPALIVE_S = 15
DEFAULT_MAP_CENTER = (-15.79, -47.88, 4)  # Brasília; ajuste com serve --map-center.


class Broadcaster:
    """Repassa os eventos do enlace a cada navegador conectado."""

    def __init__(self) -> None:
        self._clients: set[queue.Queue] = set()
        self._lock = threading.Lock()

    def publish(self, event: dict) -> None:
        data = json.dumps(event, separators=(",", ":"), ensure_ascii=False)
        with self._lock:
            clients = list(self._clients)
        for client in clients:
            try:
                client.put_nowait(data)
            except queue.Full:
                pass  # Navegador lento: descarta em vez de travar o enlace.

    def stream(self):
        client: queue.Queue = queue.Queue(maxsize=500)
        with self._lock:
            self._clients.add(client)
        try:
            while True:
                try:
                    yield f"data: {client.get(timeout=KEEPALIVE_S)}\n\n"
                except queue.Empty:
                    yield ": keepalive\n\n"
        finally:
            with self._lock:
                self._clients.discard(client)


LOOPBACK_HOSTS = frozenset({"localhost", "127.0.0.1", "::1"})


def host_name(host: str) -> str:
    """Nome do cabeçalho Host sem a porta ("[::1]:8000" -> "::1")."""
    if host.startswith("["):
        return host[1:].split("]", 1)[0]
    return host.rsplit(":", 1)[0] if host.count(":") == 1 else host


def create_app(link: Link, report_dir: Path,
               map_center: tuple[float, float, int] = DEFAULT_MAP_CENTER,
               tile_cache: TileCache | None = None,
               allowed_hosts: frozenset[str] | None = LOOPBACK_HOSTS,
               demo=None) -> Flask:
    """allowed_hosts: nomes aceitos no cabeçalho Host; None aceita qualquer um.

    demo: o DemoSatellite do `serve --demo`, para trocar o cenário ao vivo.

    O padrão (só loopback) barra DNS rebinding: uma página externa que aponte
    seu domínio para 127.0.0.1 não consegue chamar /api/tc nem /api/inject.
    """
    app = Flask(__name__, static_folder=None)

    @app.before_request
    def check_host():
        if allowed_hosts is not None and host_name(request.host) not in allowed_hosts:
            return Response("Host não permitido", status=403)
        return None

    broadcaster = Broadcaster()
    link.add_listener(broadcaster.publish)
    test_lock = threading.Lock()

    def run_test(name: str, count: int) -> None:
        try:
            report = TESTS[name][0](link, count)
            path = write_report(report, report_dir)
            broadcaster.publish({"dir": "report", "test": name, "passed": report["passed"],
                                 "summary": summarize(report), "path": str(path)})
        except Exception as error:  # O navegador precisa saber que o teste abortou.
            broadcaster.publish({"dir": "report", "test": name, "passed": False,
                                 "summary": f"# {name}: abortado\n\n{error}\n", "path": None})
        finally:
            test_lock.release()

    @app.get("/")
    def index():
        return send_from_directory(STATIC_DIR, "index.html")

    @app.get("/static/<path:name>")
    def static_file(name: str):
        return send_from_directory(STATIC_DIR, name)

    @app.get("/events")
    def events():
        return Response(broadcaster.stream(), mimetype="text/event-stream",
                        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"})

    @app.get("/tiles/<int:z>/<int:x>/<int:y>.png")
    def tile(z: int, x: int, y: int):
        data = tile_cache.get(z, x, y) if tile_cache else None
        if data is None:
            return Response(status=404)  # O contorno offline aparece por baixo.
        return Response(data, mimetype="image/png", headers={"Cache-Control": "max-age=3600"})

    @app.get("/api/config")
    def config():
        lat, lon, zoom = map_center
        return jsonify({"map_center": [lat, lon], "map_zoom": zoom,
                        "commands": sorted(COMMANDS),
                        "demo": demo.describe() if demo is not None else None})

    @app.post("/api/demo/scenario")
    def demo_scenario():
        if demo is None:
            return jsonify({"error": "disponível só no serve --demo"}), 404
        name = (request.get_json(silent=True) or {}).get("name")
        try:
            demo.set_scenario(name)
        except ValueError as error:
            return jsonify({"error": str(error)}), 400
        link.note(f"demo: cenário {name}")
        return jsonify(demo.describe())

    @app.get("/api/tracks")
    def tracks():
        return jsonify(link.tracks_snapshot())

    @app.get("/api/state")
    def state():
        return jsonify(link.snapshot())

    @app.post("/api/tc")
    def telecommand():
        body = request.get_json(silent=True) or {}
        cmd = body.get("cmd")
        args = body.get("args")
        if cmd not in COMMANDS:
            return jsonify({"error": f"comando desconhecido: {cmd}"}), 400
        if args is not None and not isinstance(args, dict):
            return jsonify({"error": "args deve ser um objeto"}), 400
        try:
            return jsonify(link.send_tc(cmd, args))
        except ValueError as error:
            return jsonify({"error": str(error)}), 400

    @app.post("/api/test")
    def start_test():
        body = request.get_json(silent=True) or {}
        name = body.get("test")
        if name not in TESTS:
            return jsonify({"error": f"teste desconhecido: {name}"}), 400
        count = int(body.get("count") or TESTS[name][1])
        if not test_lock.acquire(blocking=False):
            return jsonify({"error": "outro teste em andamento"}), 409
        threading.Thread(target=run_test, args=(name, count), name=f"test-{name}", daemon=True).start()
        return jsonify({"started": name, "count": count}), 202

    @app.post("/api/inject")
    def inject():
        """Bancada: processa um quadro fictício como se tivesse chegado pelo enlace.

        Body: {"packet": {...}} (um objeto JSON do contrato) ou
        {"line": "..."} (a linha cruza de ponte ou umbilical). O quadro só
        atualiza o painel: não confirma entregas ao satélite, não grava IDs
        no registro persistente e não entra na contagem de perdas.
        """
        body = request.get_json(silent=True)
        if isinstance(body, dict) and isinstance(body.get("packet"), dict):
            line = json.dumps(body["packet"], separators=(",", ":"))
        elif isinstance(body, dict) and isinstance(body.get("line"), str):
            line = body["line"]
        else:
            return jsonify({"error": "envie {'packet': {}} ou {'line': ''}"}), 400
        link.handle_line(line + "\n", injected=True)
        return jsonify({"ok": True})

    return app
