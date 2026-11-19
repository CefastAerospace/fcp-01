#include "ObcProtection.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcProtection.h".
 */

namespace obc_protection {
namespace {

    // A idade máxima é inclusiva. Valor histórico ou futuro não conta como leitura.
    bool IsUsable(
        const SAMPLE& sample,
        uint64_t now_ms,
        uint64_t max_age_ms
    ) {
        return sample.available && std::isfinite(sample.value) && sample.at_ms <= now_ms
            && now_ms - sample.at_ms <= max_age_ms;
    }

} // namespace

    Evaluator::Evaluator(
        const POLICY& policy,
        uint64_t boot_ms
    ) : policy_(policy), boot_ms_(boot_ms) {}

    RESULT Evaluator::Evaluate(
        uint64_t now_ms,
        const READINGS& readings
    ) {
        RESULT result = {};
        const auto& battery = readings.battery;
        const bool battery_usable = IsUsable(battery, now_ms, policy_.max_age_ms);
        bool can_recover = battery_usable && battery.value >= policy_.battery_resume_v;
        uint64_t oldest_ms = battery.at_ms;
        if (!battery_usable) result.unavailable = 1;
        else if (battery.value <= policy_.battery_trip_v) result.below_limit = 1;

        for (size_t index = 0; index < obc_data::thermal_sensors; ++index) {
            const uint8_t channel = 1U << (index + 1);
            const auto& sample = readings.temperatures[index];
            const auto& limits = policy_.temperatures[index];
            if (!IsUsable(sample, now_ms, policy_.max_age_ms)) {
                result.unavailable |= channel;
                can_recover = false;
                continue;
            }
            if (sample.value <= limits.trip_low) result.below_limit |= channel;
            if (sample.value >= limits.trip_high) result.above_limit |= channel;
            can_recover = can_recover && sample.value >= limits.resume_low && sample.value <= limits.resume_high;
            if (sample.at_ms < oldest_ms) oldest_ms = sample.at_ms;
        }

        if (!result.unavailable) has_complete_readings_ = true;
        const bool grace_expired = now_ms >= boot_ms_ && now_ms - boot_ms_ >= policy_.startup_grace_ms;
        const bool trip = result.below_limit || result.above_limit
            || (result.unavailable && (has_complete_readings_ || grace_expired));
        if (trip) protection_active_ = true;

        // A amostra mais antiga também precisa confirmar o fim da estabilização.
        if (can_recover && !trip) {
            if (!in_return_band_) recovery_since_ms_ = now_ms;
            in_return_band_ = true;
            if (oldest_ms >= recovery_since_ms_ && oldest_ms - recovery_since_ms_ >= policy_.recovery_dwell_ms) {
                protection_active_ = false;
                result.recovery_allowed = true;
            }
        } else {
            in_return_band_ = false;
        }

        result.protection_active = protection_active_;
        result.starts_allowed = !result.unavailable && !protection_active_;
        return result;
    }
} // namespace obc_protection
