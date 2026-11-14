/**
 * @file ObcI2cHealth.h
 * @brief Diagnóstico portátil de callback, recuperação e disponibilidade I2C.
 */
#pragma once

#include "I2cRecovery.h" // SERVICE_STATE, sem depender de Wire ou FreeRTOS.
#include "ObcRecordCodec.h" // EVENT/FAULT existentes, com identificação temporal.
#include <cstdio> // snprintf para descrição limitada do registro.

namespace obc_i2c_health {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint32_t callback_limit_ms = 100;
    constexpr uint32_t recovery_limit_ms = 200;
    constexpr uint32_t evaluation_period_ms = 100;
    constexpr uint16_t event_code = 259;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    enum class STATE { INACTIVE, OK, SUSPECT, UNAVAILABLE };
    enum class ACTIVITY { INACTIVE, IDLE, CALLBACK, RECOVERY };

    /** @brief Cópia coerente publicada pelo executor; todos os tempos usam millis u32. */
    struct SAMPLE {
        i2c::SERVICE_STATE service;
        bool callback_running;
        uint32_t callback_started_ms;
        uint32_t recovery_started_ms;
        uint32_t sampled_ms;
    };

    /** @brief Disponibilidade e progresso são independentes: recuperar não significa pronto. */
    struct REPORT {
        i2c::SERVICE_STATE service;
        STATE state;
        ACTIVITY activity;
        uint32_t started_ms;
        uint32_t age_ms;
        uint32_t limit_ms;
        uint32_t sampled_ms;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    const char* ServiceName(
        i2c::SERVICE_STATE service
    );
    const char* StateName(
        STATE state
    );
    const char* ActivityName(
        ACTIVITY activity
    );

    /** @brief Monta EVENT/info ou FAULT/error OBC 259; não grava nem garante persistência. */
    bool MakeEvent(
        const REPORT& report,
        const obc_time::EVENT_STAMP& stamp,
        obc_record::RECORD& record
    );

    // ─── CLASSES ──────────────────────────────────────────────────────

    /** @brief Avaliador sequencial; não acessa o controlador nem solicita recuperação. */
    class Monitor {
    public:
        /**
         * @brief Publica relatório e retorna mudança de serviço ou saúde.
         * @details Primeiro estado normal não gera evento; suspeita/indisponibilidade
         * inicial gera. IDLE não vence. Idade modular u32 exige menos de um wrap
         * entre início e detecção; suspeita detectada permanece até o fim da atividade.
         * Serviço inválido retorna false sem alterar relatório/histórico.
         */
        bool Evaluate(
            const SAMPLE& sample,
            REPORT& report
        );
    private:
        bool sampled_ = false;
        REPORT previous_ = {};
    };

} // namespace obc_i2c_health
