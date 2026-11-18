// Apenas o ambiente aprovado consome o driver EPS; builds históricos não o incluem.
#ifdef OBC_EPS_BENCH
#include "ObcEpsEsp32.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcEpsEsp32.h".
 */

namespace obc_eps {
namespace esp32 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    using namespace CEFAST::eps;
    EPS_INA219 driver;
    SNAPSHOT published = {};
    DIAGNOSTICS diagnostics = {};
    portMUX_TYPE publication_lock = portMUX_INITIALIZER_UNLOCKED;
    TaskHandle_t worker_task = nullptr;
#if defined(OBC_HOUSEKEEPING_BENCH) && !defined(OBC_HOUSEKEEPING_FIXTURES)
    // Memória pertence ao produtor por toda sua vida; nenhuma reserva tardia no heap.
    StaticTask_t worker_control;
    alignas(portBYTE_ALIGNMENT) StackType_t worker_stack[worker_stack_bytes / sizeof(StackType_t)];
    static_assert(sizeof(worker_stack) == worker_stack_bytes, "Pilha EPS deve manter 4096 bytes");
#endif
    bool init_attempted = false;
    bool init_active = false;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Contexto vivo até ExecuteChecked retornar, mesmo após timeout de fila. */
    struct REQUEST {
        INAChannel channel;
        ACTION action;
        uint64_t recovery_cycles;
        bool epoch_changed;
        bool online;
        bool valid;
        INAReading reading;
        I2C_DIAGNOSTIC bus;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Único ponto que acessa o driver; executa na tarefa I2C, sem logs ou espera.
    i2c::OPERATION_RESULT ExecuteRequest(
        TwoWire& wire,
        void* context
    ) {
        auto& request = *static_cast<REQUEST*>(context);
        const uint64_t cycles = i2c::GetDiagnostics().recovery_cycles;
        request.epoch_changed = request.action == ACTION::READ && cycles != request.recovery_cycles;
        request.recovery_cycles = cycles;
        if (request.epoch_changed) return i2c::OPERATION_RESULT::OPERATION_FAILED;
        bool success = false;
        if (request.action == ACTION::CONFIGURE) {
            success = driver.begin(wire, request.channel);
            request.reading.channel = request.channel;
            request.reading.address = driver.address(request.channel);
            request.reading.status = success ? INAStatus::Ok : INAStatus::I2CError;
        } else {
            request.reading = driver.read(request.channel);
            request.valid = request.reading.valid && request.reading.conversionReady && !request.reading.overflow;
            success = request.valid;
        }
        request.online = driver.isOnline(request.channel);
        request.bus = driver.GetI2cDiagnostic();
        const auto result = MapWireFailure(request.bus.failed,
            request.bus.phase == I2C_PHASE::BUFFER, request.bus.phase == I2C_PHASE::READ,
            request.bus.status);
        return result != i2c::OPERATION_RESULT::OK ? result : success
            ? i2c::OPERATION_RESULT::OK : i2c::OPERATION_RESULT::OPERATION_FAILED;
    }

    // Contadores diagnósticos não podem voltar a zero por overflow.
    void Count(
        uint64_t& value
    ) { if (value != std::numeric_limits<uint64_t>::max()) ++value; }

    // Metadados existem antes da tarefa; ausência de operação não significa OK.
    SNAPSHOT MakeInitialSnapshot(
    ) {
        SNAPSHOT snapshot = {};
        const uint8_t addresses[channel_count] = {
            EPS_INA219::SOLAR_ADDRESS, EPS_INA219::BATTERY_ADDRESS, EPS_INA219::GENERAL_ADDRESS
        };
        for (size_t index = 0; index < channel_count; ++index) {
            snapshot.channels[index].reading.channel = static_cast<INAChannel>(index);
            snapshot.channels[index].reading.address = addresses[index];
            snapshot.channels[index].last_status = INAStatus::NotInitialized;
            snapshot.channels[index].result = i2c::STATUS::NOT_INITIALIZED;
        }
        return snapshot;
    }

