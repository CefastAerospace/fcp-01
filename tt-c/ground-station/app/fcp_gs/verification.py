"""Roteiros de verificação do TT&C (DP §4.2.4 e §8.3.1), com relatório em reports/."""

import json
import statistics
import threading
import time
from datetime import datetime
from pathlib import Path

from .link import Link
from .protocol import COUNTER_MODULO, DeliveryId, LossTracker

TM_LOSS_LIMIT = 0.01       # REQ-TTC-003: perda < 1,0%.
TC_FORWARD_LIMIT_MS = 200  # REQ-TTC-004: entrega ao OBC em até 200 ms.
TC_END_TO_END_MS = 2000    # HLR-COMM-01 / REQ-OBC-002: até 2 s.
BURST_QUIET_S = 2.0        # Pausa que marca o fim da rajada; entre TMs dela, ~0,4 s.
TM_WAIT_S = 3.0            # Espera pela TM pedida com request_tm.
DELIVERY_WAIT_S = 60.0     # Espera pela primeira entrega nova e pela retransmissão.
DELIVERY_SETTLE_S = 15.0   # Depois do cf: tempo sem retransmissão (cobre o timeout de 10 s do OBC).


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
    Sem o ACK desse ping, ou com menos TMs que as pedidas além do limite, as
    perdas do fim da rajada ficariam fora da conta; o ensaio é reprovado.
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
            # Espera a rajada terminar: silêncio prolongado ou, com todas as TMs,
            # uma pausa curta. As TMs periódicas também contam, então a contagem
            # chega antes do fim da rajada; um ping nesse momento colide com ela.
            deadline = t0 + 30 + count * 0.6
            quiet_s = min(BURST_QUIET_S, idle_timeout)
            while time.monotonic() < deadline:
                with lock:
                    silence = time.monotonic() - state["last_rx"]
                    done = silence > idle_timeout or (state["tm"] >= count and silence > quiet_s)
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
    report["tm_missing"] = max(0, count - report["tm_received"])
    report["passed"] = bool(
        request["ok"]
        and closing is not None and closing["status"] != "timeout"
        and loss["received"] > 0
        and loss["loss_rate"] < TM_LOSS_LIMIT
        and count > 0 and report["tm_missing"] / count < TM_LOSS_LIMIT
    )
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


class _Watcher:
    """Listener que guarda os pacotes recebidos com o instante de chegada."""

    def __init__(self, link: Link) -> None:
        self.link = link
        self.packets: list[tuple[float, dict]] = []
        self.cond = threading.Condition()

    def __enter__(self) -> "_Watcher":
        self.link.add_listener(self)
        return self

    def __exit__(self, *_) -> None:
        self.link.remove_listener(self)

    def __call__(self, event: dict) -> None:
        if event.get("dir") != "rx" or not isinstance(event.get("pkt"), dict):
            return
        with self.cond:
            self.packets.append((time.monotonic(), {**event["pkt"], "_delivery": event.get("delivery")}))
            self.cond.notify_all()

    def wait_for(self, match, after: float, timeout: float) -> tuple[float, dict] | None:
        """Primeiro pacote depois de `after` (monotônico) que satisfaz `match`."""
        deadline = time.monotonic() + timeout
        with self.cond:
            while True:
                for stamp, packet in self.packets:
                    if stamp >= after and match(packet):
                        return stamp, packet
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                self.cond.wait(remaining)

    def since(self, after: float, match) -> list[tuple[float, dict]]:
        with self.cond:
            return [(stamp, packet) for stamp, packet in self.packets if stamp >= after and match(packet)]


def _is_tm(packet: dict) -> bool:
    return packet.get("t") == "tm"


def _request_tm(link: Link, watcher: _Watcher) -> tuple[dict, dict | None]:
    """request_tm e a primeira TM recebida depois do envio."""
    sent = time.monotonic()
    ack = link.send_tc("request_tm")
    if not ack["ok"]:
        return ack, None
    found = watcher.wait_for(_is_tm, sent, TM_WAIT_S)
    return ack, found[1] if found else None


def _request_rx(link: Link, watcher: _Watcher) -> tuple[dict, int | None]:
    """request_tm e o contador rx (TCs novos aceitos pelo TT&C) da TM seguinte."""
    ack, tm = _request_tm(link, watcher)
    rx = tm.get("rx") if tm else None
    return ack, rx if isinstance(rx, int) else None


