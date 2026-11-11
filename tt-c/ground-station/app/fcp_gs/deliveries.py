"""Entregas identificadas do OBC: processamento único e confirmação ao satélite.

Cada pacote de aeronave com "id":[boot,seq] é uma entrega da INT-006. A
estação processa cada ID uma única vez e responde {"t":"cf","id":[boot,seq]}
a toda recepção, inclusive repetida, para o OBC parar de retransmitir.

Semântica da confirmação: o pacote foi gravado no log da sessão e o ID foi
gravado com fsync no registro de entregas. Esse registro sobrevive ao
reinício da estação, então uma retransmissão posterior continua reconhecida
como repetida.
"""

import json
import os
import threading
from pathlib import Path

from .protocol import DeliveryId
from .session_log import fsync_directory


class DeliveryStore:
    """Conjunto de IDs já processados, opcionalmente persistido em JSON Lines.

    Sem path, fica só na memória (testes e roteiros curtos). Linhas ilegíveis,
    como uma cauda cortada por queda de energia, são ignoradas na carga.
    """

    def __init__(self, path: Path | None = None) -> None:
        self.path = path
        self._ids: set[DeliveryId] = set()
        self._lock = threading.Lock()
        self.ignored_lines = 0
        self._file = None
        if path is None:
            return
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists():
            self._load(path)
            self._repair_tail(path)
        self._file = path.open("a", encoding="utf-8")
        fsync_directory(path.parent)

    def _load(self, path: Path) -> None:
        with path.open("r", encoding="utf-8", errors="replace") as source:
            for line in source:
                try:
                    boot, seq = json.loads(line)["id"]
                except (ValueError, KeyError, TypeError):
                    self.ignored_lines += 1
                    continue
                if isinstance(boot, int) and isinstance(seq, int) and boot > 0 and seq > 0:
                    self._ids.add((boot, seq))
                else:
                    self.ignored_lines += 1

    @staticmethod
    def _repair_tail(path: Path) -> None:
        """Descarta uma última linha sem "\\n" (gravação cortada por queda de energia).

        Sem isso, o próximo ID seria anexado colado à cauda e ficaria ilegível
        no reinício seguinte, embora já tivesse sido confirmado ao satélite.
        """
        with path.open("rb+") as target:
            data = target.read()
            if not data or data.endswith(b"\n"):
                return
            target.truncate(data.rfind(b"\n") + 1)
            target.flush()
            os.fsync(target.fileno())

    def __contains__(self, delivery: DeliveryId) -> bool:
        with self._lock:
            return delivery in self._ids

    def __len__(self) -> int:
        with self._lock:
            return len(self._ids)

    def record(self, delivery: DeliveryId) -> bool:
        """Grava o ID de forma durável. Devolve False se já existia."""
        with self._lock:
            if delivery in self._ids:
                return False
            if self._file is not None:
                self._file.write(json.dumps({"id": list(delivery)}, separators=(",", ":")) + "\n")
                self._file.flush()
                os.fsync(self._file.fileno())
            self._ids.add(delivery)
            return True

    def close(self) -> None:
        with self._lock:
            if self._file is not None:
                self._file.close()
                self._file = None
