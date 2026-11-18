#ifdef OBC_HOUSEKEEPING_BENCH
#include "ObcHousekeepingBench.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcHousekeepingBench.h".
 */

namespace obc_housekeeping {
namespace bench {
namespace {

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Última recusa do SDK; function aponta para nome estático fornecido pelo allocator. */
    struct ALLOCATION_FAILURE {
        size_t size;
        uint32_t caps;
        const char* function;
        uint32_t count;
    };

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    ALLOCATION_FAILURE allocation_failure = {};
    portMUX_TYPE allocation_lock = portMUX_INITIALIZER_UNLOCKED;
    bool allocation_hook_attempted = false;
    esp_err_t allocation_hook_status = ESP_FAIL;

#ifdef OBC_HOUSEKEEPING_FIXTURES
    bool fixtures_started = false;
    uint64_t fixtures_start_ms = 0;
    const char* const source = "fixtures";
#else
    const char* const source = "acquisition";
#endif

    // ─── HELPERS ──────────────────────────────────────────────────────

    // O callback não consulta heap nem imprime: copia somente metadados em proteção curta.
    void ObserveAllocationFailure(
        size_t size,
        uint32_t caps,
        const char* function
    ) {
        portENTER_CRITICAL(&allocation_lock);
        allocation_failure.size = size;
        allocation_failure.caps = caps;
        allocation_failure.function = function;
        if (allocation_failure.count != std::numeric_limits<uint32_t>::max()) ++allocation_failure.count;
        portEXIT_CRITICAL(&allocation_lock);
    }