def run_dedup(link: Link, repeats: int = 3) -> dict:
    """Deduplicação: o mesmo seq recebe o ACK guardado e não é reexecutado.

    O rx da TM conta só TCs novos (os repetidos vão para duplicates_count).
    Entre o rx medido depois do primeiro request_tm e o rx medido depois do
    request_tm de fechamento, a diferença tem de ser 1: as repetições não contam.
    """
    started = datetime.now().isoformat(timespec="seconds")
    t0 = time.monotonic()
    with _Watcher(link) as watcher:
        first, rx_before = _request_rx(link, watcher)
        repeat_results = []
        tm_after_repeats = 0
        if first["ok"]:
            mark = time.monotonic()
            for _ in range(repeats):
                repeat_results.append(link.send_tc(first["cmd"], seq=first["seq"], retries=1))
                time.sleep(0.3)
            time.sleep(1.0)
            tm_after_repeats = len(watcher.since(mark, _is_tm))
            closing, rx_after = _request_rx(link, watcher)
        else:
            closing, rx_after = None, None

    rx_delta = None if rx_before is None or rx_after is None else (rx_after - rx_before) % COUNTER_MODULO
    report = {
        "test": "dedup",
        "requirement": "Deduplicação de TCs (contrato TT&C: 8 TCs por 30 s)",
        "source": link.source,
        "started": started,
        "duration_s": round(time.monotonic() - t0, 1),
        "first": first,
        "repeats": repeat_results,
        "closing": closing,
        "rx_before": rx_before,
        "rx_after": rx_after,
        "rx_delta": rx_delta,
        "tm_after_repeats": tm_after_repeats,
    }
    report["passed"] = bool(
        first["ok"] and closing is not None and closing["ok"]
        and len(repeat_results) == repeats
        and all(r["status"] == first["status"] and r["q"] == first["q"] for r in repeat_results)
        and rx_delta == 1
    )
    return report


def _delivery_of(packet: dict) -> DeliveryId | None:
    info = packet.get("_delivery")
    return tuple(info["id"]) if info else None


def _content(packet: dict) -> dict:
    return {key: value for key, value in packet.items() if key not in ("n", "_delivery")}


def run_delivery(link: Link, wait_s: float = DELIVERY_WAIT_S, settle_s: float = DELIVERY_SETTLE_S,
                 target: DeliveryId | None = None, stop_after_withhold: bool = False) -> dict:
    """Entrega com "cf" perdido: retransmissão com o mesmo ID, reconfirmação e processamento único.

    Sem target, retém o "cf" da primeira entrega nova (o ID já fica gravado com
    fsync) e espera a retransmissão. Com stop_after_withhold, encerra logo após
    reter, para simular a queda da estação; a segunda execução, com target,
    confere que a retransmissão é reconhecida como repetida pelo registro
    persistente, reconfirmada e não reprocessada.
    """
    started = datetime.now().isoformat(timespec="seconds")
    t0 = time.monotonic()
    chosen: dict = {"id": target}

    watcher = _Watcher(link)

    def withhold(delivery: DeliveryId, repeated: bool) -> bool:
        if target is None and chosen["id"] is None and not repeated:
            chosen["id"] = delivery
            with watcher.cond:  # O pacote já foi registrado antes do filtro: reavalia a espera.
                watcher.cond.notify_all()
            return False
        return True

    def of_target(packet: dict) -> bool:
        return packet.get("t") == "ac" and chosen["id"] is not None and _delivery_of(packet) == chosen["id"]

    receptions: list[tuple[float, dict]] = []
    link.confirmation_filter = withhold
    try:
        with watcher:
            if target is None:
                first = watcher.wait_for(lambda p: p.get("t") == "ac" and p.get("_delivery") is not None
                                         and not p["_delivery"]["repeated"] and chosen["id"] is not None
                                         and _delivery_of(p) == chosen["id"], t0, wait_s)
                if first is not None and not stop_after_withhold:
                    watcher.wait_for(of_target, first[0] + 0.001, wait_s)
            else:
                watcher.wait_for(of_target, t0, wait_s)
            if chosen["id"] is not None and not stop_after_withhold and watcher.since(t0, of_target):
                time.sleep(settle_s)  # Depois do cf, não pode haver nova retransmissão.
            receptions = watcher.since(t0, of_target)
    finally:
        link.confirmation_filter = None

    stamps = [stamp for stamp, _ in receptions]
    flags = [packet["_delivery"]["repeated"] for _, packet in receptions]
    contents = [_content(packet) for _, packet in receptions]
    phase = "crash" if stop_after_withhold else ("resume" if target is not None else "single")
    report = {
        "test": "delivery",
        "requirement": "INT-006: entrega identificada, cf perdido e reconfirmação",
        "source": link.source,
        "started": started,
        "duration_s": round(time.monotonic() - t0, 1),
        "phase": phase,
        "id": list(chosen["id"]) if chosen["id"] else None,
        "receptions": len(receptions),
        "repeated_flags": flags,
        "same_content": all(c == contents[0] for c in contents) if contents else False,
        "retransmit_after_s": [round(b - a, 2) for a, b in zip(stamps, stamps[1:])],
        "counts": dict(link.delivery_counts),
    }
    if phase == "crash":
        # Gravada e não confirmada: a próxima execução (--resume) espera este ID.
        report["passed"] = bool(len(receptions) == 1 and flags == [False])
    elif phase == "resume":
        # Após o reinício, o registro persistente reconhece a entrega: só reconfirma.
        report["passed"] = bool(len(receptions) == 1 and flags == [True])
    else:
        # Original (cf retido) + uma retransmissão reconhecida como repetida e confirmada.
        report["passed"] = bool(len(receptions) == 2 and flags == [False, True] and report["same_content"])
    return report


