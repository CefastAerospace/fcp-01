/** @file ObcModesCheck.cpp
 * @brief Verificação portátil das transições, prazo e efeitos da INT-007.
 */
#include "ObcModes.h" // Coordenador usado pelo consumidor real.
#include <cassert>
#include <cstdio>
#include <vector>

namespace {
    using namespace obc_modes;
    using obc_data::MODE;

    std::vector<EVENT> events;
    bool journal_accepts = true;
    bool start_succeeds = true;
    bool stop_succeeds = true;
    unsigned starts = 0;
    unsigned stops = 0;
    constexpr SUBSYSTEM_STATUS ready = {true, true, false, true, false, false};
    constexpr MISSION_STATUS mission_ready = {true, true, true, true};

    // Conta cada pedido ao ADCS e cada tentativa de registrar um evento.
    const ACTIONS actions = {
        [] { ++starts; return start_succeeds; },
        [] { ++stops; return stop_succeeds; },
        [](const EVENT& event) { events.push_back(event); return journal_accepts; }
    };

    // Verifica recusas independentes e que a janela depende da admissão do marcador.
    void CheckAdmission() {
        Coordinator coordinator(true);
        const auto start = COMMAND::START_MISSION;
        const MISSION_STATUS unavailable[] = {
            {false, true, true, true}, {true, false, true, true},
            {true, true, false, true}, {true, true, true, false}
        };
        for (const auto& mission : unavailable) assert(!coordinator.Execute(start, 0, ready, mission, actions));
        auto subsystems = ready;
        subsystems.boot_ready = false;
        assert(!coordinator.Execute(start, 0, subsystems, mission_ready, actions));
        assert(!Coordinator().Execute(start, 0, ready, mission_ready, actions));
        assert(coordinator.GetMode() == MODE::SAFE && events.empty());

        journal_accepts = false;
        assert(!coordinator.Execute(start, 0, ready, mission_ready, actions));
        assert(coordinator.GetMode() == MODE::SAFE && events.size() == 1);
        journal_accepts = true;
    }

    // Cobre a fronteira monotônica e causas de término, com e sem diário.
    void CheckMission() {
        Coordinator coordinator(true);
        auto mission = mission_ready;
        assert(coordinator.Execute(COMMAND::START_MISSION, 0, ready, mission, actions));
        assert(!coordinator.Execute(COMMAND::START_MISSION, 12, ready, mission, actions));
        mission.utc_valid = false;
        coordinator.Service(599999, ready, mission, actions);
        assert(coordinator.GetMode() == MODE::MISSION);
        coordinator.Service(600000, ready, mission, actions);
        assert(coordinator.GetMode() == MODE::SAFE && events.back().cause == CAUSE::DEADLINE);
        assert(events.back().at_ms == 600000);

        assert(coordinator.Execute(COMMAND::START_MISSION, 600001, ready, mission_ready, actions));
        assert(coordinator.Execute(COMMAND::STOP_MISSION, 600002, ready, mission_ready, actions));
        assert(!coordinator.Execute(COMMAND::STOP_MISSION, 600003, ready, mission_ready, actions));
        assert(events.back().cause == CAUSE::STOP_MISSION);

        assert(coordinator.Execute(COMMAND::START_MISSION, 600004, ready, mission_ready, actions));
        mission = mission_ready;
        mission.payload_recent = false;
        coordinator.Service(600005, ready, mission, actions);
        assert(coordinator.GetMode() == MODE::SAFE && events.back().cause == CAUSE::PAYLOAD_LOST);
        assert(IsFault(events.back().cause));

        assert(coordinator.Execute(COMMAND::START_MISSION, 600006, ready, mission_ready, actions));
        mission = mission_ready;
        mission.journal_ready = false;
        journal_accepts = false;
        coordinator.Service(600007, ready, mission, actions);
        assert(coordinator.GetMode() == MODE::SAFE && events.back().cause == CAUSE::JOURNAL_LOST);
        const size_t after_loss = events.size();
        coordinator.Service(600008, ready, mission_ready, actions);
        assert(coordinator.GetMode() == MODE::SAFE && events.size() == after_loss);
        journal_accepts = true;
    }

