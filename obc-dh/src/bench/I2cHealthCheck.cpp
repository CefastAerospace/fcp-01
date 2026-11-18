#include "ObcDataFlowEsp32.h" // Executor real, diagnóstico e tempo monotônico.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDataFlowEsp32.h".
 */

#ifndef OBC_I2C_HEALTH_BENCH
#error "Usar somente int008-i2c-health-check"
#endif

namespace {

    // ─── STRUCTS ──────────────────────────────────────────────────────

    struct OPERATION {
        i2c::OPERATION_RESULT result;
        uint32_t delay_ms;
        unsigned calls;
    };

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    std::atomic<bool> started(false);
    std::atomic<uint32_t> stop_delay_ms(0);
    std::atomic<bool> fail_start(false);

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Interrompe somente o roteiro; preserva diagnósticos e o loop USB para coleta.
    void Check(
        bool condition,
        const char* expression,
        int line
    ) {
        if (condition) return;
        Serial.printf("[I2C-HEALTH-CHECK] FAIL line=%d expression=%s\n", line, expression);
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }

#define CHECK(condition) Check((condition), #condition, __LINE__)

    // Garante a duração mínima em micros, cedendo CPU até o prazo real.
    // Os cenários duram menos de um wrap u32; nenhum lock fica retido.
    void Wait(
        uint32_t duration_ms
    ) {
        const uint32_t began_us = micros();
        const uint32_t duration_us = duration_ms * 1000;
        uint32_t elapsed_us = 0;
        while (elapsed_us < duration_us) {
            const uint64_t remaining_us = duration_us - elapsed_us;
            const TickType_t ticks = static_cast<TickType_t>(
                (remaining_us * configTICK_RATE_HZ + 999999) / 1000000);
            vTaskDelay(ticks);
            elapsed_us = static_cast<uint32_t>(micros()) - began_us;
        }
    }

