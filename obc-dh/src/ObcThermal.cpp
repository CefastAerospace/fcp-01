#include "ObcThermal.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcThermal.h".
 */

namespace obc_thermal {
namespace {

    // Soma sem wrap; a saturação não transforma um prazo futuro em vencido.
    uint64_t Deadline(
        uint64_t now_ms,
        uint64_t interval_ms
    ) {
        const uint64_t maximum = std::numeric_limits<uint64_t>::max();
        return maximum - now_ms < interval_ms ? maximum : now_ms + interval_ms;
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

bool CanExecute(
    ACTION action,
    uint64_t expected_cycles,
    uint64_t current_cycles,
    bool service_ready
) {
    return service_ready && (action == ACTION::CONFIGURE
        || (action == ACTION::READ && expected_cycles == current_cycles));
}

bool IsAvailable(
    const CHANNEL_STATE& state,
    uint64_t now_ms,
    bool service_ready,
    uint64_t recovery_cycles
) {
    return service_ready && state.recovery_cycles == recovery_cycles
        && state.configured && state.has_valid && state.latest_valid
        && now_ms >= state.last_valid_ms && now_ms - state.last_valid_ms <= stale_after_ms;
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
ACTION Channel::Next(
    uint64_t now_ms,
    bool service_ready,
    uint64_t recovery_cycles
) {
    if (!service_ready || recovery_cycles != state.recovery_cycles) {
        state.configured = false;
        state.latest_valid = false;
        state.next_due_ms = 0;
        state.recovery_cycles = recovery_cycles;
    }
    if (!service_ready || now_ms < state.next_due_ms) return ACTION::WAIT;
    return state.configured ? ACTION::READ : ACTION::CONFIGURE;
}

// Público:
void Channel::Complete(
    ACTION action,
    uint64_t now_ms,
    uint64_t recovery_cycles,
    bool valid,
    bool configured
) {
    if (action == ACTION::WAIT) return;
    state.attempted = true;
    state.attempted_ms = now_ms;
    state.recovery_cycles = recovery_cycles;
    state.configured = configured;
    state.latest_valid = action == ACTION::READ && valid && configured;
    if (state.latest_valid) {
        state.has_valid = true;
        state.last_valid_ms = now_ms;
    }
    const uint64_t delay_ms = !configured ? retry_period_ms
        : action == ACTION::CONFIGURE ? first_sample_delay_ms : acquisition_period_ms;
    state.next_due_ms = Deadline(now_ms, delay_ms);
}

// ─── GETTERS ──────────────────────────────────────────────────────

// Público:
CHANNEL_STATE Channel::GetState(
) const { return state; }

} // namespace obc_thermal
