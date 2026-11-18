#include "ObcResourceHealth.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcResourceHealth.h".
 */

namespace obc_resource_health {

// ─── HELPERS ──────────────────────────────────────────────────────

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

const char* StackName(
    STACK stack
) {
    switch (stack) {
        case STACK::UART: return "uart";
        case STACK::PROCESS: return "process";
        case STACK::WRITER: return "writer";
        case STACK::I2C: return "i2c";
        case STACK::TTC: return "ttc";
        case STACK::LOOP: return "loop";
    }
    return "invalid";
}

bool MakeEvent(
    CODE code,
    bool fault,
    const char* description,
    const obc_time::EVENT_STAMP& stamp,
    obc_record::RECORD& record
) {
    if (!stamp.boot_id || description == nullptr) return false;
    obc_record::RECORD created = {};
    created.header = {1, fault ? obc_data::RECORD_TYPE::FAULT : obc_data::RECORD_TYPE::EVENT, 96, stamp};
    created.body.event.source = obc_data::SOURCE::OBC;
    created.body.event.code = static_cast<uint16_t>(code);
    created.body.event.severity = fault ? obc_data::SEVERITY::ERROR : obc_data::SEVERITY::INFO;
    std::snprintf(created.body.event.description, sizeof(created.body.event.description), "%s", description);
    record = created;
    return true;
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
TTC_REPORT TtcMonitor::Evaluate(
    const TTC_SAMPLE& sample
) {
    TTC_REPORT report = {};
    report.age_ms = sample.task_started ? sample.sampled_ms - sample.progress_ms : 0;
    // Uma suspeita já vista não desaparece quando millis completa outro wrap.
    const bool held = sampled_ && previous_.progress == STATE::SUSPECT
        && previous_sample_.task_started && previous_sample_.progress_ms == sample.progress_ms
        && previous_sample_.completions == sample.completions;
    report.progress = !sample.task_started ? STATE::INACTIVE
        : report.age_ms > ttc_limit_ms || held ? STATE::SUSPECT : STATE::OK;
    report.radio = !sample.init_attempted ? STATE::INACTIVE
        : sample.initialized && sample.radio_ready ? STATE::OK : STATE::UNAVAILABLE;
    report.progress_changed = sampled_ ? previous_.progress != report.progress : report.progress == STATE::SUSPECT;
    report.radio_changed = sampled_ ? previous_.radio != report.radio : report.radio == STATE::UNAVAILABLE;
    previous_sample_ = sample;
    previous_ = report;
    sampled_ = true;
    return report;
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
MEMORY_REPORT MemoryMonitor::Evaluate(
    const MEMORY_SAMPLE& sample
) {
    MEMORY_REPORT report = {};
    report.heap_low = sample.byte_heap < (previous_.heap_low ? heap_leave_bytes : heap_enter_bytes);
    report.block_low = sample.byte_largest < (previous_.block_low ? block_leave_bytes : block_enter_bytes);
    report.pressure_changed = report.heap_low != previous_.heap_low || report.block_low != previous_.block_low;
    for (size_t index = 0; index < stack_count; ++index) {
        const uint8_t bit = static_cast<uint8_t>(1U << index);
        if (sample.stacks[index].active
                && (sample.stacks[index].free_bytes < stack_limit_bytes || (previous_.stack_low_mask & bit))) {
            report.stack_low_mask |= bit;
        }
    }
    report.stack_new_mask = report.stack_low_mask & ~previous_.stack_low_mask;
    report.allocation_delta = sample.allocation_failures >= allocation_failures_
        ? sample.allocation_failures - allocation_failures_ : 0;
    allocation_failures_ = sample.allocation_failures;
    previous_ = report;
    return report;
}

} // namespace obc_resource_health
