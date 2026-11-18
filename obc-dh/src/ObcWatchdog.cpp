#include "ObcWatchdog.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcWatchdog.h".
 */

namespace obc_watchdog {

// ─── HELPERS ──────────────────────────────────────────────────────

uint32_t GetLimitMs(
    RESOURCE resource
) {
    switch (resource) {
        case RESOURCE::UART: return 500;
        case RESOURCE::PROCESS: return 5000;
        case RESOURCE::WRITER: return 10000;
        case RESOURCE::I2C: return 2000;
        case RESOURCE::TTC: return 5000;
    }
    return 0;
}

const char* ResourceName(
    RESOURCE resource
) {
    switch (resource) {
        case RESOURCE::UART: return "uart";
        case RESOURCE::PROCESS: return "process";
        case RESOURCE::WRITER: return "writer";
        case RESOURCE::I2C: return "i2c";
        case RESOURCE::TTC: return "ttc";
    }
    return "invalid";
}

uint64_t AgeMs(
    uint64_t now_ms,
    uint64_t progress_ms
) { return now_ms >= progress_ms ? now_ms - progress_ms : 0; }

uint32_t Age32Ms(
    uint32_t now_ms,
    uint32_t progress_ms
) { return now_ms - progress_ms; }

uint8_t BlockedMask(
    const SAMPLE (&samples)[resource_count]
) {
    uint8_t mask = 0;
    for (size_t index = 0; index < resource_count; ++index) {
        if (samples[index].active && samples[index].age_ms > GetLimitMs(static_cast<RESOURCE>(index))) {
            mask |= static_cast<uint8_t>(1U << index);
        }
    }
    return mask;
}

} // namespace obc_watchdog
