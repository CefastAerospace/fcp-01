#include "I2cRecoveryCheck.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "I2cRecoveryCheck.h".
 */

namespace {

    // ─── STRUCTS ──────────────────────────────────────────────────────

    struct SIMULATION {
        uint32_t now_us = 0;
        uint32_t now_step_us = 0;
        uint32_t stop_cost_us = 0;
        unsigned stop_failures = 0;
        unsigned start_failures = 0;
        unsigned starts = 0;
        unsigned clocks = 0;
        unsigned release_after = 0;
        bool sda_stuck = false;
        bool scl_stuck = false;
        bool sda_released = true;
        bool scl_released = true;
        SemaphoreHandle_t stop_gate = nullptr;
        SemaphoreHandle_t stop_entered = nullptr;
    };

    struct CLIENT {
        i2c::OPERATION_RESULT outcome = i2c::OPERATION_RESULT::OK;
        i2c::STATUS result = i2c::STATUS::OK;
        uint32_t timeout_ms = 1000;
        uint8_t priority = 0;
        std::atomic<unsigned> calls{0};
        StaticSemaphore_t done_storage;
        StaticSemaphore_t entered_storage;
        StaticSemaphore_t gate_storage;
        SemaphoreHandle_t done = nullptr;
        SemaphoreHandle_t entered = nullptr;
        SemaphoreHandle_t gate = nullptr;
        bool hold = false;
        bool check_recursion = false;
    };

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    // Tempos exclusivos do cenário que mantém um callback iniciado além do prazo da fila.
    constexpr uint32_t context_queue_wait_ms = 1000;
    constexpr uint32_t context_hold_ms = context_queue_wait_ms + 5;
    constexpr uint32_t scenario_wait_ms = context_hold_ms + 1000;
    SIMULATION simulation;
    CLIENT clients[3];
    StaticSemaphore_t stop_gate_storage;
    StaticSemaphore_t stop_entered_storage;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Uma falha interrompe o cenário e conserva uma linha identificável no relatório.
    void Check(
        bool condition,
        const char* expression,
        int line
    ) {
        if (condition) return;
#ifdef ARDUINO
        Serial.printf("[I2C-CHECK] FAIL line=%d expression=%s\n", line, expression);
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
#else
        std::fprintf(stderr, "[I2C-CHECK] FAIL line=%d expression=%s\n", line, expression);
        std::abort();
#endif
    }

#define CHECK(condition) Check((condition), #condition, __LINE__)

    // Custo interno do driver e barreira permitem verificar orçamento e concorrência.
    bool Stop(
        void* context
    ) {
        auto& state = *static_cast<SIMULATION*>(context);
        state.now_us += state.stop_cost_us;
        if (state.stop_gate) {
            const auto gate = state.stop_gate;
            state.stop_gate = nullptr;
            xSemaphoreGive(state.stop_entered);
            xSemaphoreTake(gate, portMAX_DELAY);
        }
        if (state.stop_failures) { --state.stop_failures; return false; }
        return true;
    }

    // Conta tentativas de inicialização sem alterar o controlador físico na bancada.
    bool Start(
        void* context
    ) {
        auto& state = *static_cast<SIMULATION*>(context);
        ++state.starts;
        if (state.start_failures) { --state.start_failures; return false; }
        return true;
    }

    // Simula dreno aberto com estado de retenção externo independente.
    void Prepare(
        void* context
    ) {
        auto& state = *static_cast<SIMULATION*>(context);
        state.sda_released = true;
        state.scl_released = true;
    }

    void SetSda(
        void* context,
        bool released
    ) { static_cast<SIMULATION*>(context)->sda_released = released; }

    // Um periférico pode liberar SDA após determinado número de clocks.
    void SetScl(
        void* context,
        bool released
    ) {
        auto& state = *static_cast<SIMULATION*>(context);
        if (released && !state.scl_released && state.sda_stuck) {
            ++state.clocks;
            if (state.release_after && state.clocks >= state.release_after) state.sda_stuck = false;
        }
        state.scl_released = released;
    }

