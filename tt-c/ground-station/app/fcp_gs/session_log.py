"""Registro da sessão em JSON Lines (HLR-SW-02: dados estruturados e reproduzíveis)."""

import json
import os
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
        self._dir_synced = False
        self._directory = directory

    def write(self, event: dict, durable: bool = False) -> None:
        """Grava um evento. Com durable=True, só retorna depois do fsync.

        Usado antes de confirmar uma entrega ao satélite: o "cf" promete que o
        pacote já está no disco, não apenas no cache do sistema operacional.
        """
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
            if durable:
                os.fsync(self._file.fileno())
                if not self._dir_synced:
                    # A entrada do arquivo novo no diretório também precisa ser durável.
                    fsync_directory(self._directory)
                    self._dir_synced = True

    def close(self) -> None:
        with self._lock:
            self._file.close()


def fsync_directory(directory: Path) -> None:
    """Torna durável a criação de arquivos no diretório (sem efeito fora do POSIX)."""
    try:
        fd = os.open(directory, os.O_RDONLY)
    except OSError:
        return
    try:
        os.fsync(fd)
    except OSError:
        pass
    finally:
        os.close(fd)
