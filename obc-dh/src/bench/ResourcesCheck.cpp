#include "ObcResourcesBench.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcResourcesBench.h".
 */

#ifndef OBC_RESOURCES_BENCH
#error "Usar somente int008-resources-check"
#endif

namespace obc_resources_bench {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    enum class CASE { OFF, WAITING, PAUSE_PENDING, PAUSING, RADIO_DOWN, RADIO_RETURN,
        MEMORY_LOW, MEMORY_BAND, MEMORY_RETURN, STACK_LOW, STACK_RETURN, ALLOCATION, DONE, FINISHED };
    std::atomic<CASE> current_case(CASE::OFF);
    std::atomic<uint32_t> ready_at_ms(0);
    std::atomic<bool> radio_available(true);
    uint64_t allocation_offset = 0; // Somente o loop altera esta injeção.

    // Publica prazo antes de permitir a execução do próximo cenário.
    void Arm(
        CASE next,
        uint32_t delay_ms
    ) {
        ready_at_ms.store(millis() + delay_ms);
        current_case.store(next);
    }

    // Prazos finitos menores que 2^31 ms conservam a comparação após wrap.
    bool Ready(
    ) { return static_cast<int32_t>(millis() - ready_at_ms.load()) >= 0; }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

void Start(
) {
    CASE expected = CASE::OFF;
    if (!current_case.compare_exchange_strong(expected, CASE::WAITING)) return;
    Serial.printf("[RESOURCE-CHECK] start=1\n");
    Arm(CASE::WAITING, 3000);
}

void OverrideMemory(
    obc_resource_health::MEMORY_SAMPLE& sample
) {
    switch (current_case.load()) {
        case CASE::WAITING:
            if (Ready()) Arm(CASE::PAUSE_PENDING, 0);
            break;
        case CASE::RADIO_DOWN:
            if (Ready()) {
                radio_available.store(true);
                Serial.printf("[RESOURCE-CHECK] radio_return=1\n");
                Arm(CASE::RADIO_RETURN, 2000);
            }
            break;
        case CASE::RADIO_RETURN:
            if (Ready()) Arm(CASE::MEMORY_LOW, 0);
            break;
        case CASE::MEMORY_LOW:
            sample.byte_heap = 8191;
            sample.byte_largest = 4095;
            Serial.printf("[RESOURCE-CHECK] memory=low\n");
            Arm(CASE::MEMORY_BAND, 0);
            break;
        case CASE::MEMORY_BAND:
            sample.byte_heap = 10000;
            sample.byte_largest = 5000;
            Serial.printf("[RESOURCE-CHECK] memory=band\n");
            Arm(CASE::MEMORY_RETURN, 0);
            break;
        case CASE::MEMORY_RETURN:
            sample.byte_heap = 12288;
            sample.byte_largest = 6144;
            Serial.printf("[RESOURCE-CHECK] memory=return\n");
            Arm(CASE::STACK_LOW, 0);
            break;
        case CASE::STACK_LOW:
            sample.stacks[2] = {true, 1023};
            Serial.printf("[RESOURCE-CHECK] stack=low\n");
            Arm(CASE::STACK_RETURN, 0);
            break;
        case CASE::STACK_RETURN:
            sample.stacks[2] = {true, 4096};
            Serial.printf("[RESOURCE-CHECK] stack=historical\n");
            Arm(CASE::ALLOCATION, 0);
            break;
        case CASE::ALLOCATION:
            allocation_offset = 1;
            Serial.printf("[RESOURCE-CHECK] allocation=1\n");
            Arm(CASE::DONE, 0);
            break;
        case CASE::DONE:
            Serial.printf("[RESOURCE-CHECK] done=1\n");
            // Publicar conclusão uma vez; amostras seguintes conservam o offset.
            current_case.store(CASE::FINISHED);
            break;
        default: break;
    }
    sample.allocation_failures += allocation_offset;
}

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
            && record.body.event.code >= 260 && record.body.event.code <= 264) {
        Serial.printf("[RESOURCE-CHECK] record_code=%u kind=%s\n", record.body.event.code,
            record.header.type == obc_data::RECORD_TYPE::FAULT ? "fault" : "event");
    }
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool Radio::Init(
) { return radio_available.load(); }

// Público:
bool Radio::CheckHealth(
) { return radio_available.load(); }

// Público:
bool Radio::IsReady(
) { return radio_available.load(); }

// Público:
bool Radio::Send(
    const uint8_t* data,
    size_t len
) { return data != nullptr && len > 0 && radio_available.load(); }

// Público:
int Radio::Receive(
    uint8_t* buffer,
    size_t capacity
) {
    CASE expected = CASE::PAUSE_PENDING;
    if (current_case.compare_exchange_strong(expected, CASE::PAUSING)) {
        Serial.printf("[RESOURCE-CHECK] pause_begin=1\n");
        const uint32_t started_ms = millis();
        // Garante duração mínima monotônica; não depende do arredondamento dos ticks.
        while (millis() - started_ms < 2300) vTaskDelay(pdMS_TO_TICKS(1));
        Serial.printf("[RESOURCE-CHECK] pause_end=1 elapsed_ms=%lu\n",
            static_cast<unsigned long>(millis() - started_ms));
        radio_available.store(false);
        Arm(CASE::RADIO_DOWN, 4000);
    }
    if (!contact_sent_) {
        const char frame[] = "{\"t\":\"tc\",\"seq\":60000,\"cmd\":\"ping\"}";
        if (capacity < sizeof(frame) - 1) return 0;
        std::memcpy(buffer, frame, sizeof(frame) - 1);
        contact_sent_ = true;
        return sizeof(frame) - 1;
    }
    return 0;
}

// Público:
int16_t Radio::GetRssi(
) { return -80; }

// Público:
float Radio::GetSnr(
) { return 9.5f; }

} // namespace obc_resources_bench
