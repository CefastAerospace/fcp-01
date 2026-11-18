#include "ObcWatchdogBench.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcWatchdogBench.h".
 */

#ifndef OBC_WATCHDOG_BENCH_CASE
#error "Usar somente as bancadas int012"
#endif

namespace obc_watchdog_bench {
namespace {

    // ─── STRUCTS ──────────────────────────────────────────────────────

    struct COOKIE {
        uint32_t magic;
        uint32_t case_id;
        uint64_t boot_id;
        uint32_t timer_fired_ms;
    };

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    RTC_NOINIT_ATTR COOKIE cookie;
    constexpr uint32_t cookie_magic = 0x57445412;
    constexpr unsigned case_id = OBC_WATCHDOG_BENCH_CASE;
    std::atomic<bool> armed(false);
    std::atomic<bool> claimed(false);
    uint64_t current_boot = 0;
    bool resumed = false;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Publicado antes da espera. Somente o vencedor da injeção escreve o cookie.
    void ExpectReset(
    ) { cookie = {cookie_magic, case_id, current_boot, 0}; }

    // A espera cede CPU, é finita e nunca mantém locks/contextos de outro chamador.
    void Pause(
        const char* resource,
        uint32_t duration_ms,
        bool expect_reset
    ) {
        if (expect_reset) ExpectReset();
        const uint32_t started_ms = millis();
        Serial.printf("[WATCHDOG-CHECK] pause=%s duration_ms=%lu monotonic_ms=%lu\n",
            resource, static_cast<unsigned long>(duration_ms), static_cast<unsigned long>(started_ms));
        while (millis() - started_ms < duration_ms) vTaskDelay(pdMS_TO_TICKS(1));
        Serial.printf("[WATCHDOG-CHECK] pause_return=%s monotonic_ms=%lu\n",
            resource, static_cast<unsigned long>(millis()));
    }

    // Uma única tarefa pode reivindicar a injeção, sem suspender ou excluir workers.
    bool Claim(
    ) {
        bool expected = false;
        return armed.load() && claimed.compare_exchange_strong(expected, true);
    }

    // Contexto nulo é estável; Execute conserva a chamada até seu término ou reset.
    bool I2cOperation(
        TwoWire&,
        void*
    ) {
        Before(obc_watchdog::RESOURCE::I2C);
        return true;
    }

    void I2cClient(
        void*
    ) {
        const auto result = i2c::Execute(I2cOperation, nullptr);
        Serial.printf("[WATCHDOG-CHECK] i2c_return=%u\n", static_cast<unsigned>(result));
        vTaskDelete(nullptr);
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

void Boot(
    uint64_t boot_id
) {
    current_boot = boot_id;
    const auto reason = esp_reset_reason();
    resumed = cookie.magic == cookie_magic && cookie.case_id == case_id && cookie.boot_id != 0
        && (reason == ESP_RST_TASK_WDT || reason == ESP_RST_SW);
    Serial.printf("[WATCHDOG-CHECK] case=%u boot_id=%llu reset_reason=%u resumed=%u previous_boot=%llu timer_fired_ms=%lu monotonic_ms=%lu\n",
        case_id, static_cast<unsigned long long>(boot_id), static_cast<unsigned>(reason), resumed ? 1U : 0U,
        static_cast<unsigned long long>(resumed ? cookie.boot_id : 0),
        static_cast<unsigned long>(resumed ? cookie.timer_fired_ms : 0), static_cast<unsigned long>(millis()));
    // TT&C reabre esta UART; conclui o marcador antes de substituir o driver.
    Serial.flush();
    if (!resumed) cookie = {};
    if (case_id == 7 && !resumed) Pause("startup", 8000, true);
}

void Start(
) {
    if (resumed || armed.exchange(true)) return;
    Serial.printf("[WATCHDOG-CHECK] start=1 case=%u\n", case_id);
    if (case_id == 4) {
        if (xTaskCreate(I2cClient, "wdt_i2c_client", 3072, nullptr, 2, nullptr) != pdPASS) {
            Serial.println("[WATCHDOG-CHECK] FAIL: tarefa cliente I2C");
        }
    }
}

void Before(
    obc_watchdog::RESOURCE resource
) {
    const unsigned index = static_cast<unsigned>(resource);
    const bool returns = case_id == 8 && resource == obc_watchdog::RESOURCE::UART;
    if ((case_id != index + 1 && !returns) || !Claim()) return;
    Pause(obc_watchdog::ResourceName(resource), returns ? 900 : obc_watchdog::GetLimitMs(resource) + 8000,
        !returns);
}

void BeforeLoop(
) { if (case_id == 6 && Claim()) Pause("supervisor", 8000, true); }

void BeforeReset(
    Ttc& link
) {
    if (case_id != 9 || claimed.exchange(true)) return;
    const bool held = link.HoldTransmit(true, 100);
    unsigned filled = 0;
    for (unsigned index = 0; index < 4; ++index) {
        if (link.SendAck(static_cast<uint16_t>(61000 + index), ttc::CMD_PING, ttc::ACK_OK)) ++filled;
    }
    Serial.printf("[WATCHDOG-CHECK] ack_held=%u ack_filled=%u\n", held ? 1U : 0U, filled);
}

void AfterReset(
) {
    ExpectReset();
    if (case_id == 10 && !claimed.exchange(true)) Pause("reset_consumer", 4000, true);
}

void ResetTimerFired(
) { cookie.timer_fired_ms = millis(); }

void ObserveRecord(
    const uint8_t* bytes,
    size_t size
) {
    if (size != 96 || std::memcmp(bytes, obc_data::record_magic, 4)) return;
    obc_record::RECORD record = {};
    size_t consumed = 0;
    if (obc_record::Decode(bytes, size, record, consumed) != obc_record::STATUS::OK) return;
    if ((record.header.type == obc_data::RECORD_TYPE::FAULT || record.header.type == obc_data::RECORD_TYPE::EVENT)
            && record.body.event.source == obc_data::SOURCE::OBC
            && (record.body.event.code == 265 || record.body.event.code == 266)) {
        Serial.printf("[WATCHDOG-CHECK] record_code=%u kind=%s\n", record.body.event.code,
            record.header.type == obc_data::RECORD_TYPE::FAULT ? "fault" : "event");
    }
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool Radio::Init(
) { return false; }

// Público:
bool Radio::IsReady(
) { return false; }

// Público:
bool Radio::Send(
    const uint8_t*,
    size_t
) { return false; }

// Público:
int Radio::Receive(
    uint8_t*,
    size_t
) {
    Before(obc_watchdog::RESOURCE::TTC);
    return 0;
}

// Público:
int16_t Radio::GetRssi(
) { return 0; }

// Público:
float Radio::GetSnr(
) { return 0; }

} // namespace obc_watchdog_bench
