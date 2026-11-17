/**
 * @file ObcThermalEsp32.h
 * @brief Bancada LM75A: executor I2C real, publicação em RAM e diagnóstico USB.
 */
#pragma once

#include <Arduino.h> // Serial para diagnóstico USB.
#include "ObcThermal.h" // Estado e parâmetros térmicos provisórios.
#include "ObcEps.h" // Classificador portátil MapWireFailure já existente.
#include "Lm75a.h" // Operações e diagnóstico do driver térmico.
#include "I2cBus.h" // ExecuteChecked e geração de recuperação.
#include "ObcTimeEsp32.h" // Timer monotônico sem UTC fabricado.
#include <freertos/task.h> // Criação da tarefa e margem de pilha.
#include <cstdio> // snprintf em linhas completas.
#include <limits> // Contadores diagnósticos saturados.

namespace obc_thermal {
namespace esp32 {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint32_t worker_stack_bytes = 4096;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief temperature_c conserva histórico; available governa seu uso atual. */
    struct SAMPLE {
        uint8_t address;
        CHANNEL_STATE state;
        float temperature_c;
        CEFAST::thermal::STATUS last_status;
        CEFAST::thermal::DIAGNOSTIC bus;
        i2c::STATUS result;
        bool available;
        uint64_t age_ms;
    };

    /** @brief Cópia coerente dos cinco canais, sem localização física presumida. */
    struct SNAPSHOT {
        SAMPLE channels[channel_count];
    };

    /**
     * @brief Progresso de rodadas concluídas, independente da presença de sensores.
     * @details operations conta pedidos retornados, inclusive recusas sem callback.
     * active só confirma a primeira rodada publicada; não alimenta watchdog.
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

    /** @brief Inicia uma vez após I2C; falha preserva os serviços independentes. */
    bool Init(
    );
    /** @brief Copia sob lock curto e revalida disponibilidade sem I/O. */
    SNAPSHOT GetSnapshot(
    );
    /** @brief Copia progresso publicado pela tarefa, sem consultar sensores. */
    DIAGNOSTICS GetDiagnostics(
    );
    /** @brief Emite no máximo uma rodada/s; não publica TM nem grava SD. */
    void ReportDiagnostics(
    );

} // namespace esp32
} // namespace obc_thermal
