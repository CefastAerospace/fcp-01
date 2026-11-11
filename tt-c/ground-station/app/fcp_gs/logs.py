"""Cliente do request_log: listagem e extração de arquivos fechados do SD do OBC.

Contrato em common/protocol/ttc-json.md. O cliente só avança depois de
validar a resposta do mesmo pedido; em "busy" ou resposta perdida, repete o
mesmo nome/offset. A conferência dos CRCs dos registros FCP/FCD fica com as
ferramentas do OBC (obc-dh/tools), aplicadas ao arquivo reconstruído.
"""

import base64
import binascii
import re
import time
from pathlib import Path

from .link import Link
from .protocol import LOG_BLOCK_MAX

NAME_PATTERN = re.compile(r"[A-Za-z0-9_.-]{1,39}")
BUSY_RETRIES = 5
BUSY_DELAY_S = 1.0


class LogError(RuntimeError):
    """Falha do request_log; code é o "err" do OBC, o status do ACK ou "protocol"."""

    def __init__(self, code: str, message: str) -> None:
        super().__init__(f"{code}: {message}")
        self.code = code


def valid_name(name: str) -> bool:
    return bool(NAME_PATTERN.fullmatch(name)) and not name.startswith(".") and ".." not in name


def _check_destination(link: Link, to: str | None) -> None:
    if to is not None and to != link.source:
        raise LogError("destination", f"respostas por '{to}' não chegam a esta estação ({link.source})")


def _request(link: Link, args: dict, op: str) -> dict:
    """Envia um pedido, repetindo em "busy"; devolve a resposta sem erro."""
    for attempt in range(BUSY_RETRIES + 1):
        result = link.request_log(args)
        if not result["ok"]:
            raise LogError(result["status"], f"request_log {args} sem resposta válida")
        response = result["response"]
        if response.get("op") != op:
            raise LogError("protocol", f"resposta de outra operação: {response}")
        error = response.get("err")
        if error is None:
            return response
        if error != "busy" or attempt == BUSY_RETRIES:
            raise LogError(str(error), f"request_log {args}")
        time.sleep(BUSY_DELAY_S)
    raise AssertionError("inalcançável")


def list_files(link: Link, to: str | None = None) -> list[dict]:
    """Lista todos os arquivos, seguindo o cursor até "end": true."""
    _check_destination(link, to)
    files: list[dict] = []
    after = ""
    while True:
        args: dict = {"op": "list"}
        if after:
            args["after"] = after
        if to is not None:
            args["to"] = to
        response = _request(link, args, "list")
        entries = response.get("ls")
        if not isinstance(entries, list) or not isinstance(response.get("end"), bool):
            raise LogError("protocol", f"listagem malformada: {response}")
        for entry in entries:
            if (not isinstance(entry, list) or len(entry) != 3 or not isinstance(entry[0], str)
                    or not valid_name(entry[0]) or not isinstance(entry[1], int) or entry[1] < 0
                    or entry[2] not in (0, 1)):
                raise LogError("protocol", f"item de listagem malformado: {entry}")
            if entry[0] <= after:
                raise LogError("protocol", f"cursor não avançou: {entry[0]} após {after}")
            files.append({"name": entry[0], "size": entry[1], "open": bool(entry[2])})
            after = entry[0]
        if response["end"]:
            return files
        if not entries:
            raise LogError("protocol", "listagem vazia sem fim")


def download_file(link: Link, name: str, directory: Path, block: int = LOG_BLOCK_MAX,
                  to: str | None = None) -> Path:
    """Reconstrói um arquivo fechado em directory/name, conferindo offsets e tamanho.

    Grava em name.part e renomeia só após o EOF consistente.
    """
    if not valid_name(name):
        raise LogError("invalid_param", f"nome inválido: {name}")
    if not 1 <= block <= LOG_BLOCK_MAX:
        raise LogError("invalid_param", f"bloco fora de 1..{LOG_BLOCK_MAX}")
    _check_destination(link, to)

    directory.mkdir(parents=True, exist_ok=True)
    target = directory / name
    partial = directory / (name + ".part")
    offset = 0
    total: int | None = None
    with partial.open("wb") as output:
        while True:
            args: dict = {"op": "read", "name": name, "off": offset, "len": block}
            if to is not None:
                args["to"] = to
            response = _request(link, args, "read")
            if response.get("name") != name or response.get("off") != offset:
                raise LogError("protocol", f"resposta de outro pedido: {response}")
            size = response.get("tot")
            if not isinstance(size, int) or size < 0 or (total is not None and size != total):
                raise LogError("protocol", f"tamanho inconsistente: {size} (antes {total})")
            total = size
            try:
                data = base64.b64decode(str(response.get("d", "")), validate=True)
            except (binascii.Error, ValueError) as error:
                raise LogError("protocol", f"base64 inválido em {offset}: {error}") from error
            if len(data) > block or offset + len(data) > total:
                raise LogError("protocol", f"bloco de {len(data)} bytes em {offset} excede o pedido")
            if not data:
                if offset != total:
                    raise LogError("protocol", f"bloco vazio em {offset} antes do fim ({total})")
                break
            output.write(data)
            offset += len(data)
    partial.replace(target)
    return target