def run_congestion(link: Link, count: int = 200, ping_gap_s: float = 1.0, idle_timeout: float = 5.0) -> dict:
    """Prioridade do ACK com a fila de descida cheia (rajada test_tm) e entregas sob congestionamento.

    Sem prioridade, um ping esperaria a fila comum inteira (16 × ~0,5 s em SF7);
    com prioridade, a ida e volta fica no limite de 2 s. As entregas recebidas
    mais de uma vez mostram retransmissões do OBC durante o congestionamento.
    """
    started = datetime.now().isoformat(timespec="seconds")
    t0 = time.monotonic()
    pings = []
    with _Watcher(link) as watcher:
        request = link.send_tc("test_tm", {"count": count})
        if request["ok"]:
            deadline = t0 + 30 + count * 0.6
            while time.monotonic() < deadline:
                pings.append(link.send_tc("ping"))
                time.sleep(ping_gap_s)
                # Fim da rajada: todas as TMs chegaram ou o enlace silenciou.
                if len(watcher.since(t0, _is_tm)) >= count \
                        or not watcher.since(time.monotonic() - idle_timeout, _is_tm):
                    break
        deliveries: dict[DeliveryId, int] = {}
        for _, packet in watcher.since(t0, lambda p: p.get("t") == "ac"):
            delivery = _delivery_of(packet)
            if delivery is not None:
                deliveries[delivery] = deliveries.get(delivery, 0) + 1
        tm_received = len(watcher.since(t0, _is_tm))

    acked = [p for p in pings if p["status"] != "timeout"]
    rtt_ms = [p["rtt_ms"] for p in acked if p["rtt_ms"] is not None]
    report = {
        "test": "congestion",
        "requirement": "Prioridade do ACK e entregas sob congestionamento (TES-008)",
        "source": link.source,
        "started": started,
        "duration_s": round(time.monotonic() - t0, 1),
        "requested": count,
        "request_ack": request,
        "tm_received": tm_received,
        "pings": len(pings),
        "pings_acked": len(acked),
        "ping_rtt_ms": _stats(rtt_ms),
        "retransmissions": sum(p["attempts"] - 1 for p in pings),
        "deliveries": len(deliveries),
        "deliveries_retransmitted": sum(1 for n in deliveries.values() if n > 1),
        "results": pings,
    }
    report["passed"] = bool(
        request["ok"] and pings and len(acked) == len(pings)
        and max(rtt_ms, default=0) <= TC_END_TO_END_MS
    )
    return report


