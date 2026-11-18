#include "ObcAs5600.h"
#include "I2cBus.h" // Executor, estado e geração compartilhados.
#include "ObcTimeEsp32.h" // Relógio monotônico em ms.
#include <Arduino.h> // Serial, tarefas e seções críticas ESP32.
#include <cstdio> // std::snprintf para compor uma linha de diagnóstico.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcAs5600.h".
 */

namespace obc_as5600 {
namespace esp32 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    struct SNAPSHOT {
        SAMPLE sample;
        i2c::STATUS result;
    };

    constexpr uint32_t task_stack_bytes = 4096;
    constexpr UBaseType_t task_priority = 1;
    TaskHandle_t worker = nullptr;
    SNAPSHOT snapshot = {{}, i2c::STATUS::NOT_INITIALIZED};
    portMUX_TYPE snapshot_lock = portMUX_INITIALIZER_UNLOCKED;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // A geração pertence ao instante de execução, não ao de entrada na fila.
    i2c::OPERATION_RESULT ReadEncoder(
        TwoWire& wire,
        void* context
    ) {
        auto& sample = *static_cast<SAMPLE*>(context);
        sample.recovery_cycles = i2c::GetDiagnostics().recovery_cycles;
        const auto result = ReadRaw(wire, sample.raw_count);
        if (result == i2c::OPERATION_RESULT::OK) sample.sampled_ms = obc_time::esp32::GetMonotonicMs();
        return result;
    }

    // A espera cooperativa fica no produtor, separada do loop de TC/TM.
    void RunWorker(
        void*
    ) {
        TickType_t last_release = xTaskGetTickCount();
        for (;;) {
            SNAPSHOT current = {};
            current.result = i2c::ExecuteChecked(ReadEncoder, &current.sample, request_priority, queue_wait_ms);
            current.sample.valid = current.result == i2c::STATUS::OK;
            portENTER_CRITICAL(&snapshot_lock);
            snapshot = current;
            portEXIT_CRITICAL(&snapshot_lock);
            vTaskDelayUntil(&last_release, pdMS_TO_TICKS(acquisition_period_ms));
        }
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

bool Init(
) {
    if (worker != nullptr) return true;
    return xTaskCreate(RunWorker, "obc-as5600", task_stack_bytes, nullptr, task_priority, &worker) == pdPASS;
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
    const uint64_t observed_ms = obc_time::esp32::GetMonotonicMs();
    const bool available = IsAvailable(current.sample, observed_ms, bus.state, bus.recovery_cycles);
    char count[5] = "null";
    if (available) std::snprintf(count, sizeof(count), "%u", static_cast<unsigned>(current.sample.raw_count));
    Serial.printf("[AS5600] address=0x%02X available=%u raw_count=%s result=%u service=%u sampled_ms=%llu sample_generation=%llu bus_generation=%llu\n",
        static_cast<unsigned>(address), available ? 1U : 0U, count,
        static_cast<unsigned>(current.result), static_cast<unsigned>(bus.state),
        static_cast<unsigned long long>(current.sample.sampled_ms),
        static_cast<unsigned long long>(current.sample.recovery_cycles),
        static_cast<unsigned long long>(bus.recovery_cycles));
}

} // namespace esp32
} // namespace obc_as5600
