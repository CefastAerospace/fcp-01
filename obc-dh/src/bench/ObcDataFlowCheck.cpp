#include "ObcTtc.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDataFlowEsp32.h".
 */

#ifndef OBC_FLOW_BENCH
#error "Usar somente o ambiente int006-check"
#endif

namespace {
    extern const uint8_t fixture_start[] asm("_binary__pio_flow_fixtures_fcpu_start");
    extern const uint8_t fixture_end[] asm("_binary__pio_flow_fixtures_fcpu_end");
    std::atomic<bool> bench_requested(false);
}

namespace obc_flow {
namespace esp32 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    bool opened = false;
#ifdef OBC_DELIVERY_BENCH
    struct SIMULATED_FILE {
        char name[obc_storage::filename_chars + 1];
        uint8_t bytes[obc_delivery::data_bytes];
        size_t size;
        bool closed;
    };
    // Respostas de arquivo simuladas conservam apenas os registros FCD1.
    SIMULATED_FILE delivery_files[32] = {};
    size_t delivery_file_count = 0;
    SIMULATED_FILE current_delivery_file = {};
#endif

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Backend simulado: confirma cópia estável após bloquear o escritor por 100 ms.
    obc_storage::RESULT OpenBench(
        void*,
        const char* name
    ) {
        Serial.printf("[FLOW-CHECK] Arquivo simulado: %s\n", name);
        opened = true;
#ifdef OBC_DELIVERY_BENCH
        current_delivery_file = {};
        std::strcpy(current_delivery_file.name, name);
#endif
        return {obc_storage::STATUS::OK, 0, 0, 0};
    }

    obc_storage::RESULT AppendBench(
        void*,
        const uint8_t* bytes,
        size_t size
    ) {
        if (!opened) return {obc_storage::STATUS::WRITE_FAILED, 0, 0, 0};
        const uint32_t before = obc_record::ComputeCrc32(bytes, size);
        vTaskDelay(pdMS_TO_TICKS(100));
        if (before != obc_record::ComputeCrc32(bytes, size)) {
            Serial.printf("[FLOW-CHECK] FAIL: buffer alterado durante escrita\n");
            return {obc_storage::STATUS::WRITE_FAILED, 0, 0, 1};
        }
#ifdef OBC_DELIVERY_BENCH
        if (size >= 4 && !std::memcmp(bytes, "FCD1", 4)) {
            if (delivery_file_count == 32 || size > sizeof(current_delivery_file.bytes)) {
                return {obc_storage::STATUS::NO_SPACE, 0, 0, 1};
            }
            obc_delivery::RECORD record = {};
            if (!obc_delivery::Decode(bytes, size, record)) {
                Serial.printf("[DELIVERY-CHECK] FAIL invalid journal\n");
                return {obc_storage::STATUS::WRITE_FAILED, 0, 0, 1};
            }
            std::memcpy(current_delivery_file.bytes, bytes, size);
            current_delivery_file.size = size;
        }
#endif
        if (std::memcmp(bytes, obc_record::telemetry_magic, 4) == 0) {
            obc_record::RECORD record = {};
            size_t consumed = 0;
            if (obc_record::DecodeTelemetryV3(bytes, size, record, consumed) != obc_record::STATUS::OK) {
                Serial.printf("[FLOW-CHECK] FAIL: registro v3 invalido\n");
                return {obc_storage::STATUS::WRITE_FAILED, 0, 0, 1};
            }
            const auto& tm = record.body.telemetry;
            Serial.printf("[FLOW-CHECK] v3 ts_available=%u ts=%lu ac_available=%u ac=%u msg_available=%u stable=1\n",
                (tm.available_fields & obc_data::TM_TIMESTAMP) ? 1U : 0U,
                static_cast<unsigned long>(tm.timestamp),
                (tm.available_fields & obc_data::TM_AIRCRAFT) ? 1U : 0U,
                tm.aircraft_count, (tm.available_fields & obc_data::TM_MESSAGES) ? 1U : 0U);
        }
        return {obc_storage::STATUS::OK, size, 0, 1};
    }

    obc_storage::RESULT CloseBench(
        void*
    ) {
        opened = false;
#ifdef OBC_DELIVERY_BENCH
        if (current_delivery_file.size && delivery_file_count < 32) {
            current_delivery_file.closed = true;
            delivery_files[delivery_file_count++] = current_delivery_file;
            current_delivery_file = {};
        }
#endif
        return {obc_storage::STATUS::OK, 0, 0, 0};
    }

#ifdef OBC_DELIVERY_BENCH
    // A listagem contém somente os arquivos simulados já fechados.
    obc_storage::RESULT ListBench(
        void*,
        const char* after,
        obc_storage::FILE_INFO& out
    ) {
        out = {};
        for (size_t index = 0; index < delivery_file_count; ++index) {
            const auto& file = delivery_files[index];
            if (std::strcmp(file.name, after) > 0 && (!out.name[0] || std::strcmp(file.name, out.name) < 0)) {
                std::strcpy(out.name, file.name);
                out.size = static_cast<uint32_t>(file.size);
            }
        }
        return {obc_storage::STATUS::OK, 0, 0, 0};
    }

