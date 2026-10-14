"""Linha de comando da estação terrena.

    python -m fcp_gs ports
    python -m fcp_gs serve --port /dev/ttyUSB0 [--wired] [--http 8080]
    python -m fcp_gs tm-loss --port /dev/ttyUSB0 [--count 1000] [--wired]
    python -m fcp_gs tc-latency --port /dev/ttyUSB0 [--count 50] [--wired]
    python -m fcp_gs analyze logs/2026-10-14_120000.jsonl
"""

import argparse
import json
import sys
from pathlib import Path

from serial.tools import list_ports

from .analyze import analyze
from .link import Link, open_serial
from .session_log import SessionLog
from .verification import run_tc_latency, run_tm_loss, summarize, write_report

BASE_DIR = Path(__file__).resolve().parent.parent
LOG_DIR = BASE_DIR / "logs"
REPORT_DIR = BASE_DIR / "reports"


def _open_link(args: argparse.Namespace) -> Link:
    link = Link(open_serial(args.port, args.baud), "wired" if args.wired else "rf", SessionLog(LOG_DIR))
    link.start()
    print(f"Log da sessão: {link.log.path}")
    return link


def _cmd_ports(_: argparse.Namespace) -> int:
    for port in list_ports.comports():
        print(f"{port.device}\t{port.description}")
    return 0


def _cmd_serve(args: argparse.Namespace) -> int:
    from .server import create_app  # Flask só é necessário aqui.

    link = _open_link(args)
    app = create_app(link, REPORT_DIR)
    print(f"Abra http://localhost:{args.http}")
    try:
        app.run(host=args.host, port=args.http, threaded=True)
    finally:
        link.close()
    return 0


def _cmd_test(args: argparse.Namespace) -> int:
    link = _open_link(args)
    try:
        if args.command == "tm-loss":
            report = run_tm_loss(link, args.count)
        else:
            report = run_tc_latency(link, args.count)
    finally:
        link.close()
    path = write_report(report, REPORT_DIR)
    print(summarize(report))
    print(f"Relatório: {path}")
    return 0 if report["passed"] else 1


def _cmd_analyze(args: argparse.Namespace) -> int:
    print(json.dumps(analyze(Path(args.log)), indent=2, ensure_ascii=False))
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="fcp_gs", description="Estação terrena do FCP-01")
    commands = parser.add_subparsers(dest="command", required=True)

    commands.add_parser("ports", help="lista as portas seriais").set_defaults(func=_cmd_ports)

    def with_port(sub: argparse.ArgumentParser) -> argparse.ArgumentParser:
        sub.add_argument("--port", required=True, help="porta da ponte (RF) ou do satélite (umbilical)")
        sub.add_argument("--baud", type=int, default=115200)
        sub.add_argument("--wired", action="store_true", help="conexão direta ao satélite (umbilical)")
        return sub

    serve = with_port(commands.add_parser("serve", help="interface web"))
    serve.add_argument("--http", type=int, default=8080)
    serve.add_argument("--host", default="127.0.0.1")
    serve.set_defaults(func=_cmd_serve)

    tm_loss = with_port(commands.add_parser("tm-loss", help="REQ-TTC-003: perda em rajada de TMs"))
    tm_loss.add_argument("--count", type=int, default=1000)
    tm_loss.set_defaults(func=_cmd_test)

    tc_latency = with_port(commands.add_parser("tc-latency", help="REQ-TTC-004 / HLR-COMM-01: latência de TCs"))
    tc_latency.add_argument("--count", type=int, default=50)
    tc_latency.set_defaults(func=_cmd_test)

    analyze_cmd = commands.add_parser("analyze", help="análise pós-passagem de um log .jsonl")
    analyze_cmd.add_argument("log")
    analyze_cmd.set_defaults(func=_cmd_analyze)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
