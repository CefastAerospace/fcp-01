#include "ObcMpu9250.h"
#include "I2cBus.h" // Executor e estado da geração I2C.
#include "ObcTimeEsp32.h" // Tempo monotônico em ms.
#include <Arduino.h> // Serial, tarefas e seções críticas ESP32.
#include <cstdio> // std::snprintf para o diagnóstico dos eixos e da identidade.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcMpu9250.h".
 */

namespace obc_mpu9250 {
namespace esp32 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    struct SNAPSHOT {
        STATE state;
        IDENTITY identity;
        i2c::STATUS result;
    };

    struct REQUEST {
        ACTION action;
        uint64_t recovery_cycles;
        uint64_t completed_ms;
        IDENTITY identity;
        RAW_SAMPLE sample;
    };

    constexpr uint32_t task_stack_bytes = 4096;
    constexpr UBaseType_t task_priority = 1;
    TaskHandle_t worker = nullptr;
    SNAPSHOT snapshot = {{}, {}, i2c::STATUS::NOT_INITIALIZED};
    portMUX_TYPE snapshot_lock = portMUX_INITIALIZER_UNLOCKED;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Somente a identificação pode iniciar uma sequência em outra geração.
    i2c::OPERATION_RESULT ExecuteRequest(
        TwoWire& wire,
        void* context
    ) {
        auto& request = *static_cast<REQUEST*>(context);
        const auto bus = i2c::GetDiagnostics();
        const bool same_generation = request.recovery_cycles == bus.recovery_cycles;
        request.recovery_cycles = bus.recovery_cycles;
        if (bus.state != i2c::SERVICE_STATE::READY
                || (request.action != ACTION::IDENTIFY_RESET && !same_generation)) {
            return i2c::OPERATION_RESULT::OPERATION_FAILED;
        }

        auto result = i2c::OPERATION_RESULT::OPERATION_FAILED;
        switch (request.action) {
            case ACTION::IDENTIFY_RESET: result = IdentifyAndReset(wire, request.identity); break;
            case ACTION::WAKE: result = Wake(wire); break;
            case ACTION::CONFIGURE: result = Configure(wire); break;
            case ACTION::READ: result = ReadRaw(wire, request.sample); break;
            case ACTION::WAIT: break;
        }
        if (result == i2c::OPERATION_RESULT::OK) request.completed_ms = obc_time::esp32::GetMonotonicMs();
        return result;
    }

    // Executa no máximo uma etapa por rodada e publica apenas após o executor retornar.
    void RunWorker(
        void*
    ) {
        SNAPSHOT current = {{}, {}, i2c::STATUS::NOT_INITIALIZED};
        TickType_t last_release = xTaskGetTickCount();
        for (;;) {
            const auto before = i2c::GetDiagnostics();
            const auto action = Next(current.state, obc_time::esp32::GetMonotonicMs(),
                before.state, before.recovery_cycles);
            if (action != ACTION::WAIT) {
                REQUEST request = {action, before.recovery_cycles, 0, {}, {}};
                current.result = i2c::ExecuteChecked(ExecuteRequest, &request, request_priority, queue_wait_ms);
                const auto after = i2c::GetDiagnostics();
                const bool accepted = current.result == i2c::STATUS::OK && after.state == i2c::SERVICE_STATE::READY
                    && request.recovery_cycles == after.recovery_cycles;
                if (action == ACTION::IDENTIFY_RESET || current.state.recovery_cycles != request.recovery_cycles) {
                    current.identity = request.identity;
                }
                Complete(current.state, action, request.completed_ms, request.recovery_cycles, accepted, request.sample);

                portENTER_CRITICAL(&snapshot_lock);
                snapshot = current;
                portEXIT_CRITICAL(&snapshot_lock);
            }
            vTaskDelayUntil(&last_release, pdMS_TO_TICKS(acquisition_period_ms));
        }
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

bool Init(
) {
    if (worker != nullptr) return true;
    return xTaskCreate(RunWorker, "obc-mpu9250", task_stack_bytes, nullptr, task_priority, &worker) == pdPASS;
}

void ReportDiagnostics(
) {
    static uint64_t last_ms = 0;
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    if (now_ms - last_ms < acquisition_period_ms) return;
    last_ms = now_ms;

    portENTER_CRITICAL(&snapshot_lock);
    const SNAPSHOT current = snapshot;
    portEXIT_CRITICAL(&snapshot_lock);
    const auto bus = i2c::GetDiagnostics();
    const auto& state = current.state;
    const bool available = IsAvailable(state, obc_time::esp32::GetMonotonicMs(), bus.state, bus.recovery_cycles);
    char identity[5] = "null";
    if (current.identity.available && state.recovery_cycles == bus.recovery_cycles) {
        std::snprintf(identity, sizeof(identity), "0x%02X", static_cast<unsigned>(current.identity.value));
    }

    char acceleration[40] = "null";
    char angular_rate[40] = "null";
    if (available) {
        const auto& sample = state.sample;
        std::snprintf(acceleration, sizeof(acceleration), "[%.4f,%.4f,%.4f]",
            ToAccelerationG(sample.acceleration[0]), ToAccelerationG(sample.acceleration[1]),
            ToAccelerationG(sample.acceleration[2]));
        std::snprintf(angular_rate, sizeof(angular_rate), "[%.4f,%.4f,%.4f]",
            ToAngularRateDps(sample.angular_rate[0]), ToAngularRateDps(sample.angular_rate[1]),
            ToAngularRateDps(sample.angular_rate[2]));
    }

    Serial.printf("[MPU9250] address=0x%02X available=%u accel_g=%s gyro_dps=%s who_am_i=%s result=%u service=%u sampled_ms=%llu sample_generation=%llu bus_generation=%llu\n",
        static_cast<unsigned>(address), available ? 1U : 0U, acceleration, angular_rate, identity,
        static_cast<unsigned>(current.result), static_cast<unsigned>(bus.state),
        static_cast<unsigned long long>(state.valid ? state.completed_ms : 0),
        static_cast<unsigned long long>(state.recovery_cycles), static_cast<unsigned long long>(bus.recovery_cycles));
}

} // namespace esp32
} // namespace obc_mpu9250