    // Proteção ganha de falha e prazo. Recuperação requer TC e autorização.
    void CheckProtection() {
        Coordinator coordinator(true);
        auto subsystems = ready;
        auto mission = mission_ready;
        assert(coordinator.Execute(COMMAND::START_MISSION, 10, subsystems, mission, actions));
        subsystems.protection_active = true;
        mission.payload_recent = false;
        coordinator.Service(600010, subsystems, mission, actions);
        assert(coordinator.GetMode() == MODE::LOW_POWER && events.back().cause == CAUSE::PROTECTION);
        assert(!coordinator.Execute(COMMAND::ENTER_SAFE, 600011, subsystems, mission, actions));
        subsystems.recovery_allowed = true;
        assert(!coordinator.Execute(COMMAND::EXIT_SAFE, 600012, subsystems, mission, actions));
        subsystems.protection_active = false;
        subsystems.recovery_allowed = false;
        coordinator.Service(600013, subsystems, mission, actions);
        assert(!coordinator.Execute(COMMAND::EXIT_SAFE, 600013, subsystems, mission, actions));
        subsystems.recovery_allowed = true;
        assert(coordinator.Execute(COMMAND::EXIT_SAFE, 600014, subsystems, mission, actions));
        assert(coordinator.GetMode() == MODE::SAFE && events.back().cause == CAUSE::RECOVERY);
        assert(!coordinator.Execute(COMMAND::EXIT_SAFE, 600015, subsystems, mission, actions));
        const size_t after_recovery = events.size();
        assert(coordinator.Execute(COMMAND::ENTER_SAFE, 600016, subsystems, mission, actions));
        assert(events.size() == after_recovery);
    }

    // Uma parada incerta bloqueia novas ações até confirmação do ADCS.
    void CheckAdcs() {
        Coordinator coordinator(true);
        auto subsystems = ready;
        subsystems.adcs_allowed = false;
        assert(!coordinator.Execute(COMMAND::START_ADCS, 0, subsystems, mission_ready, actions));
        assert(starts == 0 && stops == 0);
        start_succeeds = false;
        assert(!coordinator.Execute(COMMAND::START_ADCS, 1, ready, mission_ready, actions));
        assert(coordinator.GetMode() == MODE::SAFE && events.back().cause == CAUSE::ADCS_START_FAILED);
        start_succeeds = true;
        assert(coordinator.Execute(COMMAND::START_ADCS, 2, ready, mission_ready, actions));
        subsystems = ready;
        subsystems.adcs_stopped = false;
        stop_succeeds = false;
        assert(!coordinator.Execute(COMMAND::STOP_ADCS, 3, subsystems, mission_ready, actions));
        assert(coordinator.GetMode() == MODE::DETUMBLING && events.back().cause == CAUSE::ADCS_STOP_FAILED);
        assert(!coordinator.Execute(COMMAND::START_MISSION, 4, subsystems, mission_ready, actions));
        subsystems.protection_active = true;
        coordinator.Service(5, subsystems, mission_ready, actions);
        assert(stops == 1 && coordinator.GetMode() == MODE::DETUMBLING);
        subsystems.adcs_stopped = true;
        coordinator.Service(6, subsystems, mission_ready, actions);
        assert(stops == 1 && coordinator.GetMode() == MODE::LOW_POWER && events.back().cause == CAUSE::PROTECTION);

        Coordinator automatic(true);
        stop_succeeds = true;
        assert(automatic.Execute(COMMAND::START_ADCS, 0, ready, mission_ready, actions));
        subsystems = ready;
        subsystems.adcs_done = true;
        automatic.Service(1, subsystems, mission_ready, actions);
        assert(stops == 2 && automatic.GetMode() == MODE::SAFE && events.back().cause == CAUSE::ADCS_DONE);
    }
}

int main() {
    CheckAdmission();
    CheckMission();
    CheckProtection();
    CheckAdcs();
    std::puts("ObcModesCheck passed");
}
