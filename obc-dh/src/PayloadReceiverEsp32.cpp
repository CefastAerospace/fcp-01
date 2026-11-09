#include "PayloadReceiverEsp32.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "PayloadReceiverEsp32.h".
 */

namespace obc_payload {
namespace esp32 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    Receiver* active_receiver = nullptr;
    uint8_t input[256] = {};
    size_t input_size = 0;
    size_t input_offset = 0;
    RESULT pending_reply = {};
    size_t reply_offset = 0;

} // namespace

bool Init(
    Receiver& receiver,
    uint64_t boot_id
) {
    if (active_receiver != nullptr || boot_id == 0) return false;
    if (!receiver.Init(boot_id)) return false;
    if (Serial2.setRxBufferSize(4096) != 4096 || Serial2.setTxBufferSize(256) != 256) return false;
    Serial2.begin(115200, SERIAL_8N1, pinout::uart2_rx, pinout::uart2_tx);
    if (!Serial2) return false;
    active_receiver = &receiver;
    return true;
}

STATUS Poll(
) {
    if (active_receiver == nullptr) return STATUS::NOT_INITIALIZED;
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    const STATUS timeout = active_receiver->CheckTimeout(now_ms);
    if (timeout != STATUS::WAITING) return timeout;

    // Só escreve a quantidade que o driver já aceita sem esperar espaço.
    if (reply_offset < pending_reply.reply_size) {
        const int available = Serial2.availableForWrite();
        if (available <= 0) return STATUS::WAITING;
        const size_t remaining = pending_reply.reply_size - reply_offset;
        const size_t count = remaining < static_cast<size_t>(available) ? remaining : static_cast<size_t>(available);
        reply_offset += Serial2.write(pending_reply.reply + reply_offset, count);
        if (reply_offset < pending_reply.reply_size) return STATUS::WAITING;
    }

    // Processa também candidatos retidos pela ressincronização do núcleo.
    if (input_offset == input_size) {
        input_size = Serial2.read(input, sizeof(input));
        input_offset = 0;
    }
    const RESULT result = active_receiver->Feed(input + input_offset, input_size - input_offset, now_ms);
    input_offset += result.consumed;
    pending_reply = result;
    reply_offset = 0;
    return result.status;
}

} // namespace esp32
} // namespace obc_payload
