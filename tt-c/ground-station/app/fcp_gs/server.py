"""Interface web da estação terrena: painel ao vivo (SSE), envio de TCs e roteiros de teste."""

import json
import queue
import threading
from pathlib import Path

from flask import Flask, Response, jsonify, request, send_from_directory

from .link import Link
from .protocol import COMMANDS
from .tiles import TileCache
from .verification import run_tc_latency, run_tm_loss, summarize, write_report

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


def create_app(link: Link, report_dir: Path,
               map_center: tuple[float, float, int] = DEFAULT_MAP_CENTER,
               tile_cache: TileCache | None = None) -> Flask:
    app = Flask(__name__, static_folder=None)
    broadcaster = Broadcaster()
    link.add_listener(broadcaster.publish)
    test_lock = threading.Lock()

    def run_test(name: str, count: int) -> None:
        try:
            report = run_tm_loss(link, count) if name == "tm-loss" else run_tc_latency(link, count)
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
                        "commands": sorted(COMMANDS)})

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
        defaults = {"tm-loss": 1000, "tc-latency": 50}
        if name not in defaults:
            return jsonify({"error": f"teste desconhecido: {name}"}), 400
        count = int(body.get("count") or defaults[name])
        if not test_lock.acquire(blocking=False):
            return jsonify({"error": "outro teste em andamento"}), 409
        threading.Thread(target=run_test, args=(name, count), name=f"test-{name}", daemon=True).start()
        return jsonify({"started": name, "count": count}), 202

    @app.post("/api/inject")
    def inject():
        """Bancada: processa um quadro fictício como se tivesse chegado pelo enlace.

        Body: {"packet": {...}} (um objeto JSON do contrato) ou
        {"line": "..."} (a linha cruza de ponte ou umbilical). Uso local;
        o servidor só escuta em 127.0.0.1.
        """
        body = request.get_json(silent=True)
        if isinstance(body, dict) and isinstance(body.get("packet"), dict):
            line = json.dumps(body["packet"], separators=(",", ":"))
        elif isinstance(body, dict) and isinstance(body.get("line"), str):
            line = body["line"]
        else:
            return jsonify({"error": "envie {'packet': {}} ou {'line': ''}"}), 400
        link.handle_line(line + "\n")
        return jsonify({"ok": True})

    return app
