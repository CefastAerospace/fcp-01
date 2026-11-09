#!/usr/bin/env python3
"""Inicializa nextId_ no ArduinoJson 7.0.4, com versão e hashes conferidos.

O PlatformIO executa este arquivo após instalar as dependências. Para os
testes nativos, executar `python3 tools/PatchArduinoJson.py` em obc-dh/.
Não altera flags, dados de teste ou código de outra versão da biblioteca.
Remover o hook e reinstalar a dependência desfaz a correção no cache.
"""

import hashlib
import json
from pathlib import Path
import sys


ORIGINAL_HASH = "c80c05962a51170dfbab5506b274c8d7394ae1a0165cd8784d9a7cf52e65f18a"
PATCHED_HASH = "0d1ef08519e3e05b46d5a08a8c15976158fe1b7d7c20551e00c2eb557dc8a894"
ORIGINAL = b"CollectionIterator() : slot_(nullptr), currentId_(NULL_SLOT) {}"
PATCHED = b"CollectionIterator() : slot_(nullptr), currentId_(NULL_SLOT), nextId_(NULL_SLOT) {}"


def ApplyPatch(library):
    """Aplica uma alteração exata; recusa versão ou conteúdo desconhecido."""
    manifest = json.loads((library / "library.json").read_text())
    if manifest.get("name") != "ArduinoJson" or manifest.get("version") != "7.0.4":
        raise RuntimeError("Patch aprovado somente para ArduinoJson 7.0.4")
    target = library / "src/ArduinoJson/Collection/CollectionData.hpp"
    source = target.read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    if digest == PATCHED_HASH:
        return False
    if digest != ORIGINAL_HASH or source.count(ORIGINAL) != 1:
        raise RuntimeError("CollectionData.hpp desconhecido; patch não aplicado")
    patched = source.replace(ORIGINAL, PATCHED, 1)
    if hashlib.sha256(patched).hexdigest() != PATCHED_HASH:
        raise RuntimeError("Conteúdo corrigido diverge do hash aprovado")
    target.write_bytes(patched)
    return True


if __name__ == "__main__":
    library_path = Path(sys.argv[1]) if len(sys.argv) == 2 else (
        Path(__file__).resolve().parents[1] / ".pio/libdeps/obc/ArduinoJson"
    )
    changed = ApplyPatch(library_path)
    print("ArduinoJson 7.0.4: nextId_ " + ("corrigido" if changed else "já corrigido"))
else:
    # Import é fornecido pelo SCons; não há dependência de PlatformIO no CLI.
    Import("env")
    library_path = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "ArduinoJson"
    if ApplyPatch(library_path):
        print("ArduinoJson 7.0.4: patch nextId_ aplicado")
