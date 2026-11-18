/**
 * @file ObcEpsEsp32.h
 * @brief Aquisição INA219 isolada, com driver exclusivo e cópias pequenas.
 */
#pragma once

#include "ObcEps.h" // Cadência, validade e classificação portáteis.
#include "EPS_INA219.h" // Driver EPS e seus resultados, sem duplicação de cálculos.
#include "I2cBus.h" // Executor exclusivo, estado e recuperação existentes.
#include "ObcTimeEsp32.h" // Instantes monotônicos u64, sem fabricar UTC.
#include <freertos/task.h> // Tarefa proprietária e margem de pilha.
#include <cstdio> // snprintf em linhas completas de diagnóstico.
#include <limits> // Contadores saturados.

namespace obc_eps {
namespace esp32 {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint32_t worker_stack_bytes = 4096;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief reading conserva a última amostra válida; available governa seu uso atual. */
    struct SAMPLE {
        CHANNEL_STATE state;
        CEFAST::eps::INAReading reading;
        CEFAST::eps::INAStatus last_status;
        CEFAST::eps::I2C_DIAGNOSTIC bus;
        i2c::STATUS result;
        bool available;
        uint64_t age_ms;
    };

    /** @brief Cópia coerente; valores sem has_valid não representam medições. */
    struct SNAPSHOT {
        SAMPLE channels[channel_count];
    };

    /**
     * @brief Progresso independente da presença de sensores; não alimenta watchdog.
     * @details active confirma a primeira rodada publicada, não apenas a criação
     * da tarefa. Antes dela, o diagnóstico informa starting sem emitir amostras.
     */
    struct DIAGNOSTICS {
        bool attempted;
        bool active;
        uint64_t progress_ms;
        uint64_t operations;
        uint64_t failures;
        uint32_t stack_free_bytes;
        i2c::SERVICE_STATE service;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Publica metadados e inicia uma vez após I2cBus::Init; falha conserva os outros serviços. */
    bool Init(
    );
    /** @brief Copia sob proteção curta e calcula idade/disponibilidade fora do lock. */
    SNAPSHOT GetSnapshot(
    );
    /** @brief Copia progresso e pilha publicados pela tarefa, sem I/O. */
    DIAGNOSTICS GetDiagnostics(
    );
    /** @brief Emite no máximo uma rodada/s por USB; não altera TC/TM ou SD. */
    void ReportDiagnostics(
    );

} // namespace esp32
} // namespace obc_eps