def run_health(link: Link, duration_s: float = 120.0, period_s: float = 2.0, announce=print) -> dict:
    """Verificação de saúde do rádio: com o SX1276 desligado e religado, o rst da TM sobe.

    Exige o umbilical (--wired), pois o RF fica mudo enquanto o rádio está desligado.
    """
    started = datetime.now().isoformat(timespec="seconds")
    t0 = time.monotonic()
    samples = []
    if link.source != "wired":
        announce("Use --wired: com o SX1276 desligado não há enlace RF.")
    else:
        announce(f"Desligue o SX1276 agora e religue depois de ~{int(duration_s / 3)} s. "
                 f"Acompanhamento por {int(duration_s)} s.")
    last_rst = None
    with _Watcher(link) as watcher:
        while link.source == "wired" and time.monotonic() - t0 < duration_s:
            ack, tm = _request_tm(link, watcher)
            rst = tm.get("rst") if tm else None
            samples.append({"t_s": round(time.monotonic() - t0, 1), "ack": ack["status"], "rst": rst})
            if rst != last_rst:
                announce(f"{samples[-1]['t_s']:7.1f} s  rst = {rst}")
                last_rst = rst
            time.sleep(period_s)

    values = [s["rst"] for s in samples if isinstance(s["rst"], int)]
    increase = (values[-1] - values[0]) % COUNTER_MODULO if values else 0
    report = {
        "test": "health",
        "requirement": "Verificação de saúde do rádio (rst)",
        "source": link.source,
        "started": started,
        "duration_s": round(time.monotonic() - t0, 1),
        "rst_first": values[0] if values else None,
        "rst_last": values[-1] if values else None,
        "rst_increase": increase,
        "last_ack": samples[-1]["ack"] if samples else None,
        "samples": samples,
    }
    report["passed"] = bool(link.source == "wired" and increase >= 1 and samples and samples[-1]["ack"] == "ok")
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
    test = report["test"]
    if test == "tm-loss":
        lines += [
            f"- TMs pedidas: {report['requested']}, recebidas: {report['tm_received']}, "
            f"faltando: {report['tm_missing']}",
            f"- Ping de fechamento: {(report['closing_ack'] or {}).get('status', 'não enviado')}",
            f"- Pacotes recebidos: {report['received']}, perdidos: {report['lost']}, "
            f"taxa de perda: {report['loss_rate'] * 100:.2f}% (limite {TM_LOSS_LIMIT * 100:.1f}%)",
            f"- Quadros com CRC inválido na estação terrena: {report['crc_errors']}",
        ]
    elif test == "tc-latency":
        forward, e2e = report["forward_ms"], report["end_to_end_ms"]
        lines += [
            f"- TCs enviados: {report['sent']}, confirmados: {report['acked']}, "
            f"status ok: {report['status_ok']}, retransmissões: {report['retransmissions']}",
            f"- Entrega ao OBC (q): máx {forward.get('max')} ms, p95 {forward.get('p95')} ms "
            f"(limite {TC_FORWARD_LIMIT_MS} ms)",
            f"- Ida e volta: máx {e2e.get('max')} ms, média {e2e.get('mean')} ms (limite {TC_END_TO_END_MS} ms)",
        ]
    elif test == "dedup":
        first = report["first"]
        lines += [
            f"- TC original: {first['cmd']} seq {first['seq']}, status {first['status']}, q {first['q']}",
            "- Repetições (mesmo seq): "
            + (", ".join(f"{r['status']}/q {r['q']}" for r in report["repeats"]) or "nenhuma"),
            f"- rx antes: {report['rx_before']}, depois: {report['rx_after']}, "
            f"diferença: {report['rx_delta']} (esperado 1: só o request_tm de fechamento)",
            f"- TMs recebidas durante as repetições: {report['tm_after_repeats']}",
        ]
    elif test == "delivery":
        lines += [
            f"- Fase: {report['phase']}, ID: {report['id']}",
            f"- Recepções: {report['receptions']}, repetida por recepção: {report['repeated_flags']}, "
            f"mesmo conteúdo: {report['same_content']}",
            f"- Intervalo até a retransmissão (s): {report['retransmit_after_s']}",
            f"- Contagem da estação: {report['counts']}",
        ]
    elif test == "congestion":
        rtt = report["ping_rtt_ms"]
        lines += [
            f"- Rajada: {report['requested']} TMs pedidas, {report['tm_received']} recebidas",
            f"- Pings durante a rajada: {report['pings']}, confirmados: {report['pings_acked']}, "
            f"retransmissões: {report['retransmissions']}",
            f"- Ida e volta do ping: máx {rtt.get('max')} ms, média {rtt.get('mean')} ms "
            f"(limite {TC_END_TO_END_MS} ms)",
            f"- Entregas recebidas: {report['deliveries']}, retransmitidas pelo OBC: "
            f"{report['deliveries_retransmitted']}",
        ]
    elif test == "health":
        lines += [
            f"- rst: {report['rst_first']} → {report['rst_last']} (aumento {report['rst_increase']})",
            f"- Último request_tm: {report['last_ack']}",
        ]
    return "\n".join(lines) + "\n"
