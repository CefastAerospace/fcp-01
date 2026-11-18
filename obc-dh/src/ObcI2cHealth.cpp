#include "ObcI2cHealth.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcI2cHealth.h".
 */

namespace obc_i2c_health {

// ─── HELPERS ──────────────────────────────────────────────────────

const char* ServiceName(
    i2c::SERVICE_STATE service
) {
    switch (service) {
        case i2c::SERVICE_STATE::UNINITIALIZED: return "uninitialized";
        case i2c::SERVICE_STATE::READY: return "ready";
        case i2c::SERVICE_STATE::RECOVERING: return "recovering";
        case i2c::SERVICE_STATE::UNAVAILABLE: return "unavailable";
    }
    return "invalid";
}

const char* StateName(
    STATE state
) {
    switch (state) {
        case STATE::INACTIVE: return "inactive";
        case STATE::OK: return "ok";
        case STATE::SUSPECT: return "suspect";
        case STATE::UNAVAILABLE: return "unavailable";
    }
    return "invalid";
}

const char* ActivityName(
    ACTIVITY activity
) {
    switch (activity) {
        case ACTIVITY::INACTIVE: return "inactive";
        case ACTIVITY::IDLE: return "idle";
        case ACTIVITY::CALLBACK: return "callback";
        case ACTIVITY::RECOVERY: return "recovery";
    }
    return "invalid";
}

bool MakeEvent(
    const REPORT& report,
    const obc_time::EVENT_STAMP& stamp,
    obc_record::RECORD& record
) {
    if (!stamp.boot_id || report.state == STATE::INACTIVE) return false;
    const bool fault = report.state == STATE::SUSPECT || report.state == STATE::UNAVAILABLE;
    obc_record::RECORD created = {};
    created.header = {1, fault ? obc_data::RECORD_TYPE::FAULT : obc_data::RECORD_TYPE::EVENT, 96, stamp};
    created.body.event.source = obc_data::SOURCE::OBC;
    created.body.event.code = event_code;
    created.body.event.severity = fault ? obc_data::SEVERITY::ERROR : obc_data::SEVERITY::INFO;
    std::snprintf(created.body.event.description, sizeof(created.body.event.description),
        "i2c %s %s %s age=%lu", StateName(report.state), ServiceName(report.service),
        ActivityName(report.activity), static_cast<unsigned long>(report.age_ms));
    record = created;
    return true;
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool Monitor::Evaluate(
    const SAMPLE& sample,
    REPORT& report
) {
    REPORT next = {};
    next.service = sample.service;
    next.sampled_ms = sample.sampled_ms;
    switch (sample.service) {
        case i2c::SERVICE_STATE::UNINITIALIZED:
            next.state = STATE::INACTIVE;
            next.activity = ACTIVITY::INACTIVE;
            break;
        case i2c::SERVICE_STATE::UNAVAILABLE:
            next.state = STATE::UNAVAILABLE;
            next.activity = ACTIVITY::IDLE;
            break;
        case i2c::SERVICE_STATE::RECOVERING:
            next.state = STATE::OK;
            next.activity = ACTIVITY::RECOVERY;
            next.started_ms = sample.recovery_started_ms;
            next.limit_ms = recovery_limit_ms;
            break;
        case i2c::SERVICE_STATE::READY:
            next.state = STATE::OK;
            next.activity = sample.callback_running ? ACTIVITY::CALLBACK : ACTIVITY::IDLE;
            next.started_ms = sample.callback_running ? sample.callback_started_ms : 0;
            next.limit_ms = sample.callback_running ? callback_limit_ms : 0;
            break;
        default: return false;
    }

    // A amostra é obtida sob o mesmo mutex do início. Zero é instante válido;
    // subtração unsigned atravessa o wrap de millis sem misturar relógios.
    if (next.limit_ms) {
        next.age_ms = sample.sampled_ms - next.started_ms;
        const bool still_suspect = sampled_ && previous_.state == STATE::SUSPECT
            && previous_.service == next.service && previous_.activity == next.activity
            && previous_.started_ms == next.started_ms;
        if (next.age_ms > next.limit_ms || still_suspect) next.state = STATE::SUSPECT;
    }
    const bool changed = sampled_
        ? previous_.service != next.service || previous_.state != next.state
        : next.state == STATE::SUSPECT || next.state == STATE::UNAVAILABLE;
    report = next;
    previous_ = next;
    sampled_ = true;
    return changed && next.state != STATE::INACTIVE;
}

} // namespace obc_i2c_health
