/**
 * @file ObcThermal.h
 * @brief Cadência e validade dos cinco canais LM75A de bancada, sem I/O.
 */
#pragma once

#include <cstddef> // size_t dos cinco canais.
#include <cstdint> // Tempos monotônicos e gerações I2C.
#include <limits> // Saturação do próximo prazo.

namespace obc_thermal {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr size_t channel_count = 5;
    constexpr uint8_t addresses[channel_count] = {0x48, 0x49, 0x4A, 0x4B, 0x4C};
    constexpr uint64_t acquisition_period_ms = 1000;
    constexpr uint64_t first_sample_delay_ms = 1000;
    constexpr uint64_t retry_period_ms = 5000;
    constexpr uint64_t stale_after_ms = 3000;
    constexpr uint32_t queue_wait_ms = 100;
    constexpr uint8_t request_priority = 0;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    enum class ACTION { WAIT, CONFIGURE, READ };

    /** @brief Histórico por canal; has_valid distingue tempo/temperatura zero de ausência. */
    struct CHANNEL_STATE {
        bool configured;
        bool attempted;
        bool has_valid;
        bool latest_valid;
        uint64_t attempted_ms;
        uint64_t last_valid_ms;
        uint64_t next_due_ms;
        uint64_t recovery_cycles;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Guarda usada dentro do executor; READ não usa configuração de outra geração. */
    bool CanExecute(
        ACTION action,
        uint64_t expected_cycles,
        uint64_t current_cycles,
        bool service_ready
    );

    /** @brief Revalida a cópia também contra recuperação posterior à publicação. */
    bool IsAvailable(
        const CHANNEL_STATE& state,
        uint64_t now_ms,
        bool service_ready,
        uint64_t recovery_cycles
    );

    // ─── CLASSES ──────────────────────────────────────────────────────

    /**
     * @brief Estado acessado sequencialmente pela tarefa térmica.
     * @details O LM75A não expõe conversion-ready. A espera inicial é
     * provisória e não comprova o tempo máximo de conversão físico.
     */
    class Channel {
    public:
        /** @brief Escolhe a operação e invalida a configuração após recuperação. */
        ACTION Next(
            uint64_t now_ms,
            bool service_ready,
            uint64_t recovery_cycles
        );
        /**
         * @brief Registra tentativa concluída, inclusive recusa anterior ao callback.
         * @details configured permanece true em dado fora da faixa com I/O OK.
         * Falha de transporte/configuração exige nova configuração após o intervalo offline.
         */
        void Complete(
            ACTION action,
            uint64_t now_ms,
            uint64_t recovery_cycles,
            bool valid,
            bool configured
        );
        /** @brief Copia estado sem ponteiros ou transações. */
        CHANNEL_STATE GetState(
        ) const;

    private:
        CHANNEL_STATE state = {};
    };

} // namespace obc_thermal
