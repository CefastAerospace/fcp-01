"""Análise pós-passagem de um log de sessão (.jsonl): perdas, latências e CSVs."""

import csv
import json
from pathlib import Path

from .protocol import LossTracker

TM_FIELDS = ["pc", "link", "n", "ts", "md", "vb", "ib", "vs", "is", "tp0", "tp1", "tp2", "tp3", "tp4",
             "w", "az", "ac", "msg", "rssi", "snr", "rx", "err", "rst", "gs_rssi", "gs_snr"]
AIRCRAFT_FIELDS = ["pc", "link", "n", "ts", "icao", "lat", "lon", "alt", "vel", "hdg", "cnt", "gs_rssi"]


def _tm_row(event: dict) -> dict:
    packet, meta = event["pkt"], event.get("gs", {})
    row = {key: packet.get(key) for key in TM_FIELDS if key in packet}
    for index, value in enumerate((packet.get("tp") or [])[:5]):
        row[f"tp{index}"] = value
    row.update(pc=event.get("pc"), link=event.get("link"), gs_rssi=meta.get("rssi"), gs_snr=meta.get("snr"))
    return row


def _aircraft_row(event: dict) -> dict:
    packet = event["pkt"]
    row = {key: packet.get(key) for key in AIRCRAFT_FIELDS if key in packet}
    row.update(pc=event.get("pc"), link=event.get("link"), gs_rssi=event.get("gs", {}).get("rssi"))
    return row


def analyze(log_path: Path, out_dir: Path | None = None) -> dict:
    """Lê o log e grava telemetry.csv e aircraft.csv; devolve o resumo."""
    out_dir = out_dir or log_path.with_suffix("")
    out_dir.mkdir(parents=True, exist_ok=True)

    trackers: dict[str, LossTracker] = {}
    counts: dict[str, int] = {}
    queue_ms: list[float] = []
    tm_rows, aircraft_rows = [], []
    tc_sent = 0

    with log_path.open(encoding="utf-8") as source:
        for line in source:
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue

            if event.get("dir") == "tx" and event.get("attempt") == 1:
                tc_sent += 1
            if event.get("dir") != "rx":
                continue

            kind = event.get("kind", "?")
            counts[kind] = counts.get(kind, 0) + 1
            packet = event.get("pkt")
            if not isinstance(packet, dict):
                continue

            link = event.get("link", "?")
            if isinstance(packet.get("n"), int):
                trackers.setdefault(link, LossTracker()).feed(packet["n"])

            packet_type = packet.get("t")
            counts[f"t:{packet_type}"] = counts.get(f"t:{packet_type}", 0) + 1
            if packet_type == "tm":
                tm_rows.append(_tm_row(event))
            elif packet_type == "ac":
                aircraft_rows.append(_aircraft_row(event))
            elif packet_type == "ack" and isinstance(packet.get("q"), (int, float)):
                queue_ms.append(packet["q"])

    for name, fields, rows in (("telemetry.csv", TM_FIELDS, tm_rows), ("aircraft.csv", AIRCRAFT_FIELDS, aircraft_rows)):
        with (out_dir / name).open("w", newline="", encoding="utf-8") as target:
            writer = csv.DictWriter(target, fieldnames=fields, extrasaction="ignore")
            writer.writeheader()
            writer.writerows(rows)

    return {
        "log": str(log_path),
        "output": str(out_dir),
        "counts": counts,
        "tc_sent": tc_sent,
        "loss": {link: tracker.as_dict() for link, tracker in trackers.items()},
        "forward_ms_max": max(queue_ms, default=None),
        "aircraft_seen": len({row.get("icao") for row in aircraft_rows}),
        "telemetry_rows": len(tm_rows),
    }
