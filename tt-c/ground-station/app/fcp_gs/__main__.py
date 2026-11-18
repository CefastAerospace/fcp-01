"""Linha de comando da estação terrena.

    python -m fcp_gs ports
    python -m fcp_gs serve --port /dev/ttyUSB0 [--wired] [--http 8080] [--map-center LAT,LON[,ZOOM]]
    python -m fcp_gs serve --demo [--scenario NOME] [--map-center LAT,LON[,ZOOM]]
    python -m fcp_gs tiles --bbox S,W,N,E [--zoom 4-11]
    python -m fcp_gs tm-loss --port /dev/ttyUSB0 [--count 1000] [--wired]
    python -m fcp_gs tc-latency --port /dev/ttyUSB0 [--count 50] [--wired]
    python -m fcp_gs dedup --port /dev/ttyUSB0 [--repeats 3] [--wired]
    python -m fcp_gs delivery --port /dev/ttyUSB0 [--crash | --resume] [--wired]
    python -m fcp_gs congestion --port /dev/ttyUSB0 [--count 200] [--wired]
    python -m fcp_gs health --port /dev/ttyUSB0 --wired [--duration 120]
    python -m fcp_gs inject [--port /dev/ttyUSB0] [--file quadros.jsonl] [--delay 0.05]
    python -m fcp_gs analyze logs/2026-10-14_120000.jsonl
    python -m fcp_gs logs list --port /dev/ttyUSB0 [--wired]
    python -m fcp_gs logs get NOME [NOME ...] | --all --port /dev/ttyUSB0 [--out DIR] [--wired]
"""

import argparse
import json
import sys
import time
from pathlib import Path

from serial.tools import list_ports

from .analyze import analyze
from .deliveries import DeliveryStore
from .demo import DEFAULT_SCENARIO, SCENARIOS
from .link import Link, open_serial
from .logs import LogError, download_file, list_files
from .session_log import SessionLog
from .verification import (run_congestion, run_dedup, run_delivery, run_health, run_tc_latency, run_tm_loss,
                           summarize, write_report)

BASE_DIR = Path(__file__).resolve().parent.parent
LOG_DIR = BASE_DIR / "logs"
REPORT_DIR = BASE_DIR / "reports"
TILE_DIR = BASE_DIR / "cache" / "tiles"
DELIVERY_FILE = BASE_DIR / "state" / "deliveries.jsonl"  # IDs já processados, entre sessões.
DELIVERY_PENDING_FILE = BASE_DIR / "state" / "delivery-check.json"  # ID retido por "delivery --crash".
DOWNLOAD_DIR = BASE_DIR / "downloads"
DEFAULT_MAP_CENTER = "-15.79,-47.88,4"  # Brasília.


def _map_center(text: str) -> tuple[float, float, int]:
    parts = text.split(",")
    try:
        if len(parts) not in (2, 3):
            raise ValueError
        lat, lon = float(parts[0]), float(parts[1])
        zoom = int(parts[2]) if len(parts) == 3 else 9
    except ValueError:
        raise argparse.ArgumentTypeError("use LAT,LON ou LAT,LON,ZOOM (ex.: -15.79,-47.88,9)")
    if not (-90 <= lat <= 90 and -180 <= lon <= 180 and 0 <= zoom <= 17):
        raise argparse.ArgumentTypeError("latitude, longitude ou zoom fora do intervalo")
    return lat, lon, zoom


def _bbox(text: str) -> tuple[float, float, float, float]:
    try:
        south, west, north, east = (float(v) for v in text.split(","))
    except ValueError:
        raise argparse.ArgumentTypeError("use SUL,OESTE,NORTE,LESTE (ex.: -16.2,-48.2,-15.4,-47.3)")
    if not (-90 <= south < north <= 90 and -180 <= west < east <= 180):
        raise argparse.ArgumentTypeError("bbox inválido: sul < norte e oeste < leste")
    return south, west, north, east


