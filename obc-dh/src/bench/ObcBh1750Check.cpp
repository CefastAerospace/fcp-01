#include "ObcBh1750.h"
#include "I2cBus.h" // Executor, estado e geração do barramento.
#include "ObcTimeEsp32.h" // Relógio monotônico em ms.
#include <Arduino.h> // Serial, tarefas e seções críticas ESP32.
#include <cstdio> // std::snprintf para formatar a medição em lux.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcBh1750.h".
 */

namespace obc_bh1750 {
namespace esp32 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    struct CHANNEL_REPORT {
        CHANNEL channel = {};
        i2c::STATUS result = i2c::STATUS::NOT_INITIALIZED;
    };

    struct SNAPSHOT {
        CHANNEL_REPORT channels[channel_count];
    };

    struct REQUEST {
        uint8_t address;
        ACTION action;
        uint64_t recovery_cycles;
        uint64_t completed_ms;
        uint16_t raw;
    };

    constexpr uint32_t task_stack_bytes = 4096;
    constexpr UBaseType_t task_priority = 1;
    TaskHandle_t worker = nullptr;
    SNAPSHOT snapshot = {};
    portMUX_TYPE snapshot_lock = portMUX_INITIALIZER_UNLOCKED;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Uma leitura enfileirada perde sua configuração se o I2C se recuperou.
    i2c::OPERATION_RESULT ExecuteRequest(
        TwoWire& wire,
        void* context
    ) {
        auto& request = *static_cast<REQUEST*>(context);
        const auto bus = i2c::GetDiagnostics();
        const bool same_generation = request.recovery_cycles == bus.recovery_cycles;
        request.recovery_cycles = bus.recovery_cycles;
        if (bus.state != i2c::SERVICE_STATE::READY || (request.action == ACTION::READ && !same_generation)) {
            return i2c::OPERATION_RESULT::OPERATION_FAILED;
        }

        const auto result = request.action == ACTION::CONFIGURE
            ? Configure(wire, request.address) : ReadRaw(wire, request.address, request.raw);
        if (result == i2c::OPERATION_RESULT::OK) request.completed_ms = obc_time::esp32::GetMonotonicMs();
        return result;
    }

    // Resolve um pedido por canal e deixa a espera de conversão entre rodadas.
    void RunWorker(
        void*
    ) {
        SNAPSHOT current = {};
        TickType_t last_release = xTaskGetTickCount();
        for (;;) {
            for (size_t index = 0; index < channel_count; ++index) {
                auto& report = current.channels[index];
                const auto before = i2c::GetDiagnostics();
                const auto action = Next(report.channel, obc_time::esp32::GetMonotonicMs(),
                    before.state, before.recovery_cycles);
                if (action == ACTION::WAIT) continue;

                REQUEST request = {addresses[index], action, before.recovery_cycles, 0, 0};
                report.result = i2c::ExecuteChecked(ExecuteRequest, &request, request_priority, queue_wait_ms);
                const auto after = i2c::GetDiagnostics();
                const bool accepted = report.result == i2c::STATUS::OK && after.state == i2c::SERVICE_STATE::READY
                    && request.recovery_cycles == after.recovery_cycles;
                Complete(report.channel, action, request.completed_ms, request.recovery_cycles, accepted, request.raw);

                portENTER_CRITICAL(&snapshot_lock);
                snapshot.channels[index] = report;
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
    return xTaskCreate(RunWorker, "obc-bh1750", task_stack_bytes, nullptr, task_priority, &worker) == pdPASS;
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
    for (size_t index = 0; index < channel_count; ++index) {
        const auto& report = current.channels[index];
        const auto& channel = report.channel;
        const bool available = IsAvailable(channel, observed_ms, bus.state, bus.recovery_cycles);
        char lux[16] = "null";
        if (available) std::snprintf(lux, sizeof(lux), "%.3f", static_cast<double>(ToLux(channel.raw)));
        Serial.printf("[BH1750] address=0x%02X available=%u lux=%s result=%u service=%u sampled_ms=%llu sample_generation=%llu bus_generation=%llu\n",
            static_cast<unsigned>(addresses[index]), available ? 1U : 0U, lux,
            static_cast<unsigned>(report.result), static_cast<unsigned>(bus.state),
            static_cast<unsigned long long>(channel.sampled_ms),
            static_cast<unsigned long long>(channel.recovery_cycles),
            static_cast<unsigned long long>(bus.recovery_cycles));
    }
}

} // namespace esp32
} // namespace obc_bh1750
