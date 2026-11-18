#include "ObcTaskHealth.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcTaskHealth.h".
 */

namespace obc_health {

// ─── HELPERS ──────────────────────────────────────────────────────

void Start(
    PROGRESS& progress,
    uint64_t now_ms
) { progress = {true, ACTIVITY::STARTING, now_ms, 0, 0}; }

void Mark(
    PROGRESS& progress,
    ACTIVITY activity,
    uint64_t now_ms,
    bool completed
) {
    if (!progress.enabled) return;
    progress.activity = activity;
    progress.since_ms = now_ms;
    if (completed) {
        progress.last_completed_ms = now_ms;
        if (progress.completions != std::numeric_limits<uint64_t>::max()) ++progress.completions;
    }
}

uint64_t GetLimitMs(
    TASK task
) {
    switch (task) {
        case TASK::UART: return 100;
        case TASK::PROCESS: return 2000;
        case TASK::WRITER: return 5000;
    }
    return 0;
}

const char* TaskName(
    TASK task
) {
    switch (task) {
        case TASK::UART: return "uart";
        case TASK::PROCESS: return "process";
        case TASK::WRITER: return "writer";
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
    }
    return "invalid";
}

const char* ActivityName(
    ACTIVITY activity
) {
    switch (activity) {
        case ACTIVITY::INACTIVE: return "inactive";
        case ACTIVITY::STARTING: return "starting";
        case ACTIVITY::IDLE: return "idle";
        case ACTIVITY::TRANSPORT: return "transport";
        case ACTIVITY::PROCESSING: return "processing";
        case ACTIVITY::ENCODING: return "encoding";
        case ACTIVITY::OPEN: return "open";
        case ACTIVITY::WRITE: return "write";
        case ACTIVITY::CLOSE: return "close";
        case ACTIVITY::READ: return "read";
        case ACTIVITY::LIST: return "list";
        case ACTIVITY::UNMOUNT: return "unmount";
        case ACTIVITY::MOUNT: return "mount";
        case ACTIVITY::SCAN: return "scan";
        case ACTIVITY::RECOVERY_WAIT: return "recovery_wait";
    }
    return "invalid";
}

bool MakeEvent(
    const REPORT& report,
    const obc_time::EVENT_STAMP& stamp,
    obc_record::RECORD& record
) {
    const size_t index = static_cast<size_t>(report.task);
    if (!stamp.boot_id || index >= task_count
            || (report.state != STATE::OK && report.state != STATE::SUSPECT)) return false;
    obc_record::RECORD created = {};
    const bool suspect = report.state == STATE::SUSPECT;
    created.header = {1, suspect ? obc_data::RECORD_TYPE::FAULT : obc_data::RECORD_TYPE::EVENT, 96, stamp};
    created.body.event = {};
    created.body.event.source = obc_data::SOURCE::OBC;
    created.body.event.code = static_cast<uint16_t>(256 + index);
    created.body.event.severity = suspect ? obc_data::SEVERITY::ERROR : obc_data::SEVERITY::INFO;
    std::snprintf(created.body.event.description, sizeof(created.body.event.description), "%s %s %s age=%llu",
        TaskName(report.task), StateName(report.state), ActivityName(report.activity),
        static_cast<unsigned long long>(report.age_ms));
    record = created;
    return true;
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool Monitor::Evaluate(
    TASK task,
    const PROGRESS& progress,
    uint64_t now_ms,
    REPORT& report
) {
    const size_t index = static_cast<size_t>(task);
    if (index >= task_count) return false;
    const uint64_t age = progress.enabled && now_ms >= progress.since_ms ? now_ms - progress.since_ms : 0;
    const uint64_t limit = GetLimitMs(task);
    const STATE state = !progress.enabled ? STATE::INACTIVE : age > limit ? STATE::SUSPECT : STATE::OK;
    report = {task, state, progress.enabled ? progress.activity : ACTIVITY::INACTIVE,
        now_ms, age, limit, progress.completions};
    const bool changed = sampled_[index] ? previous_[index] != state : state == STATE::SUSPECT;
    sampled_[index] = true;
    previous_[index] = state;
    return changed && state != STATE::INACTIVE;
}

} // namespace obc_health
