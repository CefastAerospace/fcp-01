#include "ObcTtc.h"
#include "ObcBenchSerial.h" // UART0 compartilhada no perfil de aquisição FCP4.
#ifdef OBC_STORAGE_RECOVERY_BENCH
#include "DeliveryRecoveryCheck.h" // Mesmo cenário de cache cheio executado no PC/ESP32.
#endif

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDataFlowEsp32.h".
 */

#ifndef OBC_FLOW_BENCH
#error "Usar somente o ambiente int006-check"
#endif

namespace {
#ifdef OBC_MODES_BENCH
    bool boot_ready = false;
    bool detumbling = false;
    unsigned detumbling_starts = 0;
#ifdef OBC_PROTECTION_BENCH
    uint64_t protection_started_ms = 0;
    constexpr obc_protection::TEMPERATURE_LIMITS bench_temperature = {10, 40, 15, 35};
    constexpr obc_protection::POLICY bench_protection_policy = {
        6, 7, {bench_temperature, bench_temperature, bench_temperature, bench_temperature, bench_temperature},
        3000, 3000, 2000
    };
    obc_protection::Evaluator protection(bench_protection_policy, 0);

    // Uma nova amostra por segundo. O calor continua após a recuperação da bateria.
    obc_protection::RESULT ReadBenchProtection() {
        const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
        const uint64_t elapsed_ms = now_ms - protection_started_ms;
        const uint64_t sample_ms = now_ms - elapsed_ms % 1000;
        obc_protection::READINGS readings = {};
        if (now_ms >= 1000) {
            const bool episode = protection_started_ms != 0;
            readings.battery = {true, episode && elapsed_ms < 4000 ? 6.0f : 8.0f, sample_ms};
            for (auto& temperature : readings.temperatures) temperature = {true, 25.0f, sample_ms};
            if (episode && elapsed_ms >= 3000 && elapsed_ms < 7000) readings.temperatures[1].value = 40.0f;
        }
        return protection.Evaluate(now_ms, readings);
    }
#else
    uint64_t protection_ends_ms = 0;
#endif

    // A segunda partida inicia o episódio de proteção do perfil selecionado.
    bool StartBenchAdcs() {
        if (detumbling) return false;
        detumbling = true;
        if (++detumbling_starts == 2) {
#ifdef OBC_PROTECTION_BENCH
            protection_started_ms = obc_time::esp32::GetMonotonicMs();
#else
            protection_ends_ms = obc_time::esp32::GetMonotonicMs() + 5000;
#endif
        }
        return true;
    }

    // A bancada confirma a parada sem acessar o driver do motor.
    bool StopBenchAdcs() {
        detumbling = false;
        return true;
    }

    // Os demais pré-requisitos vêm da recepção, NVS e escritor usados pela bancada.
    obc_modes::SUBSYSTEM_STATUS ReadBenchModeStatus() {
#ifdef OBC_PROTECTION_BENCH
        return {boot_ready, true, false, !detumbling, false, false};
#else
        const bool protection_started = protection_ends_ms != 0;
        const bool protection_active = protection_started && obc_time::esp32::GetMonotonicMs() < protection_ends_ms;
        return {boot_ready, true, false, !detumbling, protection_active, protection_started && !protection_active};
#endif
    }

