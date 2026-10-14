"""Registro da sessão em JSON Lines (HLR-SW-02: dados estruturados e reproduzíveis)."""

import json
import threading
import time
from datetime import datetime, timezone
from pathlib import Path


class SessionLog:
    """Um arquivo .jsonl por sessão, uma linha por evento, com o horário do PC.

    Eventos: {"pc": "<ISO UTC>", "pc_ms": <epoch ms>, "dir": "rx"|"tx"|"log", ...}.
    """

    def __init__(self, directory: Path) -> None:
        directory.mkdir(parents=True, exist_ok=True)
        name = datetime.now().strftime("%Y-%m-%d_%H%M%S") + ".jsonl"
        self.path = directory / name
        self._file = self.path.open("a", encoding="utf-8")
        self._lock = threading.Lock()

    def write(self, event: dict) -> None:
        now = time.time()
        record = {
            "pc": datetime.fromtimestamp(now, timezone.utc).isoformat(timespec="milliseconds"),
            "pc_ms": int(now * 1000),
            **event,
        }
        line = json.dumps(record, separators=(",", ":"), ensure_ascii=False)
        with self._lock:
            self._file.write(line + "\n")
            self._file.flush()

    def close(self) -> None:
        with self._lock:
            self._file.close()
