/**
 * @file ObcHousekeepingBench.h
 * @brief Recorte INT-009 exclusivo: aquisição conjunta ou fixtures, TM e captura SD simulada.
 */
#pragma once

#ifdef OBC_HOUSEKEEPING_BENCH
#include "ObcHousekeeping.h" // Mesmo mapeador portátil exercitado no computador.
#include "ObcEpsEsp32.h" // Cópias dos três INA219, sem transações no consumidor.
#include "ObcThermalEsp32.h" // Cópias dos cinco LM75A e disponibilidade por canal.
#include "ObcRecordCodec.h" // Assinaturas/tamanhos de BOOT, referência e TM v3.
#include <Arduino.h> // Serial em uma escrita por linha.
#include <esp_heap_caps.h> // Heap interno, maior bloco e callback de alocação recusada.
#include <cstdio> // snprintf dos diagnósticos exclusivos.
#include <cstring> // memcmp das assinaturas canônicas.

namespace obc_housekeeping {
namespace bench {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    /** @brief Somente I2C ainda pede pilha ao heap; EPS/Térmico já ocupam RAM estática. */
    constexpr size_t pending_stack_bytes = i2c::worker_stack_bytes;

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Inicia produtores independentes, ou anuncia fixtures sem iniciar aquisição. */
    void Init(
    );
    /** @brief Lê cópias dos proprietários; VIN− provisório, sem atribuir posições térmicas. */
    READINGS Read(
        uint64_t sampled_ms
    );
    /** @brief Publica saúde dos produtores e memória, sem alterar política do supervisor. */
    void ReportDiagnostics(
    );
    /** @brief Mede cada criação no setup; callback só copia metadados, sem I/O ou alocação. */
    void TraceTaskCreation(
        const char* task,
        bool after,
        size_t stack_bytes,
        int result
    );
    /** @brief Captura só BOOT/referência/TM aceitos pelo backend simulado, no escritor. */
    void CaptureRecord(
        const uint8_t* bytes,
        size_t size
    );

} // namespace bench
} // namespace obc_housekeeping
#endif
