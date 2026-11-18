#include "ObcWatchdogEsp32.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcWatchdogEsp32.h".
 */

namespace obc_watchdog {
namespace esp32 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    bool attempted = false;
    bool watchdog_ready = false;
    bool runtime = false;
    bool sampled = false;
    bool reset_pending = false;
    uint64_t last_sample_ms = 0;
    uint8_t previous_mask = 0;
    esp_timer_handle_t reset_timer = nullptr;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Executa no serviço nativo de timers, sem acessar filas, SD ou o loop OBC.
    void Restart(
        void*
    ) {
#ifdef OBC_WATCHDOG_BENCH_CASE
        obc_watchdog_bench::ResetTimerFired();
#endif
        esp_restart();
    }

    // Alimentação pertence exclusivamente à tarefa inscrita, nunca a um worker.
    void Feed(
    ) {
        if (!watchdog_ready) return;
        const esp_err_t result = esp_task_wdt_reset();
        if (result != ESP_OK) {
            watchdog_ready = false;
            Serial.printf("[WATCHDOG] feed_error=%d armed=0\n", result);
        }
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

void Init(
) {
    if (attempted) return;
    attempted = true;
    esp_err_t result = esp_task_wdt_init(watchdog_seconds, true);
#if defined(OBC_WATCHDOG_BENCH_CASE) && OBC_WATCHDOG_BENCH_CASE == 11
    result = ESP_FAIL;
#endif
    if (result == ESP_OK) result = esp_task_wdt_add(nullptr);
    watchdog_ready = result == ESP_OK;
    Serial.printf("[WATCHDOG] armed=%u timeout_ms=%lu error=%d reset_reason=%u\n",
        watchdog_ready ? 1U : 0U, static_cast<unsigned long>(watchdog_seconds * 1000),
        result, static_cast<unsigned>(esp_reset_reason()));

    esp_timer_create_args_t arguments = {};
    arguments.callback = Restart;
    arguments.dispatch_method = ESP_TIMER_TASK;
    arguments.name = "obc_reset";
#if defined(OBC_WATCHDOG_BENCH_CASE) && OBC_WATCHDOG_BENCH_CASE == 12
    const esp_err_t timer_result = ESP_FAIL;
#else
    const esp_err_t timer_result = esp_timer_create(&arguments, &reset_timer);
#endif
    Serial.printf("[WATCHDOG] reset_timer_ready=%u error=%d\n", reset_timer ? 1U : 0U, timer_result);
    Checkpoint();
}

void Checkpoint(
) { if (!runtime) Feed(); }

void StartRuntime(
) { runtime = true; }

void Service(
    const LINK_DIAGNOSTICS& ttc
) {
    if (!runtime) return;
    const uint64_t entered_ms = obc_time::esp32::GetMonotonicMs();
    if (sampled && entered_ms - last_sample_ms < evaluation_period_ms) return;

    // Cópias sequenciais, sem aninhar locks dos proprietários.
    obc_health::PROGRESS progress[obc_health::task_count] = {};
    obc_flow::esp32::GetTaskProgress(progress);
    const auto bus = i2c::GetDiagnostics();
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    SAMPLE samples[resource_count] = {};
    for (size_t index = 0; index < obc_health::task_count; ++index) {
        samples[index] = {progress[index].enabled, AgeMs(now_ms, progress[index].since_ms)};
    }
    samples[3] = {bus.worker_created, Age32Ms(bus.sampled_ms, bus.worker_progress_ms)};
    samples[4] = {ttc.task_started, Age32Ms(ttc.sampled_ms, ttc.progress_ms)};
    const uint8_t mask = BlockedMask(samples);
    sampled = true;
    last_sample_ms = now_ms;
    if (!mask) Feed();

    // Registra somente transições. Falha de registro nunca libera alimentação.
    const uint8_t changed = mask ^ previous_mask;
    previous_mask = mask;
    for (size_t index = 0; index < resource_count; ++index) {
        const uint8_t bit = static_cast<uint8_t>(1U << index);
        if (!(changed & bit)) continue;
        const bool blocked = (mask & bit) != 0;
        char description[72];
        std::snprintf(description, sizeof(description), "watchdog %s %s age=%llu",
            ResourceName(static_cast<RESOURCE>(index)), blocked ? "blocked" : "resumed",
            static_cast<unsigned long long>(samples[index].age_ms));
        Serial.printf("[WATCHDOG] resource=%s blocked=%u age_ms=%llu monotonic_ms=%llu\n",
            ResourceName(static_cast<RESOURCE>(index)), blocked ? 1U : 0U,
            static_cast<unsigned long long>(samples[index].age_ms), static_cast<unsigned long long>(now_ms));
        obc_flow::esp32::RecordWatchdogEvent(CODE::PROGRESS, blocked, description);
    }
}

bool RequestReset(
) {
    if (reset_pending) return true;
    if (!reset_timer) return false;
#if defined(OBC_WATCHDOG_BENCH_CASE) && OBC_WATCHDOG_BENCH_CASE == 13
    const esp_err_t result = ESP_FAIL;
#else
    const esp_err_t result = esp_timer_start_once(reset_timer, static_cast<uint64_t>(reset_delay_ms) * 1000);
#endif
    if (result != ESP_OK) {
        Serial.printf("[WATCHDOG] reset_schedule_error=%d pending=0\n", result);
        return false;
    }
    reset_pending = true;
    Serial.printf("[WATCHDOG] reset_scheduled=1 delay_ms=%lu monotonic_ms=%llu\n",
        static_cast<unsigned long>(reset_delay_ms),
        static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
    obc_flow::esp32::RecordWatchdogEvent(CODE::RESET_REQUEST, false, "reset_obc accepted; ACK best effort");
    return true;
}

} // namespace esp32
} // namespace obc_watchdog