    // Marca fronteiras usando o mesmo relógio u64 dos diagnósticos do loop.
    void Marker(
        const char* name
    ) {
        Serial.printf("[I2C-HEALTH-CHECK] case=%s monotonic_ms=%llu\n", name,
            static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
    }

    // Contexto permanece válido na pilha do cliente enquanto ExecuteChecked aguarda.
    i2c::OPERATION_RESULT Callback(
        TwoWire&,
        void* context
    ) {
        auto& operation = *static_cast<OPERATION*>(context);
        ++operation.calls;
        if (operation.delay_ms) Wait(operation.delay_ms);
        return operation.result;
    }

    // Controller/GPIO simulados: somente o executor consome os controles atômicos.
    bool Stop(
        void*
    ) {
        const uint32_t delay_ms = stop_delay_ms.exchange(0);
        if (delay_ms) Wait(delay_ms);
        return true;
    }

    bool Start(
        void*
    ) { return !fail_start.load(); }

    void Prepare(
        void*
    ) {}

    void SetLine(
        void*,
        bool
    ) {}

    bool ReadLine(
        void*
    ) { return true; }

    uint32_t Now(
        void*
    ) { return micros(); }

    // Conserva o tempo real da recuperação sem ocupar CPU nos intervalos longos.
    void Delay(
        void*,
        uint32_t duration_us
    ) {
        if (duration_us >= 1000) vTaskDelay(pdMS_TO_TICKS(duration_us / 1000));
        if (duration_us % 1000) delayMicroseconds(duration_us % 1000);
    }

    // O retorno do callback precede a recuperação; aguarda a conclusão publicada.
    i2c::DIAGNOSTICS WaitCycle(
        uint64_t cycle
    ) {
        const uint32_t began_ms = millis();
        for (;;) {
            const auto diagnostics = i2c::GetDiagnostics();
            if (diagnostics.recovery_cycles >= cycle && diagnostics.state != i2c::SERVICE_STATE::RECOVERING) {
                return diagnostics;
            }
            CHECK(static_cast<uint32_t>(millis() - began_ms) < 2000);
            Wait(1);
        }
    }

    // Cenários adicionais; nenhuma entrada ou expectativa das bancadas antigas muda.
    void Run(
        void*
    ) {
        using namespace i2c;
        CHECK(Init());
        Marker("idle_begin");
        Wait(1200);
        CHECK(GetDiagnostics().state == SERVICE_STATE::READY);
        Marker("idle_end");

        OPERATION operation = {OPERATION_RESULT::OK, 0, 0};
        CHECK(ExecuteChecked(Callback, &operation) == i2c::STATUS::OK);
        operation.result = OPERATION_RESULT::DEVICE_NACK;
        CHECK(ExecuteChecked(Callback, &operation) == i2c::STATUS::DEVICE_NACK);
        operation.result = OPERATION_RESULT::READ_INCOMPLETE;
        CHECK(ExecuteChecked(Callback, &operation) == i2c::STATUS::READ_INCOMPLETE);
        CHECK(GetDiagnostics().recovery_cycles == 0 && operation.calls == 3);

        Marker("callback_begin");
        operation = {OPERATION_RESULT::OK, 350, 0};
        const uint32_t before = millis();
        CHECK(ExecuteChecked(Callback, &operation, 0, 100) == i2c::STATUS::OK);
        const uint32_t elapsed_ms = millis() - before;
        CHECK(elapsed_ms >= 350 && operation.calls == 1);
        CHECK(GetDiagnostics().maximum_callback_us >= 350000);
        Serial.printf("[I2C-HEALTH-CHECK] context_hold_ms=%lu queue_wait_ms=100 calls=%u\n",
            static_cast<unsigned long>(elapsed_ms), operation.calls);
        Marker("callback_end");
        Wait(500);

        Marker("normal_recovery_begin");
        operation = {OPERATION_RESULT::TRANSFER_TIMEOUT, 0, 0};
        CHECK(ExecuteChecked(Callback, &operation) == i2c::STATUS::TRANSFER_TIMEOUT);
        CHECK(WaitCycle(1).state == SERVICE_STATE::READY);
        Marker("normal_recovery_end");
        Wait(500);

        Marker("slow_recovery_begin");
        stop_delay_ms.store(450);
        operation = {OPERATION_RESULT::BUS_ERROR, 0, 0};
        CHECK(ExecuteChecked(Callback, &operation) == i2c::STATUS::BUS_ERROR);
        auto diagnostics = WaitCycle(2);
        CHECK(diagnostics.state == SERVICE_STATE::UNAVAILABLE);
        CHECK(diagnostics.last_failure == RECOVERY_CAUSE::BUDGET_EXHAUSTED);
        CHECK(diagnostics.last_recovery_us >= 450000);
        Marker("slow_recovery_end");
        Wait(500);
        CHECK(GetDiagnostics().recovery_cycles == 2);
        CHECK(ExecuteChecked(Callback, &operation) == i2c::STATUS::SERVICE_UNAVAILABLE);

        Marker("failed_request_begin");
        fail_start.store(true);
        CHECK(RequestRecovery());
        CHECK(WaitCycle(3).state == SERVICE_STATE::UNAVAILABLE);
        Marker("failed_request_end");
        Wait(500);
        CHECK(GetDiagnostics().recovery_cycles == 3);

        Marker("successful_request_begin");
        fail_start.store(false);
        CHECK(RequestRecovery());
        CHECK(WaitCycle(4).state == SERVICE_STATE::READY);
        operation = {OPERATION_RESULT::OK, 0, 0};
        CHECK(ExecuteChecked(Callback, &operation) == i2c::STATUS::OK && operation.calls == 1);
        Marker("successful_request_end");
        Wait(500);
        diagnostics = GetDiagnostics();
        CHECK(diagnostics.recovered == 2 && diagnostics.exhausted == 2);
        Serial.printf("[I2C-HEALTH-CHECK] done=1 cycles=%llu recovered=%llu exhausted=%llu stack=%u\n",
            static_cast<unsigned long long>(diagnostics.recovery_cycles),
            static_cast<unsigned long long>(diagnostics.recovered),
            static_cast<unsigned long long>(diagnostics.exhausted), static_cast<unsigned>(diagnostics.worker_stack_free_bytes));
        vTaskDelete(nullptr);
    }

} // namespace

namespace i2c {

RECOVERY_IO GetRecoveryCheckIO(
) { return {Stop, Start, Prepare, SetLine, SetLine, ReadLine, ReadLine, Now, Delay, nullptr}; }

} // namespace i2c

namespace obc_flow {
namespace esp32 {

void StartI2cHealthBench(
) {
    if (started.exchange(true)) return;
    // Cliente de ensaio separado: nenhuma tarefa nova é criada no firmware obc.
    CHECK(xTaskCreate(Run, "i2c_health_check", 4096, nullptr, 1, nullptr) == pdPASS);
}

} // namespace esp32
} // namespace obc_flow
