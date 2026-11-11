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
#if defined(OBC_CAPACITY_BENCH)
    extern const uint8_t fixture_start[] asm("_binary__pio_capacity_fixtures_fcpu_start");
    extern const uint8_t fixture_end[] asm("_binary__pio_capacity_fixtures_fcpu_end");
#elif defined(OBC_MEMORY_BENCH)
    extern const uint8_t fixture_start[] asm("_binary__pio_memory_fixtures_fcpu_start");
    extern const uint8_t fixture_end[] asm("_binary__pio_memory_fixtures_fcpu_end");
#else
    extern const uint8_t fixture_start[] asm("_binary__pio_flow_fixtures_fcpu_start");
    extern const uint8_t fixture_end[] asm("_binary__pio_flow_fixtures_fcpu_end");
#endif
    std::atomic<bool> bench_requested(false);
#if defined(OBC_CAPACITY_BENCH) || defined(OBC_MEMORY_BENCH)
    std::atomic<size_t> limits_processed(0);
    std::atomic<bool> limits_drained(false);
#endif
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
#ifdef OBC_CAPACITY_BENCH
    constexpr size_t simulated_file_capacity = 66; // 32 pares e um par após retomada.
#else
    constexpr size_t simulated_file_capacity = 32;
#endif
    SIMULATED_FILE delivery_files[simulated_file_capacity] = {};
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
#ifdef OBC_CAPACITY_BENCH
            // Sem DATA durável, as 40 alterações precisam disputar somente o cache.
            const uint64_t started_ms = obc_time::esp32::GetMonotonicMs();
            while (limits_processed.load() < 40) {
                if (obc_time::esp32::GetMonotonicMs() - started_ms >= 8000) {
                    Serial.printf("[LIMITS] FAIL: barreira de saturacao expirou\n");
                    return {obc_storage::STATUS::WRITE_FAILED, 0, 0, 1};
                }
                vTaskDelay(pdMS_TO_TICKS(1));
            }
#endif
#ifdef OBC_STRESS_BENCH
            // Falha injetada após sete DATA e um CONFIRM; não altera a capacidade normal.
            if (delivery_file_count >= 8) {
                Serial.printf("[STRESS] append=NO_SPACE files=%u\n", static_cast<unsigned>(delivery_file_count));
                return {obc_storage::STATUS::NO_SPACE, 0, 0, 1};
            }