    const obc_ttc::MODE_SERVICES bench_modes = {
        4000, ReadBenchModeStatus, StartBenchAdcs, StopBenchAdcs,
#ifdef OBC_PROTECTION_BENCH
        ReadBenchProtection
#else
        nullptr
#endif
    };
#endif
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
#ifdef OBC_STORAGE_RECOVERY_BENCH
    bool simulated_mounted = true;
    bool simulated_failed = false;
    uint8_t recovery_phase = 0;
    bool first_mount_failed = false;
    char used_names[64][obc_storage::filename_chars + 1] = {};
    size_t used_name_count = 0;
#endif
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
#ifdef OBC_STORAGE_RECOVERY_BENCH
        if (!simulated_mounted) return {obc_storage::STATUS::NOT_INITIALIZED, 0, 0, 0};
        if (simulated_failed) return {obc_storage::STATUS::WRITER_FAILED, 0, 0, 0};
        for (size_t index = 0; index < used_name_count; ++index) {
            if (!std::strcmp(name, used_names[index])) return {obc_storage::STATUS::ALREADY_EXISTS, 0, EEXIST, 0};
        }
        if (used_name_count == 64) return {obc_storage::STATUS::NO_SPACE, 0, ENOSPC, 0};
        std::strcpy(used_names[used_name_count++], name);
#endif
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
#ifdef OBC_TASK_HEALTH_BENCH
        const bool health_normal = size >= 6 && (!std::memcmp(bytes, obc_record::telemetry_magic, 4)
            || (!std::memcmp(bytes, obc_data::record_magic, 4)
                && bytes[5] == static_cast<uint8_t>(obc_data::RECORD_TYPE::STATISTICS)));
        if (health_normal && FailHealthBenchWrite()) return {obc_storage::STATUS::WRITE_FAILED, 0, EIO, 3};
#endif
#ifdef OBC_STORAGE_RECOVERY_BENCH
        if (simulated_failed) return {obc_storage::STATUS::WRITER_FAILED, 0, 0, 0};
        const bool normal = size >= 6 && (!std::memcmp(bytes, obc_record::telemetry_magic, 4)
            || (!std::memcmp(bytes, obc_data::record_magic, 4)
                && bytes[5] == static_cast<uint8_t>(obc_data::RECORD_TYPE::STATISTICS)));
        if (normal && ((recovery_phase == 0 && delivery_file_count >= 14)
                || recovery_phase == 3 || recovery_phase == 6)) {
            ++recovery_phase;
            simulated_failed = true;
            Serial.printf("[STORAGE-CHECK] fault=%u monotonic_ms=%llu\n", recovery_phase,
                static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
            return {obc_storage::STATUS::WRITE_FAILED, 0, EIO, 3};
        }
#endif
        const uint32_t before = obc_record::ComputeCrc32(bytes, size);
        vTaskDelay(pdMS_TO_TICKS(100));
        if (before != obc_record::ComputeCrc32(bytes, size)) {
            Serial.printf("[FLOW-CHECK] FAIL: buffer alterado durante escrita\n");
            return {obc_storage::STATUS::WRITE_FAILED, 0, 0, 1};
        }
#ifdef OBC_RESOURCES_BENCH
        obc_resources_bench::ObserveRecord(bytes, size);
#endif
#ifdef OBC_WATCHDOG_BENCH_CASE
        obc_watchdog_bench::ObserveRecord(bytes, size);
#endif
#ifdef OBC_TASK_HEALTH_BENCH
        if (size == 96 && !std::memcmp(bytes, obc_data::record_magic, 4)) {
            obc_record::RECORD event = {};
            size_t consumed = 0;
            if (obc_record::Decode(bytes, size, event, consumed) != obc_record::STATUS::OK) {
                Serial.printf("[HEALTH-CHECK] FAIL: evento invalido\n");
                return {obc_storage::STATUS::WRITE_FAILED, 0, EIO, 1};
            }
            if (event.body.event.code >= 256 && event.body.event.code < 259) {
                Serial.printf("[HEALTH-CHECK] record_code=%u kind=%s\n", event.body.event.code,
                    event.header.type == obc_data::RECORD_TYPE::FAULT ? "fault" : "event");
            }
        }
#endif
#ifdef OBC_I2C_HEALTH_BENCH
        if (size == 96 && !std::memcmp(bytes, obc_data::record_magic, 4)) {
            obc_record::RECORD event = {};
            size_t consumed = 0;
            if (obc_record::Decode(bytes, size, event, consumed) == obc_record::STATUS::OK
                    && (event.header.type == obc_data::RECORD_TYPE::FAULT || event.header.type == obc_data::RECORD_TYPE::EVENT)
                    && event.body.event.source == obc_data::SOURCE::OBC && event.body.event.code == obc_i2c_health::event_code) {
                Serial.printf("[I2C-HEALTH-CHECK] record_code=259 kind=%s\n",
                    event.header.type == obc_data::RECORD_TYPE::FAULT ? "fault" : "event");
            }
        }
#endif
#ifdef OBC_MODES_BENCH
        if (size == 96 && !std::memcmp(bytes, obc_data::record_magic, 4)
                && (bytes[5] == static_cast<uint8_t>(obc_data::RECORD_TYPE::EVENT)
                    || bytes[5] == static_cast<uint8_t>(obc_data::RECORD_TYPE::FAULT))) {
            obc_record::RECORD event = {};
            size_t consumed = 0;
            if (obc_record::Decode(bytes, size, event, consumed) == obc_record::STATUS::OK
                    && event.body.event.source == obc_data::SOURCE::OBC) {
                const auto at_ms = static_cast<unsigned long long>(event.header.stamp.monotonic_ms);
                if (event.body.event.code == obc_modes::event_code) {
                    Serial.printf("[MODES-SD] event ms=%llu %s\n", at_ms, event.body.event.description);
                }
#ifdef OBC_PROTECTION_BENCH
                if (event.body.event.code == obc_protection::event_code) {
                    const bool fault = event.header.type == obc_data::RECORD_TYPE::FAULT;
                    Serial.printf("[PROTECTION-SD] %s ms=%llu %s\n",
                        fault ? "fault" : "event", at_ms, event.body.event.description);
                }
#endif
            }
        }
        if (size >= 24 && !std::memcmp(bytes, obc_record::snapshot_magic, 4)) {
            uint64_t sampled_ms = 0;
            // O ESP32 e o campo em offset 16 do cabeçalho v2 usam little-endian.
            std::memcpy(&sampled_ms, bytes + 16, sizeof(sampled_ms));
            Serial.printf("[MODES-SD] snapshot ms=%llu\n", static_cast<unsigned long long>(sampled_ms));
        }
#endif
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
        if (size >= 4 && std::memcmp(bytes, obc_record::telemetry_magic, 4) == 0) {
            obc_record::RECORD record = {};
            size_t consumed = 0;
            if (obc_record::DecodeTelemetryV3(bytes, size, record, consumed) != obc_record::STATUS::OK) {
                Serial.printf("[FLOW-CHECK] FAIL: registro v3 invalido\n");
                return {obc_storage::STATUS::WRITE_FAILED, 0, 0, 1};
            }
            const auto& tm = record.body.telemetry;
#ifdef OBC_MODES_BENCH
            Serial.printf("[MODES-SD] tm md=%s\n", ttc::ModeName(static_cast<ttc::OP_MODE>(tm.mode)));
#endif
            Serial.printf("[FLOW-CHECK] v3 ts_available=%u ts=%lu ac_available=%u ac=%u msg_available=%u stable=1\n",
                (tm.available_fields & obc_data::TM_TIMESTAMP) ? 1U : 0U,
                static_cast<unsigned long>(tm.timestamp),
                (tm.available_fields & obc_data::TM_AIRCRAFT) ? 1U : 0U,
                tm.aircraft_count, (tm.available_fields & obc_data::TM_MESSAGES) ? 1U : 0U);
        }
#ifdef OBC_STORAGE_RECOVERY_BENCH
        if (size == 36 && !std::memcmp(bytes, obc_data::record_magic, 4)
                && bytes[5] == static_cast<uint8_t>(obc_data::RECORD_TYPE::TIME_REFERENCE)) {
            obc_record::RECORD reference = {};
            size_t consumed = 0;
            if (obc_record::Decode(bytes, size, reference, consumed) != obc_record::STATUS::OK) {
                Serial.printf("[STORAGE-CHECK] FAIL: referencia invalida\n");
                return {obc_storage::STATUS::WRITE_FAILED, 0, EIO, 1};
            }
            Serial.printf("[STORAGE-CHECK] reference_unix=%lu phase=%u\n",
                static_cast<unsigned long>(reference.body.time_reference.unix_s), recovery_phase);
        }
        if (normal && (recovery_phase == 2 || recovery_phase == 5)) {
            ++recovery_phase;
            Serial.printf("[STORAGE-CHECK] normal_write=%u monotonic_ms=%llu\n", recovery_phase,
                static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
        }
#endif
#ifdef OBC_HOUSEKEEPING_BENCH
        // Somente os bytes aceitos; nenhum registro de escrita recusada é capturado.
        obc_housekeeping::bench::CaptureRecord(bytes, size);
#endif
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

#ifdef OBC_TASK_HEALTH_BENCH
    // Somente ciclo lógico da bancada; conserva arquivos FCD1 já fechados.
    obc_storage::RESULT EndHealthBench(
        void*
    ) { return CloseBench(nullptr); }

    obc_storage::RESULT InitHealthBench(
        void*
    ) { return {obc_storage::STATUS::OK, 0, 0, 0}; }
#endif

#ifdef OBC_STORAGE_RECOVERY_BENCH
    // Desmontagem conserva os arquivos simulados; erro não desfaz a liberação.
    obc_storage::RESULT EndBench(
        void*
    ) {
        CloseBench(nullptr);
        simulated_mounted = false;
        simulated_failed = false;
        return {recovery_phase == 1 ? obc_storage::STATUS::CLOSE_FAILED : obc_storage::STATUS::OK, 0, 0, 0};
    }

    // Primeiro episódio falha uma vez; segundo recupera; terceiro esgota o limite.
    obc_storage::RESULT InitBench(
        void*
    ) {
        const bool failed = recovery_phase == 7 || (recovery_phase == 1 && !first_mount_failed);
        Serial.printf("[STORAGE-CHECK] mount_begin=%u monotonic_ms=%llu\n", recovery_phase,
            static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
        // Permite atualizar set_time enquanto o escritor aguarda a montagem.
        vTaskDelay(pdMS_TO_TICKS(300));
        if (recovery_phase == 1) first_mount_failed = true;
        if (!failed) {
            simulated_mounted = true;
            recovery_phase = recovery_phase == 1 ? 2 : 5;
        }
        Serial.printf("[STORAGE-CHECK] mount=%u phase=%u files=%u monotonic_ms=%llu\n", failed ? 0U : 1U,
            recovery_phase, static_cast<unsigned>(delivery_file_count),
            static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
        return {failed ? obc_storage::STATUS::MOUNT_FAILED : obc_storage::STATUS::OK, 0, 0, 0};
    }
#endif

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

RECOVERY_IO GetBenchRecoveryIO(
) {
#ifdef OBC_STORAGE_RECOVERY_BENCH
    return {EndBench, InitBench, nullptr};
#elif defined(OBC_TASK_HEALTH_BENCH)
    return {EndHealthBench, InitHealthBench, nullptr};
#else
    return {};
#endif
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
#ifdef OBC_WATCHDOG_BENCH_CASE
    obc_watchdog_bench::Start();
#endif
#ifdef OBC_RESOURCES_BENCH
    obc_resources_bench::Start();
#endif
#ifdef OBC_I2C_HEALTH_BENCH
    obc_flow::esp32::StartI2cHealthBench();
#endif
#ifdef OBC_TASK_HEALTH_BENCH
    obc_flow::esp32::StartHealthBench();
#endif
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
#elif defined(OBC_MODES_BENCH)
        next_ms = now_ms + 250;
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
#ifdef OBC_BUFFERED_FRAMEWORK_LOG
    if (!obc_bench_serial::Init()) {
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
#else
#ifdef OBC_HOUSEKEEPING_BENCH
    // O buffer nativo absorve rajadas sem mudar baudrate ou descartar linhas.
    // setTxBufferSize configura o tamanho; begin realiza a alocação no driver.
    const bool tx_configured = Serial.setTxBufferSize(8192) == 8192;
#endif
    Serial.begin(115200);
#ifdef OBC_HOUSEKEEPING_BENCH
    if (!tx_configured || !Serial) {
        // Uma bancada sem transporte não pode anunciar prontidão funcional.
        if (Serial) Serial.println("[HK-CHECK] FAIL: UART0 TX buffer unavailable");
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
#endif
#ifdef OBC_WATCHDOG_ENABLED
    obc_watchdog::esp32::Init();
#endif
#ifdef OBC_STORAGE_RECOVERY_BENCH
    const bool reservation = obc_delivery::check::CheckRecoveryReservation();
    Serial.printf("[DELIVERY-RECOVERY-CHECK] %s\n", reservation ? "PASS" : "FAIL");
    if (!reservation) {
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
#ifdef OBC_CAPACITY_BENCH
    Serial.printf("[LIMITS] scenario=capacity files=66\n");
#elif defined(OBC_MEMORY_BENCH)
    Serial.printf("[LIMITS] scenario=memory files=32\n");
#endif
#ifdef OBC_STRESS_BENCH
    Serial.printf("[STRESS] ativo: read lento/busy/io; NO_SPACE apos 8 arquivos FCD1\n");
#endif
#ifdef OBC_STORAGE_RECOVERY_BENCH
    Serial.printf("[STORAGE-CHECK] ativo: 3 tentativas/episodio, intervalo 5000 ms, sem reboot\n");
#endif
    Serial.printf("[FLOW-CHECK] ADS-B em RAM e SD lento simulados; sem ensaio ESP32-Pi/cartao\n");
    obc_startup::STARTUP_REPORT report = {};
    report.time_status = obc_time::esp32::StartBoot(report.boot_id);
#ifdef OBC_WATCHDOG_BENCH_CASE
    obc_watchdog_bench::Boot(report.boot_id);
#endif
#ifdef OBC_WATCHDOG_ENABLED
    obc_watchdog::esp32::Checkpoint();
#if defined(OBC_WATCHDOG_BENCH_CASE) && OBC_WATCHDOG_BENCH_CASE == 4
    // Cenário I2C isolado: conserva os buffers originais nos demais ambientes.
    report.i2c_ready = i2c::Init();
    obc_watchdog::esp32::Checkpoint();
#endif
#endif
#ifdef OBC_HOUSEKEEPING_BENCH
    // Somente receptor antecipado; o estágio SD será reservado pelo tamanho real.
    if (report.time_status == obc_time::STATUS::OK) {
        obc_flow::esp32::ReserveHousekeepingBuffers(report.boot_id);
    }
#endif
    report.storage_result.status = obc_storage::STATUS::OK;
#ifdef OBC_MODES_BENCH
    boot_ready = report.time_status == obc_time::STATUS::OK;
    obc_ttc::Init(&bench_modes);
#else
    obc_ttc::Init();
#endif
#ifdef OBC_WATCHDOG_ENABLED
    obc_watchdog::esp32::Checkpoint();
#endif
    obc_flow::esp32::Init(report);
#ifdef OBC_HOUSEKEEPING_BENCH
    // As pilhas maiores já existem; a admissão contabilizou I2C e produtores pendentes.
    report.i2c_ready = i2c::Init();
    obc_housekeeping::bench::Init();
#endif
#ifdef OBC_WATCHDOG_ENABLED
    obc_watchdog::esp32::Checkpoint();
    obc_watchdog::esp32::StartRuntime();
#endif
}

/** @brief Mantém o mesmo atendimento de comandos e diagnósticos do firmware central. */
void loop(
) {
#ifdef OBC_WATCHDOG_BENCH_CASE
    obc_watchdog_bench::BeforeLoop();
#endif
    obc_ttc::Service();
    obc_flow::esp32::ReportDiagnostics();
#ifdef OBC_HOUSEKEEPING_BENCH
    obc_housekeeping::bench::ReportDiagnostics();
#endif
#ifdef OBC_WATCHDOG_ENABLED
    obc_watchdog::esp32::Service(obc_ttc::GetDiagnostics());
#endif
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
