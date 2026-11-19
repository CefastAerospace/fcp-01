#include "ObcModes.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcModes.h".
 */

namespace obc_modes {

    using obc_data::MODE;

    bool IsFault(
        CAUSE cause
    ) {
        return cause == CAUSE::PAYLOAD_LOST || cause == CAUSE::JOURNAL_LOST
            || cause == CAUSE::ADCS_START_FAILED || cause == CAUSE::ADCS_STOP_FAILED;
    }

    const char* CauseName(
        CAUSE cause
    ) {
        switch (cause) {
            case CAUSE::START_MISSION: return "start_mission";
            case CAUSE::STOP_MISSION: return "stop_mission";
            case CAUSE::DEADLINE: return "deadline";
            case CAUSE::PAYLOAD_LOST: return "payload_lost";
            case CAUSE::JOURNAL_LOST: return "journal_lost";
            case CAUSE::START_ADCS: return "start_adcs";
            case CAUSE::STOP_ADCS: return "stop_adcs";
            case CAUSE::ADCS_DONE: return "adcs_done";
            case CAUSE::ENTER_SAFE: return "enter_safe";
            case CAUSE::PROTECTION: return "protection";
            case CAUSE::RECOVERY: return "recovery";
            case CAUSE::ADCS_START_FAILED: return "adcs_start_failed";
            case CAUSE::ADCS_STOP_FAILED: return "adcs_stop_failed";
        }
        return "unknown";
    }

    Coordinator::Coordinator(
        bool operational
    ) : mode_(operational ? MODE::SAFE : MODE::BENCH) {}

    void Coordinator::Transition(
        const EVENT& event,
        const ACTIONS& actions
    ) {
        mode_ = event.to;
        actions.record(event);
    }

    bool Coordinator::Enter(
        MODE target,
        CAUSE cause,
        uint64_t now_ms,
        const ACTIONS& actions
    ) {
        if (mode_ == MODE::DETUMBLING && !actions.stop_adcs()) {
            stop_pending_ = true;
            stop_target_ = target;
            stop_cause_ = cause;
            actions.record({mode_, mode_, CAUSE::ADCS_STOP_FAILED, now_ms});
            return false;
        }
        Transition({mode_, target, cause, now_ms}, actions);
        return true;
    }

    void Coordinator::Service(
        uint64_t now_ms,
        const SUBSYSTEM_STATUS& subsystems,
        const MISSION_STATUS& mission,
        const ACTIONS& actions
    ) {
        if (mode_ == MODE::BENCH) return;

        // Uma proteção permanece pendente até a confirmação de parada.
        if (stop_pending_) {
            if (subsystems.protection_active) {
                stop_target_ = MODE::LOW_POWER;
                stop_cause_ = CAUSE::PROTECTION;
            }
            if (subsystems.adcs_stopped) {
                stop_pending_ = false;
                Transition({mode_, stop_target_, stop_cause_, now_ms}, actions);
            }
        } else if (subsystems.protection_active && mode_ != MODE::LOW_POWER) {
            Enter(MODE::LOW_POWER, CAUSE::PROTECTION, now_ms, actions);
        } else if (mode_ == MODE::DETUMBLING && subsystems.adcs_done) {
            Enter(MODE::SAFE, CAUSE::ADCS_DONE, now_ms, actions);
        } else if (mode_ == MODE::MISSION) {
            if (!mission.flow_ready || !mission.payload_recent) {
                Transition({mode_, MODE::SAFE, CAUSE::PAYLOAD_LOST, now_ms}, actions);
            } else if (!mission.journal_ready) {
                Transition({mode_, MODE::SAFE, CAUSE::JOURNAL_LOST, now_ms}, actions);
            } else if (now_ms - mission_started_ms_ >= mission_duration_ms) {
                Transition({mode_, MODE::SAFE, CAUSE::DEADLINE, now_ms}, actions);
            }
        }
    }

    bool Coordinator::Execute(
        COMMAND command,
        uint64_t now_ms,
        const SUBSYSTEM_STATUS& subsystems,
        const MISSION_STATUS& mission,
        const ACTIONS& actions
    ) {
        if (mode_ == MODE::BENCH || stop_pending_ || subsystems.protection_active) return false;

        switch (command) {
            case COMMAND::START_MISSION:
                if (mode_ != MODE::SAFE || !subsystems.boot_ready || !mission.flow_ready
                        || !mission.payload_recent || !mission.journal_ready || !mission.utc_valid) return false;
                if (!actions.record({mode_, MODE::MISSION, CAUSE::START_MISSION, now_ms})) return false;
                mission_started_ms_ = now_ms;
                mode_ = MODE::MISSION;
                return true;
            case COMMAND::STOP_MISSION:
                return mode_ == MODE::MISSION && Enter(MODE::SAFE, CAUSE::STOP_MISSION, now_ms, actions);
            case COMMAND::START_ADCS:
                if (mode_ != MODE::SAFE || !subsystems.boot_ready || !subsystems.adcs_allowed) return false;
                if (!actions.start_adcs()) {
                    actions.record({mode_, mode_, CAUSE::ADCS_START_FAILED, now_ms});
                    return false;
                }
                Transition({mode_, MODE::DETUMBLING, CAUSE::START_ADCS, now_ms}, actions);
                return true;
            case COMMAND::STOP_ADCS:
                return mode_ == MODE::DETUMBLING && Enter(MODE::SAFE, CAUSE::STOP_ADCS, now_ms, actions);
            case COMMAND::ENTER_SAFE:
                return mode_ == MODE::SAFE
                    || (mode_ != MODE::LOW_POWER && Enter(MODE::SAFE, CAUSE::ENTER_SAFE, now_ms, actions));
            case COMMAND::EXIT_SAFE:
                return mode_ == MODE::LOW_POWER && subsystems.recovery_allowed
                    && Enter(MODE::SAFE, CAUSE::RECOVERY, now_ms, actions);
        }
        return false;
    }

    MODE Coordinator::GetMode() const {
        return mode_;
    }

    bool Coordinator::IsProtectionInEffect() const {
        return mode_ == MODE::LOW_POWER || (stop_pending_ && stop_target_ == MODE::LOW_POWER);
    }

} // namespace obc_modes