    // Ler linha liberada ainda reflete a retenção simulada pelo periférico.
    bool ReadSda(
        void* context
    ) {
        const auto& state = *static_cast<SIMULATION*>(context);
        return state.sda_released && !state.sda_stuck;
    }

    bool ReadScl(
        void* context
    ) {
        const auto& state = *static_cast<SIMULATION*>(context);
        return state.scl_released && !state.scl_stuck;
    }

    // Relógio injetado exercita fronteiras de orçamento e wrap sem espera física.
    uint32_t Now(
        void* context
    ) {
        auto& state = *static_cast<SIMULATION*>(context);
        const uint32_t now_us = state.now_us;
        state.now_us += state.now_step_us;
        return now_us;
    }

    void Delay(
        void* context,
        uint32_t duration_us
    ) { static_cast<SIMULATION*>(context)->now_us += duration_us; }

    // Retornos e limites são verificados independentemente do executor FreeRTOS.
    void CheckCore(
    ) {
        using namespace i2c;
        CHECK(GetRecoveryCause(OPERATION_RESULT::DEVICE_NACK, true, true) == RECOVERY_CAUSE::NONE);
        CHECK(GetRecoveryCause(OPERATION_RESULT::READ_INCOMPLETE, true, true) == RECOVERY_CAUSE::NONE);
        CHECK(GetRecoveryCause(OPERATION_RESULT::OPERATION_FAILED, true, true) == RECOVERY_CAUSE::NONE);
        CHECK(GetRecoveryCause(OPERATION_RESULT::TRANSFER_TIMEOUT, true, true) == RECOVERY_CAUSE::TRANSFER_TIMEOUT);
        CHECK(GetRecoveryCause(OPERATION_RESULT::BUS_ERROR, true, true) == RECOVERY_CAUSE::BUS_ERROR);
        CHECK(GetRecoveryCause(OPERATION_RESULT::OK, false, true) == RECOVERY_CAUSE::SDA_LOW);
        CHECK(GetRecoveryCause(OPERATION_RESULT::OK, true, false) == RECOVERY_CAUSE::SCL_LOW);

        simulation = {};
        auto result = RecoverBus(GetRecoveryCheckIO());
        CHECK(result.recovered && result.attempts == 1 && result.pulses == 0);
        simulation = {};
        simulation.sda_stuck = true;
        simulation.release_after = 9;
        result = RecoverBus(GetRecoveryCheckIO());
        CHECK(result.recovered && result.pulses == 9 && result.sda_high && result.scl_high);
        simulation = {};
        simulation.sda_stuck = true;
        result = RecoverBus(GetRecoveryCheckIO());
        CHECK(!result.recovered && result.attempts == 2 && result.pulses == 18);
        CHECK(result.failure == RECOVERY_CAUSE::SDA_LOW && simulation.starts == 0);
        CHECK(simulation.sda_released && simulation.scl_released);
        simulation = {};
        simulation.scl_stuck = true;
        result = RecoverBus(GetRecoveryCheckIO());
        CHECK(!result.recovered && result.attempts == 2 && result.pulses == 0);
        CHECK(result.failure == RECOVERY_CAUSE::SCL_LOW && simulation.starts == 0);
        simulation = {};
        simulation.start_failures = 1;
        result = RecoverBus(GetRecoveryCheckIO());
        CHECK(result.recovered && result.attempts == 2 && result.elapsed_us >= recovery_interval_us);
        simulation = {};
        simulation.start_failures = 2;
        result = RecoverBus(GetRecoveryCheckIO());
        CHECK(!result.recovered && result.failure == RECOVERY_CAUSE::INIT_FAILED);
        simulation = {};
        simulation.stop_failures = 2;
        result = RecoverBus(GetRecoveryCheckIO());
        CHECK(!result.recovered && result.failure == RECOVERY_CAUSE::DEINIT_FAILED && simulation.starts == 0);
        simulation = {};
        simulation.stop_cost_us = recovery_budget_us;
        result = RecoverBus(GetRecoveryCheckIO());
        CHECK(!result.recovered && result.failure == RECOVERY_CAUSE::BUDGET_EXHAUSTED);
        CHECK(result.attempts == 1 && result.elapsed_us == recovery_budget_us);
        simulation = {};
        simulation.stop_cost_us = recovery_budget_us - recovery_phase_us;
        result = RecoverBus(GetRecoveryCheckIO());
        CHECK(!result.recovered && result.failure == RECOVERY_CAUSE::BUDGET_EXHAUSTED && simulation.starts == 0);
        simulation = {};
        simulation.now_us = UINT32_MAX - 4;
        result = RecoverBus(GetRecoveryCheckIO());
        CHECK(result.recovered && result.elapsed_us == 10);
        // O relógio continua avançando entre a última etapa e a publicação final.
        for (uint32_t cost = recovery_budget_us - 50; cost < recovery_budget_us; ++cost) {
            simulation = {};
            simulation.now_step_us = 1;
            simulation.stop_cost_us = cost;
            result = RecoverBus(GetRecoveryCheckIO());
            if (result.elapsed_us >= recovery_budget_us) {
                CHECK(!result.recovered && result.failure == RECOVERY_CAUSE::BUDGET_EXHAUSTED);
            }
        }
        simulation = {};
    }

