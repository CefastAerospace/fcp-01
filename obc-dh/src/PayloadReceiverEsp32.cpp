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
    uint8_t ring[1024] = {};
    size_t ring_head = 0;
    size_t ring_size = 0;
    portMUX_TYPE transport_lock = portMUX_INITIALIZER_UNLOCKED;
    TaskHandle_t transport_task = nullptr;
    bool transport_started = false;
    TRANSPORT_STATISTICS transport_statistics = {};

    // Move somente bytes; nunca chama o parser nem espera espaço de TX.
    void PumpTransport(
    ) {
        uint8_t outgoing[header_bytes + 5 + crc_bytes];
        portENTER_CRITICAL(&transport_lock);
        const size_t remaining = pending_reply.reply_size - reply_offset;
        if (remaining) std::memcpy(outgoing, pending_reply.reply + reply_offset, remaining);
        const size_t free_bytes = sizeof(ring) - ring_size;
        portEXIT_CRITICAL(&transport_lock);
        if (remaining) {
            const int available = Serial2.availableForWrite();
            if (available > 0) {
                const size_t count = remaining < static_cast<size_t>(available) ? remaining : static_cast<size_t>(available);
                const size_t sent = Serial2.write(outgoing, count);
                portENTER_CRITICAL(&transport_lock);
                reply_offset += sent;
                portEXIT_CRITICAL(&transport_lock);
            }
        }
        uint8_t incoming[256];
        const size_t capacity = free_bytes < sizeof(incoming) ? free_bytes : sizeof(incoming);
        if (capacity == 0) {
            portENTER_CRITICAL(&transport_lock);
            if (transport_statistics.full_observations != UINT64_MAX) ++transport_statistics.full_observations;
            portEXIT_CRITICAL(&transport_lock);
            return;
        }
#ifdef OBC_FLOW_BENCH
        const size_t count = ReadBenchInput(incoming, capacity, obc_time::esp32::GetMonotonicMs());
#else
        const size_t count = Serial2.read(incoming, capacity);
#endif
        portENTER_CRITICAL(&transport_lock);
        for (size_t index = 0; index < count; ++index) {
            ring[(ring_head + ring_size) % sizeof(ring)] = incoming[index];
            ++ring_size;
        }
        portEXIT_CRITICAL(&transport_lock);
    }

    // Proprietário único de Serial2 após StartTransportTask; dorme entre rodadas.
    void TransportEntry(
        void*
    ) {
        uint64_t previous_ms = obc_time::esp32::GetMonotonicMs();
        for (;;) {
            const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
            PumpTransport();
            portENTER_CRITICAL(&transport_lock);
            const uint64_t gap = now_ms - previous_ms;
            if (gap > transport_statistics.maximum_gap_ms) transport_statistics.maximum_gap_ms = gap;
            portEXIT_CRITICAL(&transport_lock);
            previous_ms = now_ms;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

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

bool StartTransportTask(
) {
    if (active_receiver == nullptr) return false;
    if (transport_started) return true;
    transport_started = true;
    if (xTaskCreatePinnedToCore(TransportEntry, "payload_uart", transport_stack_bytes,
            nullptr, 3, &transport_task, 1) != pdPASS) {
        transport_started = false;
        transport_task = nullptr;
        return false;
    }
    return true;
}

TRANSPORT_STATISTICS GetTransportStatistics(
) {
    portENTER_CRITICAL(&transport_lock);
    TRANSPORT_STATISTICS copy = transport_statistics;
    portEXIT_CRITICAL(&transport_lock);
    copy.stack_free_bytes = transport_task ? uxTaskGetStackHighWaterMark(transport_task) : 0;
    return copy;
}

STATUS Poll(
) {
    if (active_receiver == nullptr) return STATUS::NOT_INITIALIZED;
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    const STATUS timeout = active_receiver->CheckTimeout(now_ms);
    if (timeout != STATUS::WAITING) return timeout;

    // Na integração, o transporte acessa Serial2 e o consumidor só acessa o ring.
    if (transport_started) {
        portENTER_CRITICAL(&transport_lock);
        if (reply_offset < pending_reply.reply_size) {
            portEXIT_CRITICAL(&transport_lock);
            return STATUS::WAITING;
        }
        if (input_offset == input_size) {
            input_offset = 0;
            input_size = ring_size < sizeof(input) ? ring_size : sizeof(input);
            for (size_t index = 0; index < input_size; ++index) {
                input[index] = ring[ring_head];
                ring_head = (ring_head + 1) % sizeof(ring);
            }
            ring_size -= input_size;
        }
        portEXIT_CRITICAL(&transport_lock);
        const RESULT result = active_receiver->Feed(input + input_offset, input_size - input_offset, now_ms);
        input_offset += result.consumed;
        portENTER_CRITICAL(&transport_lock);
        pending_reply = result;
        reply_offset = 0;
        portEXIT_CRITICAL(&transport_lock);
        return result.status;
    }

    // Bancadas sequenciais antigas: mesmo fluxo, sem tarefa ou ring intermediário.
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
