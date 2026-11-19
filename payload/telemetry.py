import json
import sys
import time
import threading
from datetime import datetime, timezone

import serial

# O payload encaminhado é no mesmo "formato" que o `dump1090 --interactive`
# envia, a diferença é que agora existe tanto um id no envio atual, com um
# pequeno cache, e também mensagens da mesma aeronave são mescladas em um
# mesmo objeto.
#
# o payload envia a mensagem com um id monotônico, e espera que obc
# retorne o id seguinte, que a partir desse momento, removerá do cache
# o valor antigo e seguirá a partir dele

messages = {}
last_updated = {}
block_lines = []

payload_cache = {}
next_id = 1

ser = serial.Serial("/dev/serial0", baudrate=115200, timeout=1)
state_lock = threading.Lock()
serial_write_lock = threading.Lock()

def remove_expired():
    now = time.monotonic()
    for message_id in list(messages):
        if message_id in last_updated and now - last_updated[message_id] >= 60:
            del messages[message_id]
            del last_updated[message_id]

def send_payload(payload):
    with serial_write_lock:
        ser.write(payload)

def read_serial():
    while True:
        line = ser.readline()
        if not line:
            continue

        try:
            received = json.loads(line.decode("utf-8"))
            requested_id = int(received["id"])
        except (UnicodeDecodeError, json.JSONDecodeError, KeyError, TypeError, ValueError):
            continue

        with state_lock:
            # The receiver says it has everything before requested_id.
            for cached_id in list(payload_cache):
                if cached_id < requested_id:
                    del payload_cache[cached_id]

            # If we've already sent this ID, resend its original payload.
            payload = payload_cache.get(requested_id)

        if payload is not None:
            send_payload(payload)


def process_block(lines):
    global next_id

    if not lines:
        return

    header = lines[0].strip()
    if not (header.startswith("*") and header.endswith(";")):
        return

    fields = {}
    for line in lines[1:]:
        if ":" in line:
            key, value = line.split(":", 1)
            fields[key.strip()] = value.strip()

    message_id = fields["ICAO Address"]

    with state_lock:
        remove_expired()

        message = messages.setdefault(message_id, {})
        message.update(fields)
        message["tick"] = datetime.now(timezone.utc).isoformat()

        payload_id = next_id
        next_id += 1

        payload = json.dumps({
            "id": payload_id,
            "messages": messages,
        }).encode("utf-8") + b"\n"

        payload_cache[payload_id] = payload

    print(payload)
    send_payload(payload)


threading.Thread(target=read_serial, daemon=True).start()

for line in sys.stdin:
    line = line.rstrip("\r\n")
    print(f"=== {line}")
    if not line.strip():
        process_block(block_lines)
        block_lines = []
    else:
        block_lines.append(line)

process_block(block_lines)
ser.close()
