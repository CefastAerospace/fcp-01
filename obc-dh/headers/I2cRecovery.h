/**
 * @file I2cRecovery.h
 * @brief Classificação e recuperação I2C portáteis, com orçamento cooperativo.
 */
#pragma once

#include <stdint.h> // Inteiros de duração e contadores.

namespace i2c {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint8_t recovery_attempts = 2;
    constexpr uint32_t recovery_budget_us = 100000;
    constexpr uint32_t recovery_interval_us = 10000;
    constexpr uint8_t recovery_clock_pulses = 9;
    constexpr uint32_t recovery_phase_us = 5;
    constexpr uint32_t line_release_timeout_us = 1000;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Causa informada pelo callback; leitura curta conserva causa indeterminada. */
    enum class OPERATION_RESULT {
        OK,
        DEVICE_NACK,
        TRANSFER_TIMEOUT,
        BUS_ERROR,
        READ_INCOMPLETE,
        OPERATION_FAILED
    };

    /** @brief Disponibilidade do serviço, independente da presença dos sensores. */
    enum class SERVICE_STATE { UNINITIALIZED, READY, RECOVERING, UNAVAILABLE };

    /** @brief Causas de início e de falha da recuperação, sem inferir erro do sensor. */
    enum class RECOVERY_CAUSE {
        NONE,
        REQUESTED,
        TRANSFER_TIMEOUT,
        BUS_ERROR,
        SDA_LOW,
        SCL_LOW,
        DEINIT_FAILED,
        INIT_FAILED,
        BUDGET_EXHAUSTED
    };

    /**
     * @brief Operações exclusivas do executor, substituíveis na bancada.
     * @details prepare libera ambas as linhas em dreno aberto após stop bem-sucedido.
     * set_* false aterra a linha, true libera. now_us usa diferença u32 com wrap.
     */
    struct RECOVERY_IO {
        bool (*stop)(void* context);
        bool (*start)(void* context);
        void (*prepare)(void* context);
        void (*set_sda)(void* context, bool released);
        void (*set_scl)(void* context, bool released);
        bool (*read_sda)(void* context);
        bool (*read_scl)(void* context);
        uint32_t (*now_us)(void* context);
        void (*delay_us)(void* context, uint32_t duration_us);
        void* context;
    };

    /** @brief Resultado de um ciclo, com duração medida inclusive nas chamadas do driver. */
    struct RECOVERY_RESULT {
        bool recovered;
        uint8_t attempts;
        uint8_t pulses;
        uint32_t elapsed_us;
        RECOVERY_CAUSE failure;
        bool sda_high;
        bool scl_high;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Escolhe recuperação por linha presa ou falha explicitamente classificada. */
    RECOVERY_CAUSE GetRecoveryCause(
        OPERATION_RESULT result,
        bool sda_high,
        bool scl_high
    );

    /**
     * @brief Executa até duas tentativas, libera SDA com até nove clocks e gera STOP.
     * @details O orçamento é verificado entre chamadas. Um driver bloqueado exige
     * supervisão externa da INT-008. O chamador fornece callbacks válidos e exclusivos.
     */
    RECOVERY_RESULT RecoverBus(
        const RECOVERY_IO& io
    );

} // namespace i2c
