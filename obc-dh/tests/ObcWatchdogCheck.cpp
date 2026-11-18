/** @file ObcWatchdogCheck.cpp
 * @brief Fronteiras e recuperação da decisão real de alimentação do watchdog.
 */
#include "ObcWatchdog.h" // Política portátil usada pelo adaptador ESP32.
#include <cassert>
#include <cstdio>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcWatchdog.h".
 */

int main(
) {
    using namespace obc_watchdog;
    const uint32_t limits[] = {500, 5000, 10000, 2000, 5000};
    SAMPLE samples[resource_count] = {};
    assert(BlockedMask(samples) == 0);
    for (size_t index = 0; index < resource_count; ++index) {
        const auto resource = static_cast<RESOURCE>(index);
        assert(GetLimitMs(resource) == limits[index]);
        samples[index] = {false, UINT64_MAX};
        assert(BlockedMask(samples) == 0);
        samples[index] = {true, limits[index] - 1};
        assert(BlockedMask(samples) == 0);
        samples[index].age_ms = limits[index];
        assert(BlockedMask(samples) == 0);
        ++samples[index].age_ms;
        assert(BlockedMask(samples) == (1U << index));
        // Progresso novo libera imediatamente; suspeita não fica retida.
        samples[index].age_ms = 0;
        assert(BlockedMask(samples) == 0);
    }
    for (auto& sample : samples) sample = {true, UINT64_MAX};
    assert(BlockedMask(samples) == 31);
    samples[2].age_ms = 0;
    assert(BlockedMask(samples) == 27);
    for (auto& sample : samples) sample = {true, 0};
    assert(BlockedMask(samples) == 0);
    assert(AgeMs(0, 0) == 0);
    assert(AgeMs(5001, 0) == 5001);
    assert(AgeMs(10, 11) == 0);
    assert(AgeMs(UINT64_MAX, UINT64_MAX - 10000) == 10000);
    assert(Age32Ms(0, 0) == 0);
    assert(Age32Ms(100, UINT32_MAX - 99) == 200);
    assert(Age32Ms(5000, UINT32_MAX) == 5001);
    assert(watchdog_seconds == 5 && reset_delay_ms == 1500 && evaluation_period_ms == 100);
    std::puts("ObcWatchdogCheck passed");
}