def _zoom_range(text: str) -> tuple[int, int]:
    try:
        low, _, high = text.partition("-")
        zmin, zmax = int(low), int(high or low)
    except ValueError:
        raise argparse.ArgumentTypeError("use ZMIN-ZMAX (ex.: 4-11)")
    if not 0 <= zmin <= zmax <= 17:
        raise argparse.ArgumentTypeError("zoom entre 0 e 17, ZMIN <= ZMAX")
    return zmin, zmax


def _open_link(args: argparse.Namespace) -> Link:
    if args.demo:
        from .demo import DemoSatellite

        center = getattr(args, "map_center", None) or _map_center(DEFAULT_MAP_CENTER)
        stream = DemoSatellite(center=center[:2], wired=args.wired, scenario=args.scenario)
    else:
        if args.port is None:
            print("--port é obrigatório sem --demo.", file=sys.stderr)
            sys.exit(2)
        stream = open_serial(args.port, args.baud)
    # O demo gera IDs próprios a cada execução: não mistura com o registro real.
    deliveries = DeliveryStore() if args.demo else DeliveryStore(DELIVERY_FILE)
    link = Link(stream, "wired" if args.wired else "rf", SessionLog(LOG_DIR), deliveries)
    link.start()
    if not args.demo:
        print(f"Log da sessão: {link.log.path}")
    return link


def _cmd_ports(_: argparse.Namespace) -> int:
    for port in list_ports.comports():
        print(f"{port.device}\t{port.description}")
    return 0


def _cmd_serve(args: argparse.Namespace) -> int:
    from .server import create_app  # Flask só é necessário aqui.

    from .tiles import TileCache

    link = _open_link(args)
    from .server import LOOPBACK_HOSTS

    if args.host in ("0.0.0.0", "::"):
        # Acesso pela rede foi pedido explicitamente: qualquer Host é aceito.
        print("ATENÇÃO: painel acessível pela rede, sem autenticação (TCs incluídos).", file=sys.stderr)
        allowed_hosts = None
    else:
        allowed_hosts = LOOPBACK_HOSTS | {args.host}
    app = create_app(link, REPORT_DIR, map_center=args.map_center, tile_cache=TileCache(TILE_DIR),
                     allowed_hosts=allowed_hosts, demo=link.stream if args.demo else None)
    if args.demo:
        print(f"Demo, cenário {args.scenario}: {SCENARIOS[args.scenario].description}")
    print(f"Abra http://localhost:{args.http}")
    try:
        app.run(host=args.host, port=args.http, threaded=True)
    finally:
        link.close()
    return 0


def _cmd_test(args: argparse.Namespace) -> int:
    target = None
    if args.command == "delivery" and args.resume:
        try:
            target = tuple(json.loads(DELIVERY_PENDING_FILE.read_text(encoding="utf-8"))["id"])
        except (OSError, ValueError, KeyError, TypeError):
            print(f"Sem ID pendente em {DELIVERY_PENDING_FILE}: rode antes 'delivery --crash'.", file=sys.stderr)
            return 2
        if args.demo:
            print("--resume não vale no --demo: o registro do demo fica só na memória.", file=sys.stderr)
            return 2
    link = _open_link(args)
    try:
        if args.command == "tm-loss":
            report = run_tm_loss(link, args.count)
        elif args.command == "tc-latency":
            report = run_tc_latency(link, args.count)
        elif args.command == "dedup":
            report = run_dedup(link, args.repeats)
        elif args.command == "congestion":
            report = run_congestion(link, args.count)
        elif args.command == "health":
            report = run_health(link, args.duration)
        else:
            report = run_delivery(link, target=target, stop_after_withhold=args.crash)
    finally:
        link.close()
    if args.command == "delivery" and report["phase"] == "crash" and report["passed"]:
        DELIVERY_PENDING_FILE.parent.mkdir(parents=True, exist_ok=True)
        DELIVERY_PENDING_FILE.write_text(json.dumps({"id": report["id"]}), encoding="utf-8")
        print("ID gravado e cf retido. Reinicie a estação com 'delivery --resume' "
              f"antes de o OBC desistir; ID em {DELIVERY_PENDING_FILE}.")
    elif args.command == "delivery" and report["phase"] == "resume" and report["passed"]:
        DELIVERY_PENDING_FILE.unlink(missing_ok=True)
    path = write_report(report, REPORT_DIR)
    print(summarize(report))
    print(f"Relatório: {path}")
    return 0 if report["passed"] else 1


