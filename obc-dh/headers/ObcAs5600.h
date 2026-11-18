/**
 * @file ObcAs5600.h
 * @brief Aquisição bruta e disponibilidade de bancada do encoder AS5600.
 */
#pragma once

#include <stdint.h> // Tipos da contagem e do relógio monotônico.
#include <Wire.h> // TwoWire entregue pelo executor I2C.
#include "I2cRecovery.h" // Resultado tipado e estado do serviço.

namespace obc_as5600 {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint8_t address = 0x36;
    constexpr uint32_t acquisition_period_ms = 1000;
    constexpr uint64_t stale_after_ms = 3000;
    constexpr uint32_t queue_wait_ms = 100;
    constexpr uint8_t request_priority = 0;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Amostra identificada pela geração do I2C que fez a leitura. */
    struct SAMPLE {
        uint64_t sampled_ms;
        uint64_t recovery_cycles;
        uint16_t raw_count;
        bool valid;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Lê RAW ANGLE no callback do executor. Preserva raw_count em falha. */
    i2c::OPERATION_RESULT ReadRaw(
        TwoWire& wire,
        uint16_t& raw_count
    );

    /** @brief Exige leitura válida, serviço pronto, geração atual e idade aceitável. */
    bool IsAvailable(
        const SAMPLE& sample,
        uint64_t now_ms,
        i2c::SERVICE_STATE service,
        uint64_t recovery_cycles
    );

#ifdef OBC_AS5600_BENCH
    namespace esp32 {
        /** @brief Inicia a aquisição de bancada após o boot do executor I2C. */
        bool Init();

        /** @brief Publica uma amostra por segundo pela USB. */
        void ReportDiagnostics();
    }
#endif

} // namespace obc_as5600
