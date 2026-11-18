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
from contextlib import contextmanager
from pathlib import Path

try:
    import fcntl
except ImportError:  # Windows: sem flock; um processo por registro.
    fcntl = None

from .protocol import DeliveryId
from .session_log import fsync_directory


class DeliveryStore:
    """Conjunto de IDs já processados, opcionalmente persistido em JSON Lines.

    Sem path, fica só na memória (testes e roteiros curtos). Linhas ilegíveis
    são ignoradas na carga. Uma última linha sem "\\n" (gravação cortada por
    queda de energia) é removida antes da próxima gravação, para o ID seguinte
    não ficar colado nela.

    Vários processos podem usar o mesmo arquivo (ex.: serve por RF e outro
    --wired): toda leitura e gravação acontece sob flock, e cada processo
    relê o que os outros acrescentaram antes de decidir se um ID é novo.
    """

    def __init__(self, path: Path | None = None) -> None:
        self.path = path
        self._ids: set[DeliveryId] = set()
        self._lock = threading.Lock()
        self.ignored_lines = 0
        self._file = None
        self._offset = 0  # Bytes já lidos, sempre no fim de uma linha completa.
        if path is None:
            return
        path.parent.mkdir(parents=True, exist_ok=True)
        self._file = path.open("ab")
        fsync_directory(path.parent)
        with self._lock, _file_lock(self._file, exclusive=True):
            self._sync()
            self._drop_partial_tail()

    def _sync(self) -> None:
        """Lê as linhas completas acrescentadas desde a última leitura."""
        with self.path.open("rb") as source:
            source.seek(self._offset)
            data = source.read()
        end = data.rfind(b"\n") + 1
        for line in data[:end].splitlines():
            self._parse_line(line)
        self._offset += end

    def _parse_line(self, line: bytes) -> None:
        try:
            boot, seq = json.loads(line)["id"]
        except (ValueError, KeyError, TypeError):
            self.ignored_lines += 1
            return
        if isinstance(boot, int) and isinstance(seq, int) and boot > 0 and seq > 0:
            self._ids.add((boot, seq))
        else:
            self.ignored_lines += 1

    def _drop_partial_tail(self) -> None:
        """Sob o flock exclusivo, bytes depois de _offset só podem ser de uma gravação cortada."""
        if self.path.stat().st_size <= self._offset:
            return
        self.ignored_lines += 1
        os.truncate(self.path, self._offset)
        os.fsync(self._file.fileno())

    def __contains__(self, delivery: DeliveryId) -> bool:
        with self._lock:
            if self._file is not None:
                with _file_lock(self._file, exclusive=False):
                    self._sync()
            return delivery in self._ids

    def __len__(self) -> int:
        with self._lock:
            return len(self._ids)

    def record(self, delivery: DeliveryId) -> bool:
        """Grava o ID de forma durável. Devolve False se já existia."""
        with self._lock:
            if self._file is None:
                if delivery in self._ids:
                    return False
                self._ids.add(delivery)
                return True
            with _file_lock(self._file, exclusive=True):
                self._sync()
                if delivery in self._ids:
                    return False
                self._drop_partial_tail()
                line = (json.dumps({"id": list(delivery)}, separators=(",", ":")) + "\n").encode()
                self._file.write(line)
                self._file.flush()
                os.fsync(self._file.fileno())
                self._offset += len(line)
                self._ids.add(delivery)
                return True

    def close(self) -> None:
        with self._lock:
            if self._file is not None:
                self._file.close()
                self._file = None


@contextmanager
def _file_lock(file, exclusive: bool):
    """flock entre processos (sem efeito fora do POSIX)."""
    if fcntl is None:
        yield
        return
    fcntl.flock(file.fileno(), fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH)
    try:
        yield
    finally:
        fcntl.flock(file.fileno(), fcntl.LOCK_UN)