def _cmd_analyze(args: argparse.Namespace) -> int:
    print(json.dumps(analyze(Path(args.log)), indent=2, ensure_ascii=False))
    return 0


def _cmd_inject(args: argparse.Namespace) -> int:
    """Manda linhas JSON na serial, como se a ponte/satélite as recebesse."""
    frames = args.file.read().splitlines() if args.file else sys.stdin.read().splitlines()
    frames = [line.strip() for line in frames if line.strip()]
    if not frames:
        print("Nada para injetar (entrada vazia).", file=sys.stderr)
        return 1
    link = _open_link(args)
    try:
        for line in frames:
            link.stream.write((line + "\n").encode())
            link.stream.flush()
            print(f"→ {line}")
            time.sleep(max(args.delay, 0.0))
    finally:
        link.close()
    return 0


def _cmd_logs(args: argparse.Namespace) -> int:
    """request_log: lista ou extrai arquivos fechados do SD do OBC."""
    link = _open_link(args)
    try:
        if args.action == "list":
            for item in list_files(link):
                print(f"{item['name']}\t{item['size']}\t{'aberto' if item['open'] else 'fechado'}")
            return 0
        names = args.names
        if args.all:
            names = [item["name"] for item in list_files(link) if not item["open"]]
        if not names:
            print("Informe NOME ou --all.", file=sys.stderr)
            return 2
        for name in names:
            path = download_file(link, name, args.out)
            print(f"{name}: {path.stat().st_size} bytes em {path}")
        return 0
    except LogError as error:
        print(f"request_log: {error}", file=sys.stderr)
        return 1
    finally:
        link.close()