    // I/O fica fora da publicação; o loop nunca espera pelo executor I2C.
    void RunWorker(
        void*
    ) {
        Channel channels[channel_count];
        SNAPSHOT snapshot = MakeInitialSnapshot();
        DIAGNOSTICS progress = {};
        progress.attempted = true;
        progress.active = true;
        for (;;) {
            for (size_t index = 0; index < channel_count; ++index) {
                const auto bus = i2c::GetDiagnostics();
                progress.service = bus.state;
                auto& sample = snapshot.channels[index];
                const ACTION action = channels[index].Next(obc_time::esp32::GetMonotonicMs(),
                    bus.state == i2c::SERVICE_STATE::READY, bus.recovery_cycles);
                if (action != ACTION::WAIT) {
                    REQUEST request = {};
                    request.channel = static_cast<INAChannel>(index);
                    request.action = action;
                    request.recovery_cycles = bus.recovery_cycles;
                    sample.result = i2c::ExecuteChecked(ExecuteRequest, &request,
                        request_priority, queue_wait_ms);
                    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
                    Count(progress.operations);
                    if (sample.result != i2c::STATUS::OK) Count(progress.failures);
                    if (request.epoch_changed) {
                        channels[index].Next(now_ms, false, request.recovery_cycles);
                    } else {
                        channels[index].Complete(action, now_ms, request.recovery_cycles,
                            sample.result == i2c::STATUS::OK && request.valid, request.online);
                    }
                    sample.bus = request.bus;
                    sample.last_status = request.reading.status;
                    if (sample.result == i2c::STATUS::OK && request.valid) sample.reading = request.reading;
                }
                sample.state = channels[index].GetState();
            }
            // Mesmo sem sensores, cada rodada concluída demonstra progresso.
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
    const auto initial_snapshot = MakeInitialSnapshot();
    DIAGNOSTICS initial_diagnostics = {};
    initial_diagnostics.attempted = true;
    initial_diagnostics.service = i2c::GetDiagnostics().state;
    portENTER_CRITICAL(&publication_lock);
    published = initial_snapshot;
    diagnostics = initial_diagnostics;
    portEXIT_CRITICAL(&publication_lock);
    if (initial_diagnostics.service != i2c::SERVICE_STATE::READY) return false;
#if defined(OBC_HOUSEKEEPING_BENCH) && !defined(OBC_HOUSEKEEPING_FIXTURES)
    worker_task = xTaskCreateStaticPinnedToCore(RunWorker, "eps_ina219", worker_stack_bytes,
        nullptr, 1, worker_stack, &worker_control, 1);
    if (worker_task == nullptr) return false;
#else
    if (xTaskCreatePinnedToCore(RunWorker, "eps_ina219", worker_stack_bytes,
            nullptr, 1, &worker_task, 1) != pdPASS) return false;
#endif
    init_active = true;
    return true;
}

SNAPSHOT GetSnapshot(
) {
    portENTER_CRITICAL(&publication_lock);
    SNAPSHOT snapshot = published;
    portEXIT_CRITICAL(&publication_lock);
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    const auto bus = i2c::GetDiagnostics();
    for (auto& sample : snapshot.channels) {
        sample.available = bus.state == i2c::SERVICE_STATE::READY
            && sample.state.recovery_cycles == bus.recovery_cycles && IsAvailable(sample.state, now_ms);
        sample.reading.valid = sample.available;
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
    // active só é publicado após uma rodada real; não inventar amostra/progresso.
    if (!progress.active) {
        Serial.printf("[EPS] state=%s service=%u\n",
            init_active ? "starting" : progress.attempted ? "inactive" : "not_initialized",
            static_cast<unsigned>(progress.service));
        return;
    }
    const auto snapshot = GetSnapshot();
    for (const auto& sample : snapshot.channels) {
        char values[200] = "vin_minus_V=null vin_plus_V=null shunt_mV=null current_A=null power_minus_W=null power_plus_W=null";
        if (sample.state.has_valid) {
            std::snprintf(values, sizeof(values),
                "vin_minus_V=%g vin_plus_V=%g shunt_mV=%g current_A=%g power_minus_W=%g power_plus_W=%g",
                sample.reading.busVoltage_V, sample.reading.sourceVoltage_V, sample.reading.shuntVoltage_mV,
                sample.reading.current_A, sample.reading.power_W, sample.reading.sourcePower_W);
        }
        char line[520];
        const int length = std::snprintf(line, sizeof(line),
            "[EPS] channel=%s address=0x%02x available=%u has_valid=%u age_ms=%llu attempted_ms=%llu last_valid_ms=%llu driver=%s result=%u phase=%u status=%u received=%u %s\n",
            EPS_INA219::channelName(sample.reading.channel), sample.reading.address,
            sample.available ? 1U : 0U, sample.state.has_valid ? 1U : 0U,
            static_cast<unsigned long long>(sample.age_ms),
            static_cast<unsigned long long>(sample.state.attempted_ms),
            static_cast<unsigned long long>(sample.state.last_valid_ms),
            EPS_INA219::statusName(sample.last_status), static_cast<unsigned>(sample.result),
            static_cast<unsigned>(sample.bus.phase), sample.bus.status, sample.bus.received, values);
        if (length > 0 && static_cast<size_t>(length) < sizeof(line)) {
            Serial.write(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(length));
        }
    }
    Serial.printf("[EPS-HEALTH] active=%u service=%u progress_ms=%llu age_ms=%llu operations=%llu failures=%llu stack_free_bytes=%lu\n",
        progress.active ? 1U : 0U, static_cast<unsigned>(progress.service),
        static_cast<unsigned long long>(progress.progress_ms),
        static_cast<unsigned long long>(now_ms >= progress.progress_ms ? now_ms - progress.progress_ms : 0),
        static_cast<unsigned long long>(progress.operations), static_cast<unsigned long long>(progress.failures),
        static_cast<unsigned long>(progress.stack_free_bytes));
}

} // namespace esp32
} // namespace obc_eps
#endif // OBC_EPS_BENCH
