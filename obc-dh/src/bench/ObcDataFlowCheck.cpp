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

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Backend simulado: confirma cópia estável após bloquear o escritor por 100 ms.
    obc_storage::RESULT OpenBench(
        void*,
        const char* name
    ) {
        Serial.printf("[FLOW-CHECK] Arquivo simulado: %s\n", name);
        opened = true;
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
        return {obc_storage::STATUS::OK, 0, 0, 0};
    }

} // namespace

JOURNAL_IO GetBenchIO(
) {
    return {OpenBench, AppendBench, CloseBench, nullptr};
}

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
}