    // Uma espera de teste expira com diagnóstico em vez de bloquear indefinidamente.
    void Take(
        SemaphoreHandle_t semaphore
    ) { CHECK(xSemaphoreTake(semaphore, pdMS_TO_TICKS(scenario_wait_ms)) == pdTRUE); }

    // Espera estado publicado, cedendo CPU ao worker real.
    void WaitState(
        i2c::SERVICE_STATE expected
    ) {
        const uint32_t started_ms = millis();
        while (i2c::GetDiagnostics().state != expected) {
            CHECK(static_cast<uint32_t>(millis()) - started_ms < scenario_wait_ms);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // Callback usa contexto do chamador até completar e pode testar recursão rejeitada.
    i2c::OPERATION_RESULT Operation(
        TwoWire&,
        void* context
    ) {
        auto& client = *static_cast<CLIENT*>(context);
        ++client.calls;
        xSemaphoreGive(client.entered);
        if (client.check_recursion) {
            CHECK(i2c::ExecuteChecked(Operation, context) == i2c::STATUS::INVALID_CONTEXT);
            CHECK(!i2c::RequestRecovery());
        }
        if (client.hold) Take(client.gate);
        return client.outcome;
    }

    // Cada tarefa solicitante consome sua conclusão antes de encerrar.
    void RunClient(
        void* context
    ) {
        auto& client = *static_cast<CLIENT*>(context);
        client.result = i2c::ExecuteChecked(Operation, &client, client.priority, client.timeout_ms);
        xSemaphoreGive(client.done);
        vTaskDelete(nullptr);
    }

    // Inicialização ocorre somente depois da conclusão do uso anterior do contexto.
    void PrepareClient(
        size_t index,
        i2c::OPERATION_RESULT outcome,
        bool hold = false,
        uint32_t timeout_ms = 1000
    ) {
        auto& client = clients[index];
        client.outcome = outcome;
        client.result = i2c::STATUS::OK;
        client.calls = 0;
        client.timeout_ms = timeout_ms;
        client.priority = 0;
        client.hold = hold;
        client.check_recursion = false;
        client.done = xSemaphoreCreateBinaryStatic(&client.done_storage);
        client.entered = xSemaphoreCreateBinaryStatic(&client.entered_storage);
        client.gate = xSemaphoreCreateBinaryStatic(&client.gate_storage);
    }

    void StartClient(
        size_t index
    ) {
        CHECK(xTaskCreate(RunClient, "i2c_client", 4096, &clients[index], 2, nullptr) == pdPASS);
    }

    // Barreira por metadados confirma que os pedidos já entraram na fila.
    void WaitQueued(
        size_t count
    ) {
        const uint32_t started_ms = millis();
        while (i2c::GetDiagnostics().queued_requests != count) {
            CHECK(static_cast<uint32_t>(millis()) - started_ms < scenario_wait_ms);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // Permite controlar a recuperação depois de publicar a conclusão do callback.
    void HoldRecovery(
    ) {
        simulation.stop_gate = xSemaphoreCreateBinaryStatic(&stop_gate_storage);
        simulation.stop_entered = xSemaphoreCreateBinaryStatic(&stop_entered_storage);
    }

    // Exercita o contrato booleano com o próprio contexto, inclusive false legítimo.
    bool LegacyOperation(
        TwoWire&,
        void* context
    ) { return *static_cast<bool*>(context); }

    // Mesma fila, callbacks, cancelamento e semáforos usados pelo firmware normal.
    void CheckExecutor(
    ) {
        using namespace i2c;
        CHECK(!RequestRecovery() && GetDiagnostics().state == SERVICE_STATE::UNINITIALIZED);
        CHECK(Init() && Init());
        CHECK(Execute(nullptr, nullptr) == i2c::STATUS::INVALID_ARGUMENT);
        CHECK(ExecuteChecked(nullptr, nullptr) == i2c::STATUS::INVALID_ARGUMENT);
        bool success = true;
        CHECK(Execute(LegacyOperation, &success) == i2c::STATUS::OK);
        success = false;
        CHECK(Execute(LegacyOperation, &success) == i2c::STATUS::OPERATION_FAILED);
        for (auto outcome : {OPERATION_RESULT::DEVICE_NACK, OPERATION_RESULT::READ_INCOMPLETE}) {
            PrepareClient(0, outcome);
            clients[0].check_recursion = true;
            StartClient(0);
            Take(clients[0].done);
            CHECK(clients[0].calls == 1);
            CHECK(clients[0].result == (outcome == OPERATION_RESULT::DEVICE_NACK ? i2c::STATUS::DEVICE_NACK : i2c::STATUS::READ_INCOMPLETE));
        }
        CHECK(GetDiagnostics().recovery_cycles == 0);

        // Timeout da fila alcançado durante callback iniciado conserva o contexto.
        PrepareClient(0, OPERATION_RESULT::OK, true, context_queue_wait_ms);
        StartClient(0);
        Take(clients[0].entered);
        vTaskDelay(pdMS_TO_TICKS(context_hold_ms));
        CHECK(xSemaphoreTake(clients[0].done, 0) == pdFALSE);
        CHECK(GetDiagnostics().callback_running);
        xSemaphoreGive(clients[0].gate);
        Take(clients[0].done);
        CHECK(clients[0].result == i2c::STATUS::OK && clients[0].calls == 1);
        CHECK(GetDiagnostics().maximum_callback_us >= context_queue_wait_ms * 1000);

        // Recuperação: pedido ativo retorna, fila expira e sobrevivente retoma.
        simulation = {};
        simulation.sda_stuck = true;
        simulation.release_after = 3;
        simulation.start_failures = 1;
        HoldRecovery();
        PrepareClient(0, OPERATION_RESULT::TRANSFER_TIMEOUT, true);
        PrepareClient(1, OPERATION_RESULT::OK, false, 100);
        PrepareClient(2, OPERATION_RESULT::OK);
        clients[2].priority = 4;
        StartClient(0);
        Take(clients[0].entered);
        StartClient(1);
        StartClient(2);
        WaitQueued(2);
        xSemaphoreGive(clients[0].gate);
        Take(simulation.stop_entered);
        Take(clients[0].done);
        CHECK(clients[0].result == i2c::STATUS::TRANSFER_TIMEOUT && clients[0].calls == 1);
        CHECK(Execute(LegacyOperation, &success) == i2c::STATUS::SERVICE_RECOVERING);
        Take(clients[1].done);
        CHECK(clients[1].result == i2c::STATUS::QUEUE_TIMEOUT && clients[1].calls == 0);
        xSemaphoreGive(&stop_gate_storage);
        Take(clients[2].done);
        CHECK(clients[2].result == i2c::STATUS::OK && clients[2].calls == 1);
        WaitState(SERVICE_STATE::READY);
        auto diagnostic = GetDiagnostics();
        CHECK(diagnostic.recovered == 1 && diagnostic.recovery_attempt_count == 2);
        CHECK(diagnostic.recovery_pulses == 3 && diagnostic.transfer_timeouts == 1);

        // Esgotamento conclui a fila e exige nova solicitação explícita.
        simulation = {};
        simulation.scl_stuck = true;
        PrepareClient(0, OPERATION_RESULT::BUS_ERROR, true);
        PrepareClient(1, OPERATION_RESULT::OK);
        StartClient(0);
        Take(clients[0].entered);
        StartClient(1);
        WaitQueued(1);
        xSemaphoreGive(clients[0].gate);
        Take(clients[0].done);
        Take(clients[1].done);
        CHECK(clients[0].result == i2c::STATUS::BUS_ERROR && clients[0].calls == 1);
        CHECK(clients[1].result == i2c::STATUS::SERVICE_UNAVAILABLE && clients[1].calls == 0);
        CHECK(ExecuteChecked(Operation, &clients[0]) == i2c::STATUS::SERVICE_UNAVAILABLE);
        diagnostic = GetDiagnostics();
        CHECK(diagnostic.state == SERVICE_STATE::UNAVAILABLE && diagnostic.exhausted == 1);
        CHECK(diagnostic.last_failure == RECOVERY_CAUSE::SCL_LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        CHECK(GetDiagnostics().recovery_cycles == diagnostic.recovery_cycles);
        simulation = {};
        CHECK(RequestRecovery());
        CHECK(!RequestRecovery());
        WaitState(SERVICE_STATE::READY);
        diagnostic = GetDiagnostics();
        CHECK(diagnostic.trigger == RECOVERY_CAUSE::REQUESTED && diagnostic.recovered == 2);
        CHECK(Execute(LegacyOperation, &success) == i2c::STATUS::OPERATION_FAILED);
    }

    // Relatório compacto permite conferir término e pilha no ESP32.
    void RunChecks(
    ) {
        CheckCore();
        CheckExecutor();
        const auto diagnostic = i2c::GetDiagnostics();
#ifdef ARDUINO
        Serial.printf("[I2C-CHECK] PASS cycles=%llu attempts=%llu recovered=%llu exhausted=%llu stack=%u callback_max_us=%lu context_queue_ms=%lu context_hold_ms=%lu\n",
#else
        std::printf("[I2C-CHECK] PASS cycles=%llu attempts=%llu recovered=%llu exhausted=%llu stack=%u callback_max_us=%lu context_queue_ms=%lu context_hold_ms=%lu\n",
#endif
            static_cast<unsigned long long>(diagnostic.recovery_cycles),
            static_cast<unsigned long long>(diagnostic.recovery_attempt_count),
            static_cast<unsigned long long>(diagnostic.recovered), static_cast<unsigned long long>(diagnostic.exhausted),
            static_cast<unsigned>(diagnostic.worker_stack_free_bytes), static_cast<unsigned long>(diagnostic.maximum_callback_us),
            static_cast<unsigned long>(context_queue_wait_ms), static_cast<unsigned long>(context_hold_ms));
    }

} // namespace

namespace i2c {

RECOVERY_IO GetRecoveryCheckIO(
) {
    return {Stop, Start, Prepare, SetSda, SetScl, ReadSda, ReadScl, Now, Delay, &simulation};
}

} // namespace i2c

#ifdef ARDUINO
/** @brief Aguarda início explícito para o computador conseguir capturar o relatório. */
void setup(
) {
    Serial.begin(115200);
    Serial.println("[I2C-CHECK] READY send g");
}

/** @brief Executa uma vez com o executor FreeRTOS real e GPIOs simulados. */
void loop(
) {
    static bool started = false;
    if (!started && Serial.available() && Serial.read() == 'g') {
        started = true;
        RunChecks();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
#else
/** @brief Executa cenários concorrentes e reúne threads antes do encerramento. */
int main(
) {
    RunChecks();
    StopNativeI2cTasks();
}
#endif