    // Blocos e EOF usam o mesmo contrato do serviço real.
    obc_storage::RESULT ReadBench(
        void*,
        const char* name,
        uint32_t offset,
        uint8_t* bytes,
        size_t capacity,
        uint32_t& total
    ) {
        vTaskDelay(pdMS_TO_TICKS(1));
        total = 0;
        for (size_t index = 0; index < delivery_file_count; ++index) {
            const auto& file = delivery_files[index];
            if (std::strcmp(file.name, name)) continue;
            total = static_cast<uint32_t>(file.size);
            if (offset > total) return {obc_storage::STATUS::INVALID_ARGUMENT, 0, 0, 0};
            const size_t size = capacity < total - offset ? capacity : total - offset;
            std::memcpy(bytes, file.bytes + offset, size);
            return {obc_storage::STATUS::OK, size, 0, 0};
        }
        return {obc_storage::STATUS::NOT_FOUND, 0, 0, 0};
    }
#endif

} // namespace

JOURNAL_IO GetBenchIO(
) {
    return {OpenBench, AppendBench, CloseBench, nullptr};
}

#ifdef OBC_DELIVERY_BENCH
obc_delivery::READER_IO GetBenchReaderIO(
) { return {ListBench, ReadBench, nullptr}; }
#endif

} // namespace esp32
} // namespace obc_flow

namespace obc_payload {
namespace esp32 {

void StartBenchInput(
) {
    bench_requested.store(true);
}

size_t ReadBenchInput(
    uint8_t* bytes,
    size_t capacity,
    uint64_t now_ms
) {
    static bool started = false;
    static uint64_t next_ms = 0;
    static size_t offset = 0;
    static size_t frame_end_offset = 0;
    if (!bench_requested.load()) return 0;
    if (!started) {
        started = true;
        next_ms = now_ms + 1000;
        Serial.printf("[FLOW-CHECK] UTC recebida; inicio da injecao em 1000 ms\n");
    }
    const size_t length = reinterpret_cast<uintptr_t>(fixture_end) - reinterpret_cast<uintptr_t>(fixture_start);
    if (now_ms < next_ms || offset == length) return 0;
    if (frame_end_offset == offset) {
        if (length - offset < obc_payload::header_bytes) {
            Serial.printf("[FLOW-CHECK] FAIL: fixture truncada\n");
            offset = length;
            return 0;
        }
        const uint8_t* frame = fixture_start + offset;
        uint32_t body_size = 0;
        for (unsigned index = 0; index < 4; ++index) body_size |= static_cast<uint32_t>(frame[18 + index]) << (8 * index);
        const size_t size = obc_payload::header_bytes + body_size + obc_payload::crc_bytes;
        if (size > length - offset) {
            Serial.printf("[FLOW-CHECK] FAIL: fixture truncada\n");
            offset = length;
            return 0;
        }
        frame_end_offset = offset + size;
    }
    const size_t remaining = frame_end_offset - offset;
    const size_t count = remaining < capacity ? remaining : capacity;
    std::memcpy(bytes, fixture_start + offset, count);
    offset += count;
    if (offset == frame_end_offset) next_ms = now_ms + 20;
    return count;
}

} // namespace esp32
} // namespace obc_payload

/** @brief Reserva boot real; substitui somente o backend de SD e a entrada UART. */
void setup(
) {
    Serial.begin(115200);
    Serial.printf("[FLOW-CHECK] ADS-B em RAM e SD lento simulados; sem ensaio ESP32-Pi/cartao\n");
    obc_startup::STARTUP_REPORT report = {};
    report.time_status = obc_time::esp32::StartBoot(report.boot_id);
    report.storage_result.status = obc_storage::STATUS::OK;
    obc_ttc::Init();
    obc_flow::esp32::Init(report);
}

/** @brief Mantém o mesmo atendimento de comandos e diagnósticos do firmware central. */
void loop(
) {
    obc_ttc::Service();
    obc_flow::esp32::ReportDiagnostics();
#ifdef OBC_DELIVERY_BENCH
    // Transporte simulado no consumidor independente do escritor de 100 ms.
    static obc_delivery::DELIVERY delivery = {};
    static uint64_t complete_ms = 0;
    static obc_delivery::ID previous = {};
    static bool retry = false;
    static bool waiting_completion = false;
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    if (waiting_completion && now_ms >= complete_ms) {
        obc_flow::esp32::FinishDeliveryTransmit(delivery.id, true);
        // Primeira tentativa perde ACK. Segunda confirma, inclusive antes do SD.
        if (retry) obc_flow::esp32::ConfirmDelivery(delivery.id);
        waiting_completion = false;
    }
    if (!waiting_completion && obc_flow::esp32::BeginDeliveryTransmit(200, delivery)) {
        retry = obc_delivery::Compare(previous, delivery.id) == 0;
        previous = delivery.id;
        complete_ms = now_ms + 10;
        waiting_completion = true;
        Serial.printf("[DELIVERY-CHECK] tx_boot=%llu tx_sequence=%llu retry=%u original_ms=%llu\n",
            static_cast<unsigned long long>(delivery.id.boot), static_cast<unsigned long long>(delivery.id.sequence),
            retry ? 1U : 0U, static_cast<unsigned long long>(delivery.monotonic_ms));
    }
#endif
}