def _cmd_tiles(args: argparse.Namespace) -> int:
    from .tiles import TileCache

    zmin, zmax = args.zoom

    def progress(result: dict) -> None:
        done = result["cached"] + result["fetched"] + result["failed"]
        print(f"\r{done}/{result['total']} folhas", end="", flush=True)

    try:
        result = TileCache(TILE_DIR).prefetch(args.bbox, zmin, zmax, progress=progress)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 2
    print(f"\nNo cache: {result['cached']} · baixadas: {result['fetched']} · falhas: {result['failed']}"
          f"\nPasta: {TILE_DIR}")
    return 1 if result["failed"] else 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="fcp_gs", description="Estação terrena do FCP-01")
    commands = parser.add_subparsers(dest="command", required=True)

    commands.add_parser("ports", help="lista as portas seriais").set_defaults(func=_cmd_ports)

    def with_port(sub: argparse.ArgumentParser) -> argparse.ArgumentParser:
        sub.add_argument("--port", help="porta da ponte (RF) ou do satélite (umbilical)")
        sub.add_argument("--baud", type=int, default=115200)
        sub.add_argument("--wired", action="store_true", help="conexão direta ao satélite (umbilical)")
        sub.add_argument("--demo", action="store_true", help="satélite simulado na memória, sem hardware")
        sub.add_argument("--scenario", choices=list(SCENARIOS), default=DEFAULT_SCENARIO,
                         help="cenário do --demo (troca ao vivo pela interface): "
                              + "; ".join(f"{s.name}: {s.title}" for s in SCENARIOS.values()))
        return sub

    serve = with_port(commands.add_parser("serve", help="interface web"))
    serve.add_argument("--http", type=int, default=8080)
    serve.add_argument("--host", default="127.0.0.1")
    serve.add_argument("--map-center", type=_map_center, default=_map_center(DEFAULT_MAP_CENTER),
                       help="centro inicial do mapa: LAT,LON[,ZOOM] (padrão: Brasília)")
    serve.set_defaults(func=_cmd_serve)

    tm_loss = with_port(commands.add_parser("tm-loss", help="REQ-TTC-003: perda em rajada de TMs"))
    tm_loss.add_argument("--count", type=int, default=1000)
    tm_loss.set_defaults(func=_cmd_test)

    inject_cmd = with_port(commands.add_parser("inject", help="injeta linhas JSON fictícias na serial"))
    inject_cmd.add_argument("--file", type=argparse.FileType("r"), help="arquivo com uma linha JSON por quadro; padrão: stdin")
    inject_cmd.add_argument("--delay", type=float, default=0.05, help="segundos entre quadros")
    inject_cmd.set_defaults(func=_cmd_inject)

    tc_latency = with_port(commands.add_parser("tc-latency", help="REQ-TTC-004 / HLR-COMM-01: latência de TCs"))
    tc_latency.add_argument("--count", type=int, default=50)
    tc_latency.set_defaults(func=_cmd_test)

    dedup_cmd = with_port(commands.add_parser("dedup", help="deduplicação: mesmo seq sem reexecução"))
    dedup_cmd.add_argument("--repeats", type=int, default=3)
    dedup_cmd.set_defaults(func=_cmd_test)

    delivery_cmd = with_port(commands.add_parser(
        "delivery", help="entrega com cf perdido: retransmissão, reconfirmação e processamento único"))
    phase = delivery_cmd.add_mutually_exclusive_group()
    phase.add_argument("--crash", action="store_true", help="grava o ID, retém o cf e encerra (queda da estação)")
    phase.add_argument("--resume", action="store_true", help="depois do --crash: confere a reconfirmação")
    delivery_cmd.set_defaults(func=_cmd_test)

    congestion_cmd = with_port(commands.add_parser(
        "congestion", help="prioridade do ACK com a fila de descida cheia (rajada test_tm)"))
    congestion_cmd.add_argument("--count", type=int, default=200)
    congestion_cmd.set_defaults(func=_cmd_test)

    health_cmd = with_port(commands.add_parser("health", help="verificação de saúde: rst sobe ao desligar o SX1276"))
    health_cmd.add_argument("--duration", type=float, default=120.0, help="segundos de acompanhamento")
    health_cmd.set_defaults(func=_cmd_test)

    tiles_cmd = commands.add_parser("tiles", help="pré-carrega folhas do mapa para uso sem internet")
    tiles_cmd.add_argument("--bbox", type=_bbox, required=True, help="SUL,OESTE,NORTE,LESTE")
    tiles_cmd.add_argument("--zoom", type=_zoom_range, default=(4, 11), help="ZMIN-ZMAX (padrão 4-11)")
    tiles_cmd.set_defaults(func=_cmd_tiles)

    logs_cmd = with_port(commands.add_parser("logs", help="request_log: lista e extrai arquivos do SD do OBC"))
    logs_cmd.add_argument("action", choices=("list", "get"))
    logs_cmd.add_argument("names", nargs="*", help="arquivos fechados a extrair (get)")
    logs_cmd.add_argument("--all", action="store_true", help="extrai todos os arquivos fechados (get)")
    logs_cmd.add_argument("--out", type=Path, default=DOWNLOAD_DIR, help="pasta de destino (get)")
    logs_cmd.set_defaults(func=_cmd_logs)

    analyze_cmd = commands.add_parser("analyze", help="análise pós-passagem de um log .jsonl")
    analyze_cmd.add_argument("log")
    analyze_cmd.set_defaults(func=_cmd_analyze)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