    // Setup sequencial: registra antes da primeira criação, mesmo com produtores iniciados depois.
    esp_err_t RegisterAllocationDiagnostics(
    ) {
        if (!allocation_hook_attempted) {
            allocation_hook_attempted = true;
            allocation_hook_status = heap_caps_register_failed_alloc_callback(ObserveAllocationFailure);
        }
        return allocation_hook_status;
    }

#ifdef OBC_HOUSEKEEPING_FIXTURES
    // Cenários finitos de cinco segundos; a primeira TM inicia a sequência.
    READINGS ReadFixtures(
        uint64_t sampled_ms,
        const char*& scenario
    ) {
        if (!fixtures_started) {
            fixtures_start_ms = sampled_ms;
            fixtures_started = true;
        }
        const uint64_t elapsed = sampled_ms - fixtures_start_ms;
        const size_t phase = elapsed / 5000 < 6 ? static_cast<size_t>(elapsed / 5000) : 6;
        const char* const scenarios[] = {"normal", "zero_negative", "battery_missing",
            "solar_missing", "thermal_partial", "unavailable", "recovered"};
        scenario = scenarios[phase];
        READINGS readings = {};
        readings.battery = {true, 7.8f, 0.42f};
        readings.solar = {true, 9.1f, 0.31f};
        const float temperatures[] = {21.5f, 22.0f, 23.1f, 20.9f, 24.3f};
        for (size_t index = 0; index < obc_data::thermal_sensors; ++index) {
            readings.temperatures[index] = {true, temperatures[index]};
        }
        if (phase == 1) {
            readings.battery = {true, 0, 0};
            readings.solar.current_a = -0.31f;
            readings.temperatures[0].temperature_c = 0;
            readings.temperatures[1].temperature_c = -12.5f;
        } else if (phase == 2) readings.battery.available = false;
        else if (phase == 3) readings.solar.available = false;
        else if (phase == 4) readings.temperatures[2].available = false;
        else if (phase == 5) {
            readings.battery.available = false;
            readings.solar.available = false;
            for (auto& temperature : readings.temperatures) temperature.available = false;
        }
        return readings;
    }
#endif

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

void Init(
) {
    const esp_err_t hook_status = RegisterAllocationDiagnostics();
    bool eps_started = false;
    bool thermal_started = false;
#ifndef OBC_HOUSEKEEPING_FIXTURES
    eps_started = obc_eps::esp32::Init();
    thermal_started = obc_thermal::esp32::Init();
#endif
    char line[200];
    const int length = std::snprintf(line, sizeof(line),
        "[HK-CHECK] source=%s ready=1 voltage=vin_minus current=A_to_mA eps_started=%u thermal_started=%u hook_status=%d\n",
        source, eps_started ? 1U : 0U, thermal_started ? 1U : 0U, static_cast<int>(hook_status));
    if (length > 0 && static_cast<size_t>(length) < sizeof(line)) {
        Serial.write(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(length));
    }
}

READINGS Read(
    uint64_t sampled_ms
) {
    const char* scenario = "no_sensors";
    READINGS readings = {};
#ifdef OBC_HOUSEKEEPING_FIXTURES
    readings = ReadFixtures(sampled_ms, scenario);
#else
    const auto eps = obc_eps::esp32::GetSnapshot();
    const auto& battery = eps.channels[static_cast<size_t>(CEFAST::eps::INAChannel::Battery)];
    const auto& solar = eps.channels[static_cast<size_t>(CEFAST::eps::INAChannel::Solar)];
    readings.battery = {battery.available, battery.reading.busVoltage_V, battery.reading.current_A};
    readings.solar = {solar.available, solar.reading.busVoltage_V, solar.reading.current_A};
    const auto thermal = obc_thermal::esp32::GetSnapshot();
    for (size_t index = 0; index < obc_data::thermal_sensors; ++index) {
        readings.temperatures[index] = {thermal.channels[index].available,
            thermal.channels[index].temperature_c};
    }
#endif
    char line[150];
    const int length = std::snprintf(line, sizeof(line),
        "[HK-CHECK] source=%s scenario=%s sample_ms=%llu\n",
        source, scenario, static_cast<unsigned long long>(sampled_ms));
    if (length > 0 && static_cast<size_t>(length) < sizeof(line)) {
        Serial.write(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(length));
    }
    return readings;
}

void ReportDiagnostics(
) {
#ifndef OBC_HOUSEKEEPING_FIXTURES
    obc_eps::esp32::ReportDiagnostics();
    obc_thermal::esp32::ReportDiagnostics();
#endif
    static uint64_t last_ms = 0;
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    if (now_ms - last_ms < 1000) return;
    last_ms = now_ms;
    char line[200];
    const int length = std::snprintf(line, sizeof(line),
        "[HK-HEALTH] source=%s heap_bytes=%u block_bytes=%u loop_stack_bytes=%u\n", source,
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    if (length > 0 && static_cast<size_t>(length) < sizeof(line)) {
        Serial.write(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(length));
    }
}

void TraceTaskCreation(
    const char* task,
    bool after,
    size_t stack_bytes,
    int result
) {
    RegisterAllocationDiagnostics();
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const size_t free_bytes = heap_caps_get_free_size(caps);
    const size_t largest_bytes = heap_caps_get_largest_free_block(caps);
    portENTER_CRITICAL(&allocation_lock);
    const auto failure = allocation_failure;
    portEXIT_CRITICAL(&allocation_lock);
    char line[320];
    const int length = std::snprintf(line, sizeof(line),
        "[HK-ALLOC] task=%s phase=%s stack_bytes=%u result=%d byte_heap=%u byte_largest=%u refusals=%u refused_bytes=%u refused_caps=0x%lx function=%s\n",
        task, after ? "after" : "before", static_cast<unsigned>(stack_bytes), result,
        static_cast<unsigned>(free_bytes), static_cast<unsigned>(largest_bytes), failure.count,
        static_cast<unsigned>(failure.size), static_cast<unsigned long>(failure.caps),
        failure.function ? failure.function : "none");
    if (length > 0 && static_cast<size_t>(length) < sizeof(line)) {
        Serial.write(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(length));
    }
}

void CaptureRecord(
    const uint8_t* bytes,
    size_t size
) {
    const bool telemetry = size == obc_record::telemetry_record_bytes
        && !std::memcmp(bytes, obc_record::telemetry_magic, 4);
    const bool metadata = size >= obc_data::header_bytes
        && !std::memcmp(bytes, obc_data::record_magic, 4)
        && (bytes[5] == static_cast<uint8_t>(obc_data::RECORD_TYPE::BOOT)
            || bytes[5] == static_cast<uint8_t>(obc_data::RECORD_TYPE::TIME_REFERENCE));
    if ((!telemetry && !metadata) || size > obc_record::telemetry_record_bytes) return;
    char line[2 * obc_record::telemetry_record_bytes + 32];
    size_t length = static_cast<size_t>(std::snprintf(line, sizeof(line), "[HK-RECORD] hex="));
    const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < size; ++index) {
        line[length++] = digits[bytes[index] >> 4];
        line[length++] = digits[bytes[index] & 0x0F];
    }
    line[length++] = '\n';
    Serial.write(reinterpret_cast<const uint8_t*>(line), length);
}

} // namespace bench
} // namespace obc_housekeeping
#endif
