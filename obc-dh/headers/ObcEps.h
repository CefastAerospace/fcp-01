/**
 * @file ObcEps.h
 * @brief Cadência e validade portáteis da aquisição INA219 de bancada.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include "I2cRecovery.h" // OPERATION_RESULT sem dependência de Wire/FreeRTOS.

namespace obc_eps {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr size_t channel_count = 3;
    constexpr uint64_t acquisition_period_ms = 1000;
    constexpr uint64_t retry_period_ms = 5000;
    constexpr uint64_t stale_after_ms = 3000;
    constexpr uint64_t conversion_wait_ms = 2; // Arredonda 1064 us para cima.
    constexpr uint32_t queue_wait_ms = 100;
    constexpr uint8_t request_priority = 0;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    enum class ACTION { WAIT, CONFIGURE, READ };

    /** @brief Histórico de validade; zero é instante válido quando has_valid=true. */
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

    /** @brief Classifica somente a informação exposta por Wire; curto não significa timeout. */
    i2c::OPERATION_RESULT MapWireFailure(
        bool failed,
        bool buffer_failure,
        bool short_read,
        uint8_t status
    );

    /** @brief Retorna disponibilidade atual sem transformar o histórico em leitura nova. */
    bool IsAvailable(
        const CHANNEL_STATE& state,
        uint64_t now_ms
    );

    // ─── CLASSES ──────────────────────────────────────────────────────

    /** @brief Estado de um canal, acessado sequencialmente pela tarefa proprietária. */
    class Channel {
    public:
        /** @brief Escolhe trabalho sem I/O; recuperação invalida configuração/amostra corrente. */
        ACTION Next(
            uint64_t now_ms,
            bool service_ready,
            uint64_t recovery_cycles
        );
        /** @brief Registra retorno, inclusive falha de admissão sem callback executado. */
        void Complete(
            ACTION action,
            uint64_t now_ms,
            uint64_t recovery_cycles,
            bool valid,
            bool online
        );
        /** @brief Copia o estado; não contém ponteiros para o driver. */
        CHANNEL_STATE GetState(
        ) const;

    private:
        CHANNEL_STATE state_ = {};
    };

} // namespace obc_eps
