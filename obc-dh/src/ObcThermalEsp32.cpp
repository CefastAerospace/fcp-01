// Somente a bancada térmica inclui o driver; os ambientes anteriores o excluem.
#ifdef OBC_THERMAL_BENCH
#include "ObcThermalEsp32.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcThermalEsp32.h".
 */

namespace obc_thermal {
namespace esp32 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    SNAPSHOT published = {};
    DIAGNOSTICS diagnostics = {};
    portMUX_TYPE publication_lock = portMUX_INITIALIZER_UNLOCKED;
    TaskHandle_t worker_task = nullptr;
    bool init_attempted = false;
    bool init_active = false;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Contexto preservado até ExecuteChecked retornar, sem saída parcial. */
    struct REQUEST {
        uint8_t address;
        ACTION action;
        uint64_t recovery_cycles;
        bool epoch_changed;
        uint16_t raw;
        CEFAST::thermal::STATUS status;
        CEFAST::thermal::DIAGNOSTIC bus;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Único ponto de I/O; a guarda observa recuperação ocorrida enquanto enfileirado.
    i2c::OPERATION_RESULT ExecuteRequest(
        TwoWire& wire,
        void* context
    ) {
        auto& request = *static_cast<REQUEST*>(context);
        const auto bus = i2c::GetDiagnostics();
        const bool allowed = CanExecute(request.action, request.recovery_cycles,
            bus.recovery_cycles, bus.state == i2c::SERVICE_STATE::READY);
        request.recovery_cycles = bus.recovery_cycles;
        if (!allowed) {
            request.epoch_changed = true;
            return i2c::OPERATION_RESULT::OPERATION_FAILED;
        }
        request.status = request.action == ACTION::CONFIGURE
            ? CEFAST::thermal::Configure(wire, request.address, request.bus)
            : CEFAST::thermal::ReadRaw(wire, request.address, request.raw, request.bus);
        return obc_eps::MapWireFailure(request.status != CEFAST::thermal::STATUS::OK,
            request.status == CEFAST::thermal::STATUS::BUFFER_ERROR,
            request.status == CEFAST::thermal::STATUS::READ_INCOMPLETE, request.bus.wire_status);
    }

    // Contadores não voltam a zero por overflow.
    void Count(
        uint64_t& value
    ) { if (value != std::numeric_limits<uint64_t>::max()) ++value; }

    // Metadados são publicados antes da criação da tarefa, sem simular tentativa.
    SNAPSHOT MakeInitialSnapshot(
    ) {
        SNAPSHOT snapshot = {};
        for (size_t index = 0; index < channel_count; ++index) {
            snapshot.channels[index].address = addresses[index];
            snapshot.channels[index].last_status = CEFAST::thermal::STATUS::NOT_ATTEMPTED;
            snapshot.channels[index].result = i2c::STATUS::NOT_INITIALIZED;
        }
        return snapshot;
    }

