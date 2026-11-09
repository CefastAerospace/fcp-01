/**
 * @file PayloadReceiverCheck.cpp
 * @brief Autoverificação ESP32 da amostra e medição de memória/pilha.
 * @details uart-receiver-check incorpora payload.json sem alterar a origem.
 * Injeta bytes no núcleo em RAM; não comprova troca física UART com o Pi.
 * Usa boot sintético 1, sem reservar NVS ou iniciar serviços de missão.
 */

#include "PayloadReceiverEsp32.h" // Núcleo, adaptador, Serial, ESP e tempo.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "PayloadReceiverEsp32.h".
 */

namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    obc_payload::Receiver receiver;
    uint8_t frame[8192] = {};
    extern const uint8_t sample_start[] asm("_binary_payload_json_start");
    extern const uint8_t sample_end[] asm("_binary_payload_json_end");

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Forma um único DATA com a amostra incorporada e metadados sintéticos.
    // O vetor independente de referência fica na suíte nativa struct/zlib.
    size_t BuildSampleFrame(
    ) {
        const size_t sample_size = reinterpret_cast<uintptr_t>(sample_end)
            - reinterpret_cast<uintptr_t>(sample_start) - 1;
        const size_t total = obc_payload::header_bytes + sample_size + obc_payload::crc_bytes;
        if (total > sizeof(frame)) return 0;
        std::memcpy(frame, "FCPU", 4);
        frame[4] = 1;
        frame[5] = static_cast<uint8_t>(obc_payload::FRAME_TYPE::DATA);
        frame[6] = 1;
        frame[14] = 1;
        for (size_t index = 0; index < 4; ++index) {
            frame[18 + index] = static_cast<uint8_t>(sample_size >> (index * 8));
        }
        std::memcpy(frame + obc_payload::header_bytes, sample_start, sample_size);
        const uint32_t crc = obc_record::ComputeCrc32(frame, total - obc_payload::crc_bytes);
        for (size_t index = 0; index < 4; ++index) {
            frame[total - obc_payload::crc_bytes + index] = static_cast<uint8_t>(crc >> (index * 8));
        }
        return total;
    }

} // namespace

/** @brief Confere amostra, repetição e CRC antes de imprimir as medições. */
void setup(
) {
    Serial.begin(115200);
    Serial.println("[UART-RX-CHECK] Injecao em RAM; boot sintetico=1; sem ensaio ESP32-Pi");
    const size_t length = BuildSampleFrame();
    if (length == 0) {
        Serial.println("[UART-RX-CHECK] FAIL: inicializacao/amostra");
        return;
    }
    // O contexto é pequeno; Init reserva os dois blocos internos no heap.
    Serial.printf("[UART-RX-CHECK] heap_initial=%u; largest_block=%u\n",
        ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    if (!obc_payload::esp32::Init(receiver, 1)) {
        Serial.println("[UART-RX-CHECK] FAIL: inicializacao UART2/memoria");
        return;
    }
    const uint32_t heap_before = ESP.getFreeHeap();
    const auto first = receiver.Feed(frame, length, obc_time::esp32::GetMonotonicMs());
    const auto* snapshot = receiver.GetSnapshot();
    const bool valid_sample = first.status == obc_payload::STATUS::READY
        && first.consumed == length && snapshot != nullptr
        && snapshot->id == 480 && snapshot->aircraft_count == 7
        && obc_data::Validate(*snapshot) == obc_data::STATUS::OK;
    const auto duplicate = receiver.Feed(frame, length, obc_time::esp32::GetMonotonicMs());
    frame[length - 1] ^= 1;
    const auto corrupt = receiver.Feed(frame, length, obc_time::esp32::GetMonotonicMs());
    const bool passed = valid_sample && duplicate.status == obc_payload::STATUS::DUPLICATE
        && corrupt.status == obc_payload::STATUS::INVALID_FRAME
        && receiver.GetStatistics().accepted == 1;
    Serial.printf("[UART-RX-CHECK] %s; accepted=%llu; duplicates=%llu; rejected=%llu\n",
        passed ? "PASS" : "FAIL", receiver.GetStatistics().accepted,
        receiver.GetStatistics().duplicates, receiver.GetStatistics().rejected);
    Serial.printf("[UART-RX-CHECK] receiver_bytes=%u; parser_peak_bytes=%u\n",
        static_cast<unsigned>(sizeof(receiver)),
        static_cast<unsigned>(receiver.GetStatistics().parser_peak_bytes));
    Serial.printf("[UART-RX-CHECK] heap_before=%u; heap_after=%u; stack_min_free_bytes=%u\n",
        heap_before, ESP.getFreeHeap(), static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

/** @brief Mantém o adaptador atendido, sem transmitir vetores pela UART2. */
void loop(
) {
    obc_payload::esp32::Poll();
    delay(1);
}
