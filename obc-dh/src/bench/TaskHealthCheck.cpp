#include "ObcDataFlowEsp32.h" // Fluxo real, IO simulado e primitivas FreeRTOS.
#include <atomic> // Coordenação das injeções entre loop e proprietários das tarefas.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDataFlowEsp32.h".
 */

#ifndef OBC_TASK_HEALTH_BENCH
#error "Usar somente int008-health-check"
#endif

namespace {

    enum class CASE {
        OFF, ARMING, UART_PENDING, UART_RUNNING, PROCESS_PENDING, PROCESS_RUNNING,
        WRITE_PENDING, WRITE_RUNNING, SCAN_PENDING, SCAN_RUNNING,
        RECOVERY_PENDING, RECOVERY_RUNNING, DONE
    };
    std::atomic<CASE> current_case(CASE::OFF);
    std::atomic<uint32_t> ready_at_ms(0);
    // Somente a tarefa escritora altera a contagem da varredura.
    unsigned slow_operations = 0;

    // Publica prazo antes de liberar o próximo caso para outro proprietário.
    void Arm(
        CASE next,
        uint32_t delay_ms
    ) {
        ready_at_ms.store(static_cast<uint32_t>(obc_time::esp32::GetMonotonicMs()) + delay_ms);
        current_case.store(next);
    }

    // Prazos da bancada são menores que 2^31 ms, inclusive ao cruzar wrap u32.
    bool Ready(
        CASE expected
    ) {
        return current_case.load() == expected
            && static_cast<int32_t>(static_cast<uint32_t>(obc_time::esp32::GetMonotonicMs()) - ready_at_ms.load()) >= 0;
    }

    // Um caso só é consumido pelo proprietário da atividade selecionada.
    bool Claim(
        CASE expected,
        CASE running
    ) { return Ready(expected) && current_case.compare_exchange_strong(expected, running); }

    // vTaskDelay não detém locks ou exclui/suspende uma tarefa com contexto em uso.
    void Pause(
        obc_health::TASK task,
        obc_health::ACTIVITY activity,
        uint32_t duration_ms,
        CASE next
    ) {
        Serial.printf("[HEALTH-CHECK] pause_begin=%s activity=%s duration_ms=%lu monotonic_ms=%llu\n",
            obc_health::TaskName(task), obc_health::ActivityName(activity), static_cast<unsigned long>(duration_ms),
            static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        Serial.printf("[HEALTH-CHECK] pause_end=%s monotonic_ms=%llu\n",
            obc_health::TaskName(task), static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
        Arm(next, 1000);
    }

} // namespace

namespace obc_health {
namespace bench {

void Before(
    TASK task,
    ACTIVITY activity
) {
    if (task == TASK::UART && activity == ACTIVITY::TRANSPORT && Claim(CASE::UART_PENDING, CASE::UART_RUNNING)) {
        Pause(task, activity, 300, CASE::PROCESS_PENDING);
    } else if (task == TASK::PROCESS && activity == ACTIVITY::PROCESSING
            && Claim(CASE::PROCESS_PENDING, CASE::PROCESS_RUNNING)) {
        Pause(task, activity, 2500, CASE::WRITE_PENDING);
    } else if (task == TASK::WRITER && activity == ACTIVITY::WRITE
            && Claim(CASE::WRITE_PENDING, CASE::WRITE_RUNNING)) {
        Pause(task, activity, 5500, CASE::SCAN_PENDING);
    } else if (task == TASK::WRITER && current_case.load() == CASE::SCAN_RUNNING
            && (activity == ACTIVITY::READ || activity == ACTIVITY::LIST) && slow_operations < 32) {
        ++slow_operations;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

} // namespace bench
} // namespace obc_health

namespace obc_flow {
namespace esp32 {

void StartHealthBench(
) {
    CASE expected = CASE::OFF;
    if (current_case.compare_exchange_strong(expected, CASE::ARMING)) {
        Serial.printf("[HEALTH-CHECK] start=1 monotonic_ms=%llu\n",
            static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
        Arm(CASE::UART_PENDING, 3000);
    }
}

bool HealthBenchScanRequested(
) { return Ready(CASE::SCAN_PENDING); }

void RunHealthBenchScan(
    const obc_delivery::READER_IO& reader
) {
    if (!Claim(CASE::SCAN_PENDING, CASE::SCAN_RUNNING)) return;
    const uint64_t started = obc_time::esp32::GetMonotonicMs();
    Serial.printf("[HEALTH-CHECK] scan_begin=1 monotonic_ms=%llu\n", static_cast<unsigned long long>(started));
    while (slow_operations < 32) {
        obc_delivery::RECOVERY audit = {};
        if (!obc_delivery::Audit(reader, audit)) {
            Serial.printf("[HEALTH-CHECK] FAIL: varredura simulada\n");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    Serial.printf("[HEALTH-CHECK] scan_end=1 operations=%u elapsed_ms=%llu monotonic_ms=%llu\n",
        slow_operations, static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs() - started),
        static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
    Arm(CASE::RECOVERY_PENDING, 1000);
}

bool FailHealthBenchWrite(
) {
    if (!Claim(CASE::RECOVERY_PENDING, CASE::RECOVERY_RUNNING)) return false;
    Serial.printf("[HEALTH-CHECK] recovery_begin=1 monotonic_ms=%llu\n",
        static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
    return true;
}

void ObserveHealthBenchRecovery(
    uint64_t recovered
) {
    CASE expected = CASE::RECOVERY_RUNNING;
    if (recovered && current_case.compare_exchange_strong(expected, CASE::DONE)) {
        Serial.printf("[HEALTH-CHECK] recovery_end=1 done=1 monotonic_ms=%llu\n",
            static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
    }
}

} // namespace esp32
} // namespace obc_flow