    // Conversão, cópias e espera pertencem ao consumidor, fora do executor e do lock.
    void RunWorker(
        void*
    ) {
        Channel channels[channel_count];
        SNAPSHOT snapshot = MakeInitialSnapshot();
        DIAGNOSTICS progress = {};
        progress.attempted = true;
        for (;;) {
            for (size_t index = 0; index < channel_count; ++index) {
                const auto bus = i2c::GetDiagnostics();
                auto& sample = snapshot.channels[index];
                const ACTION action = channels[index].Next(obc_time::esp32::GetMonotonicMs(),
                    bus.state == i2c::SERVICE_STATE::READY, bus.recovery_cycles);
                if (action != ACTION::WAIT) {
                    REQUEST request = {};
                    request.address = sample.address;
                    request.action = action;
                    request.recovery_cycles = bus.recovery_cycles;
                    sample.result = i2c::ExecuteChecked(ExecuteRequest, &request,
                        request_priority, queue_wait_ms);
                    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
                    bool valid = false;
                    float temperature_c = 0;
                    if (sample.result == i2c::STATUS::OK && action == ACTION::READ) {
                        valid = CEFAST::thermal::DecodeTemperature(request.raw, temperature_c);
                        if (!valid) request.status = CEFAST::thermal::STATUS::OUT_OF_RANGE;
                    }
                    Count(progress.operations);
                    if (sample.result != i2c::STATUS::OK
                            || request.status != CEFAST::thermal::STATUS::OK) Count(progress.failures);
                    if (request.epoch_changed) {
                        channels[index].Next(now_ms, false, request.recovery_cycles);
                    } else {
                        channels[index].Complete(action, now_ms, request.recovery_cycles,
                            valid, sample.result == i2c::STATUS::OK);
                    }
                    if (valid) sample.temperature_c = temperature_c;
                    sample.last_status = request.status;
                    sample.bus = request.bus;
                }
                sample.state = channels[index].GetState();
            }

            progress.active = true;
            progress.service = i2c::GetDiagnostics().state;
            progress.progress_ms = obc_time::esp32::GetMonotonicMs();
            progress.stack_free_bytes = uxTaskGetStackHighWaterMark(nullptr);
            portENTER_CRITICAL(&publication_lock);
            published = snapshot;
            diagnostics = progress;
            portEXIT_CRITICAL(&publication_lock);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

bool Init(
) {
    if (init_attempted) return init_active;
    init_attempted = true;
    DIAGNOSTICS initial_diagnostics = {};
    initial_diagnostics.attempted = true;
    initial_diagnostics.service = i2c::GetDiagnostics().state;
    const SNAPSHOT initial_snapshot = MakeInitialSnapshot();
    portENTER_CRITICAL(&publication_lock);
    published = initial_snapshot;
    diagnostics = initial_diagnostics;
    portEXIT_CRITICAL(&publication_lock);
    if (initial_diagnostics.service != i2c::SERVICE_STATE::READY) return false;
    if (xTaskCreatePinnedToCore(RunWorker, "thermal_lm75a", worker_stack_bytes,
            nullptr, 1, &worker_task, 1) != pdPASS) return false;
    init_active = true;
    return true;
}

SNAPSHOT GetSnapshot(
) {
    portENTER_CRITICAL(&publication_lock);
    SNAPSHOT snapshot = published;
    portEXIT_CRITICAL(&publication_lock);
    const auto bus = i2c::GetDiagnostics();
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    for (auto& sample : snapshot.channels) {
        sample.available = IsAvailable(sample.state, now_ms,
            bus.state == i2c::SERVICE_STATE::READY, bus.recovery_cycles);
        sample.age_ms = sample.state.has_valid && now_ms >= sample.state.last_valid_ms
            ? now_ms - sample.state.last_valid_ms : 0;
    }
    return snapshot;
}

DIAGNOSTICS GetDiagnostics(
) {
    portENTER_CRITICAL(&publication_lock);
    const DIAGNOSTICS result = diagnostics;
    portEXIT_CRITICAL(&publication_lock);
    return result;
}

void ReportDiagnostics(
) {
    static uint64_t last_ms = 0;
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    if (now_ms - last_ms < acquisition_period_ms) return;
    last_ms = now_ms;
    const auto progress = GetDiagnostics();
    if (!progress.active) {
        Serial.printf("[THERMAL] state=%s service=%u\n",
            init_active ? "starting" : progress.attempted ? "inactive" : "not_initialized",
            static_cast<unsigned>(progress.service));
        return;
    }
    const auto snapshot = GetSnapshot();
    for (size_t index = 0; index < channel_count; ++index) {
        const auto& sample = snapshot.channels[index];
        char current[32] = "null";
        char historical[32] = "null";
        if (sample.state.has_valid) std::snprintf(historical, sizeof(historical), "%g", sample.temperature_c);
        if (sample.available) std::snprintf(current, sizeof(current), "%g", sample.temperature_c);
        char line[420];
        const int length = std::snprintf(line, sizeof(line),
            "[THERMAL] channel=thermal_%u address=0x%02x available=%u has_valid=%u age_ms=%llu attempted=%u attempted_ms=%llu last_valid_ms=%llu driver=%u result=%u phase=%u status=%u received=%u temperature_C=%s last_valid_C=%s\n",
            static_cast<unsigned>(index + 1), sample.address, sample.available ? 1U : 0U,
            sample.state.has_valid ? 1U : 0U, static_cast<unsigned long long>(sample.age_ms),
            sample.state.attempted ? 1U : 0U, static_cast<unsigned long long>(sample.state.attempted_ms),
            static_cast<unsigned long long>(sample.state.last_valid_ms),
            static_cast<unsigned>(sample.last_status), static_cast<unsigned>(sample.result),
            static_cast<unsigned>(sample.bus.phase), sample.bus.wire_status, sample.bus.received,
            current, historical);
        if (length > 0 && static_cast<size_t>(length) < sizeof(line)) {
            Serial.write(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(length));
        }
    }
    Serial.printf("[THERMAL-HEALTH] active=%u service=%u progress_ms=%llu age_ms=%llu operations=%llu failures=%llu stack_free_bytes=%lu\n",
        progress.active ? 1U : 0U, static_cast<unsigned>(progress.service),
        static_cast<unsigned long long>(progress.progress_ms),
        static_cast<unsigned long long>(now_ms >= progress.progress_ms ? now_ms - progress.progress_ms : 0),
        static_cast<unsigned long long>(progress.operations),
        static_cast<unsigned long long>(progress.failures),
        static_cast<unsigned long>(progress.stack_free_bytes));
}

} // namespace esp32
} // namespace obc_thermal
#endif // OBC_THERMAL_BENCH
