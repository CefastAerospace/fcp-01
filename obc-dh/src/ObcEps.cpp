#include "ObcEps.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcEps.h".
 */

namespace obc_eps {

// ─── HELPERS ──────────────────────────────────────────────────────

i2c::OPERATION_RESULT MapWireFailure(
    bool failed,
    bool buffer_failure,
    bool short_read,
    uint8_t status
) {
    using RESULT = i2c::OPERATION_RESULT;
    if (!failed) return RESULT::OK;
    if (buffer_failure) return RESULT::OPERATION_FAILED;
    if (short_read) return RESULT::READ_INCOMPLETE;
    switch (status) {
        case 2:
        case 3: return RESULT::DEVICE_NACK;
        case 5: return RESULT::TRANSFER_TIMEOUT;
        case 4: return RESULT::BUS_ERROR;
        default: return RESULT::OPERATION_FAILED;
    }
}

bool IsAvailable(
    const CHANNEL_STATE& state,
    uint64_t now_ms
) {
    return state.configured && state.has_valid && state.latest_valid
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
    // Um ciclo novo é observado antes de qualquer novo uso da configuração.
    if (!service_ready || recovery_cycles != state_.recovery_cycles) {
        state_.configured = false;
        state_.latest_valid = false;
        state_.next_due_ms = 0;
        state_.recovery_cycles = recovery_cycles;
    }
    if (!service_ready || now_ms < state_.next_due_ms) return ACTION::WAIT;
    return state_.configured ? ACTION::READ : ACTION::CONFIGURE;
}

// Público:
void Channel::Complete(
    ACTION action,
    uint64_t now_ms,
    uint64_t recovery_cycles,
    bool valid,
    bool online
) {
    if (action == ACTION::WAIT) return;
    state_.attempted = true;
    state_.attempted_ms = now_ms;
    state_.recovery_cycles = recovery_cycles;
    state_.configured = online;
    state_.latest_valid = action == ACTION::READ && valid && online;
    if (state_.latest_valid) {
        state_.has_valid = true;
        state_.last_valid_ms = now_ms;
    }
    state_.next_due_ms = now_ms + (!online ? retry_period_ms
        : action == ACTION::CONFIGURE ? conversion_wait_ms : acquisition_period_ms);
}

// ─── GETTERS ──────────────────────────────────────────────────────

// Público:
CHANNEL_STATE Channel::GetState(
) const { return state_; }

} // namespace obc_eps
