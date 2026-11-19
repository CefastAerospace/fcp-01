/** @file ObcModes.h
 * @brief Coordenação portátil de modos e da janela ADS-B no boot atual.
 */
#pragma once

#include "models/ObcData.h" // MODE usado pela telemetria e pelo diário.

namespace obc_modes {

    constexpr uint64_t mission_duration_ms = 600000;
    constexpr uint16_t event_code = 267;

    enum class COMMAND { START_MISSION, STOP_MISSION, START_ADCS, STOP_ADCS, ENTER_SAFE, EXIT_SAFE };

    enum class CAUSE {
        START_MISSION, STOP_MISSION, DEADLINE, PAYLOAD_LOST, JOURNAL_LOST,
        START_ADCS, STOP_ADCS, ADCS_DONE, ENTER_SAFE, PROTECTION, RECOVERY,
        ADCS_START_FAILED, ADCS_STOP_FAILED
    };

    /** @brief Estado observado do boot, do ADCS e da proteção aprovada. */
    struct SUBSYSTEM_STATUS {
        bool boot_ready;
        bool adcs_allowed;
        bool adcs_done;
        bool adcs_stopped;
        bool protection_active;
        bool recovery_allowed;
    };

    /** @brief Evidência local exigida para abrir e manter a janela ADS-B. */
    struct MISSION_STATUS {
        bool flow_ready;
        bool payload_recent;
        bool journal_ready;
        bool utc_valid;
    };

    /** @brief Mudança efetiva ou falha de ação, com instante monotônico. */
    struct EVENT {
        obc_data::MODE from;
        obc_data::MODE to;
        CAUSE cause;
        uint64_t at_ms;
    };

    /** @brief Ações locais com resultado imediato, sem espera por I/O no loop OBC. */
    struct ACTIONS {
        /** @brief Confirma início. Falha preserva o motor desabilitado. */
        bool (*start_adcs)();
        /** @brief Confirma parada e desabilitação. Uma parada pendente é observada no status. */
        bool (*stop_adcs)();
        /** @brief Submete evento ao diário. true confirma admissão, sem garantir escrita. */
        bool (*record)(const EVENT& event);
    };

    /** @brief Indica se a causa deve ser registrada como FAULT. */
    bool IsFault(
        CAUSE cause
    );

    /** @brief Identificador da causa para o diário e o diagnóstico serial. */
    const char* CauseName(
        CAUSE cause
    );

    /** @brief Um proprietário sequencial, sem estado durável ou periféricos próprios. */
    class Coordinator {
    public:
        /** @brief Inicia em BENCH ou SAFE conforme a configuração explícita de boot. */
        explicit Coordinator(
            bool operational = false
        );

        /** @brief Aplica proteção, confirma parada pendente e encerra janela por falha ou prazo. */
        void Service(
            uint64_t now_ms,
            const SUBSYSTEM_STATUS& subsystems,
            const MISSION_STATUS& mission,
            const ACTIONS& actions
        );

        /** @brief Executa um comando validado após Service com a mesma amostra. */
        bool Execute(
            COMMAND command,
            uint64_t now_ms,
            const SUBSYSTEM_STATUS& subsystems,
            const MISSION_STATUS& mission,
            const ACTIONS& actions
        );

        /** @brief Modo efetivo publicável na TM. */
        obc_data::MODE GetMode() const;

        /** @brief Indica LOW-POWER efetivo ou parada ADCS ainda pendente por proteção. */
        bool IsProtectionInEffect() const;

    private:
        void Transition(
            const EVENT& event,
            const ACTIONS& actions
        );
        bool Enter(
            obc_data::MODE target,
            CAUSE cause,
            uint64_t now_ms,
            const ACTIONS& actions
        );

        obc_data::MODE mode_;
        uint64_t mission_started_ms_ = 0;
        bool stop_pending_ = false;
        obc_data::MODE stop_target_ = obc_data::MODE::SAFE;
        CAUSE stop_cause_ = CAUSE::STOP_ADCS;
    };

} // namespace obc_modes
