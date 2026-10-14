"""Roteiros de verificação do TT&C (DP §4.2.4 e §8.3.1), com relatório em reports/."""

import json
import statistics
import threading
import time
from datetime import datetime
from pathlib import Path

from .link import Link
from .protocol import LossTracker

TM_LOSS_LIMIT = 0.01       # REQ-TTC-003: perda < 1,0%.
TC_FORWARD_LIMIT_MS = 200  # REQ-TTC-004: entrega ao OBC em até 200 ms.
TC_END_TO_END_MS = 2000    # HLR-COMM-01 / REQ-OBC-002: até 2 s.


def _stats(values: list[float]) -> dict:
    if not values:
        return {"count": 0}
    ordered = sorted(values)
    p95 = ordered[min(len(ordered) - 1, int(round(0.95 * (len(ordered) - 1))))]
    return {"count": len(values), "min": ordered[0], "mean": round(statistics.fmean(values), 1),
            "p95": p95, "max": ordered[-1]}


def run_tm_loss(link: Link, count: int = 1000, idle_timeout: float = 5.0) -> dict:
    """REQ-TTC-003: pede uma rajada de `count` TMs (test_tm) e mede a perda pelo contador "n".

    Um ping no final fecha a contagem: o "n" do ACK revela perdas no fim da rajada.
    """
    tracker = LossTracker()
    state = {"tm": 0, "crc": 0, "last_rx": time.monotonic()}
    lock = threading.Lock()

    def listener(event: dict) -> None:
        if event.get("dir") != "rx":
            return
        with lock:
            state["last_rx"] = time.monotonic()
            if event.get("kind") == "crc":
                state["crc"] += 1
            packet = event.get("pkt") or {}
            if isinstance(packet.get("n"), int):
                tracker.feed(packet["n"])
            if packet.get("t") == "tm":
                state["tm"] += 1

    started = datetime.now().isoformat(timespec="seconds")
    t0 = time.monotonic()
    link.add_listener(listener)
    try:
        request = link.send_tc("test_tm", {"count": count})
        if request["ok"]:
            # Espera a rajada terminar: todas as TMs ou silêncio prolongado.
            deadline = t0 + 30 + count * 0.6
            while time.monotonic() < deadline:
                with lock:
                    done = state["tm"] >= count or time.monotonic() - state["last_rx"] > idle_timeout
                if done:
                    break
                time.sleep(0.1)
            closing = link.send_tc("ping")
        else:
            closing = None
    finally:
        link.remove_listener(listener)

    with lock:
        loss = tracker.as_dict()
        report = {
            "test": "tm-loss",
            "requirement": "REQ-TTC-003",
            "source": link.source,
            "started": started,
            "duration_s": round(time.monotonic() - t0, 1),
            "requested": count,
            "request_ack": request,
            "closing_ack": closing,
            "tm_received": state["tm"],
            "crc_errors": state["crc"],
            **loss,
        }
    report["passed"] = bool(request["ok"] and loss["received"] > 0 and loss["loss_rate"] < TM_LOSS_LIMIT)
    return report


def _latency_command(index: int) -> tuple[str, dict | None]:
    """Sequência de TCs distintos alternando tipos de comando."""
    choice = index % 4
    if choice == 0:
        return "ping", None
    if choice == 1:
        return "request_tm", None
    if choice == 2:
        return "set_time", {"unix": int(time.time())}
    return "set_mode", {"mode": "bench"}


def run_tc_latency(link: Link, count: int = 50, gap_s: float = 0.2) -> dict:
    """REQ-TTC-004 e HLR-COMM-01: envia `count` TCs e mede "q" (bordo) e ida e volta."""
    started = datetime.now().isoformat(timespec="seconds")
    t0 = time.monotonic()
    results = []
    for index in range(count):
        cmd, args = _latency_command(index)
        results.append(link.send_tc(cmd, args))
        time.sleep(gap_s)

    acked = [r for r in results if r["status"] != "timeout"]
    queue_ms = [r["q"] for r in acked if isinstance(r["q"], (int, float))]
    rtt_ms = [r["rtt_ms"] for r in acked if r["rtt_ms"] is not None]
    report = {
        "test": "tc-latency",
        "requirement": "REQ-TTC-004, HLR-COMM-01",
        "source": link.source,
        "started": started,
        "duration_s": round(time.monotonic() - t0, 1),
        "sent": count,
        "acked": len(acked),
        "status_ok": sum(1 for r in acked if r["ok"]),
        "retransmissions": sum(r["attempts"] - 1 for r in results),
        "forward_ms": _stats(queue_ms),
        "end_to_end_ms": _stats(rtt_ms),
        "results": results,
    }
    report["passed"] = bool(
        len(acked) == count
        and len(queue_ms) == count
        and max(queue_ms, default=0) <= TC_FORWARD_LIMIT_MS
        and max(rtt_ms, default=0) <= TC_END_TO_END_MS
    )
    return report


def write_report(report: dict, directory: Path) -> Path:
    """Grava o relatório em JSON e um resumo em Markdown; devolve o caminho do JSON."""
    directory.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y-%m-%d_%H%M%S")
    path = directory / f"{report['test']}_{stamp}.json"
    path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    path.with_suffix(".md").write_text(summarize(report), encoding="utf-8")
    return path


def summarize(report: dict) -> str:
    """Resumo legível do relatório."""
    verdict = "APROVADO" if report["passed"] else "REPROVADO"
    lines = [f"# {report['test']} ({report['requirement']}): {verdict}", "",
             f"- Início: {report['started']}, duração {report['duration_s']} s, meio: {report['source']}"]
    if report["test"] == "tm-loss":
        lines += [
            f"- TMs pedidas: {report['requested']}, recebidas: {report['tm_received']}",
            f"- Pacotes recebidos: {report['received']}, perdidos: {report['lost']}, "
            f"taxa de perda: {report['loss_rate'] * 100:.2f}% (limite {TM_LOSS_LIMIT * 100:.1f}%)",
            f"- Quadros com CRC inválido na estação terrena: {report['crc_errors']}",
        ]
    else:
        forward, e2e = report["forward_ms"], report["end_to_end_ms"]
        lines += [
            f"- TCs enviados: {report['sent']}, confirmados: {report['acked']}, "
            f"status ok: {report['status_ok']}, retransmissões: {report['retransmissions']}",
            f"- Entrega ao OBC (q): máx {forward.get('max')} ms, p95 {forward.get('p95')} ms "
            f"(limite {TC_FORWARD_LIMIT_MS} ms)",
            f"- Ida e volta: máx {e2e.get('max')} ms, média {e2e.get('mean')} ms (limite {TC_END_TO_END_MS} ms)",
        ]
    return "\n".join(lines) + "\n"