#endif
            if (delivery_file_count == simulated_file_capacity || size > sizeof(current_delivery_file.bytes)) {
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
        if (current_delivery_file.size && delivery_file_count < simulated_file_capacity) {
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
#ifdef OBC_STRESS_BENCH
        // Nomes reservados somente neste ambiente, sem novos comandos no produto.
        if (!std::strcmp(name, "busy.bin")) {
            Serial.printf("[STRESS] read=busy\n");
            return {obc_storage::STATUS::BUSY, 0, 0, 0};
        }
        if (!std::strcmp(name, "io.bin")) {
            Serial.printf("[STRESS] read=io\n");
            const size_t partial = capacity < 3 ? capacity : 3;
            std::memset(bytes, 0xAA, partial);
            total = 3;
            return {obc_storage::STATUS::READ_FAILED, partial, 0, 0};
        }
        if (!std::strcmp(name, "slow.bin") || !std::strcmp(name, "ok.bin")) {
            if (!std::strcmp(name, "slow.bin")) {
                Serial.printf("[STRESS] slow=begin\n");
                vTaskDelay(pdMS_TO_TICKS(1500));
                Serial.printf("[STRESS] slow=end\n");
            }
            const uint8_t content[] = "OBC-STRESS";
            total = sizeof(content) - 1;
            if (offset > total) return {obc_storage::STATUS::INVALID_ARGUMENT, 0, 0, 0};
            const size_t size = capacity < total - offset ? capacity : total - offset;
            std::memcpy(bytes, content + offset, size);
            return {obc_storage::STATUS::OK, size, 0, 0};
        }
#endif
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

#if defined(OBC_CAPACITY_BENCH) || defined(OBC_MEMORY_BENCH)
void ObserveLimitsPayload(
    obc_payload::STATUS status,
    const obc_data::PAYLOAD_SNAPSHOT* snapshot,
    const obc_delivery::STATISTICS& statistics
) {
    const size_t index = limits_processed.fetch_add(1) + 1;
    const char* name = status == obc_payload::STATUS::READY ? "READY"
        : status == obc_payload::STATUS::NO_MEMORY ? "NO_MEMORY" : "unexpected";
    const uint32_t crc = snapshot ? obc_record::ComputeCrc32(
        reinterpret_cast<const uint8_t*>(snapshot), sizeof(*snapshot)) : 0;
    const bool valid = snapshot && obc_data::Validate(*snapshot) == obc_data::STATUS::OK;
    Serial.printf("[LIMITS-PAYLOAD] case=%u status=%s id=%llu count=%u crc=%lu valid=%u extras=%u identification=%u tick=%u accepted=%llu lost=%llu occupied=%u peak=%u\n",
        static_cast<unsigned>(index), name,
        static_cast<unsigned long long>(snapshot ? snapshot->id : 0),
        snapshot ? snapshot->aircraft_count : 0U, static_cast<unsigned long>(crc), valid ? 1U : 0U,
        snapshot && snapshot->aircraft_count ? snapshot->aircraft[0].extra_count : 0U,
        snapshot && snapshot->aircraft_count ? static_cast<unsigned>(std::strlen(snapshot->aircraft[0].identification)) : 0U,
        snapshot && snapshot->aircraft_count ? static_cast<unsigned>(std::strlen(snapshot->aircraft[0].tick)) : 0U,
        static_cast<unsigned long long>(statistics.accepted), static_cast<unsigned long long>(statistics.lost),
        static_cast<unsigned>(statistics.occupied), static_cast<unsigned>(statistics.high_water));
}

void ObserveLimitsDeliveries(
    const obc_delivery::STATISTICS& statistics
) {
#ifdef OBC_CAPACITY_BENCH
    if (statistics.accepted == 32 && statistics.confirmed == 32 && statistics.occupied == 0) {
        limits_drained.store(true);
    }
#else
    (void)statistics;
#endif
}
#endif

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
#if defined(OBC_CAPACITY_BENCH) || defined(OBC_MEMORY_BENCH)
    static size_t sent_frames = 0;
#endif
    if (!bench_requested.load()) return 0;
    if (!started) {
        started = true;
        next_ms = now_ms + 1000;
        Serial.printf("[FLOW-CHECK] UTC recebida; inicio da injecao em 1000 ms\n");
    }
    const size_t length = reinterpret_cast<uintptr_t>(fixture_end) - reinterpret_cast<uintptr_t>(fixture_start);
    if (now_ms < next_ms || offset == length) return 0;
    if (frame_end_offset == offset) {
#ifdef OBC_CAPACITY_BENCH
        if (sent_frames == 40 && !limits_drained.load()) return 0;
#elif defined(OBC_MEMORY_BENCH)
        if (limits_processed.load() < sent_frames) return 0;
#endif
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
    if (offset == frame_end_offset) {
#if defined(OBC_CAPACITY_BENCH) || defined(OBC_MEMORY_BENCH)
        ++sent_frames;
#endif
#ifdef OBC_MEMORY_BENCH
        next_ms = now_ms + 1000;
#else
        next_ms = now_ms + 20;
#endif
    }
    return count;
}

} // namespace esp32
} // namespace obc_payload

/** @brief Reserva boot real; substitui somente o backend de SD e a entrada UART. */
void setup(
) {
    Serial.begin(115200);
#ifdef OBC_CAPACITY_BENCH
    Serial.printf("[LIMITS] scenario=capacity files=66\n");
#elif defined(OBC_MEMORY_BENCH)
    Serial.printf("[LIMITS] scenario=memory files=32\n");
#endif
#ifdef OBC_STRESS_BENCH
    Serial.printf("[STRESS] ativo: read lento/busy/io; NO_SPACE apos 8 arquivos FCD1\n");
#endif
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
#if defined(OBC_DELIVERY_BENCH) && !defined(OBC_TTC_INTEGRATION_BENCH)
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
