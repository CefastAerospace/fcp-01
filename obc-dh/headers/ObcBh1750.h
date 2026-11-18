/**
 * @file ObcBh1750.h
 * @brief Aquisição e validade independentes dos dois canais BH1750 de bancada.
 */
#pragma once

#include <stddef.h> // Capacidade do conjunto de canais.
#include <stdint.h> // Endereços, contagem bruta e tempo monotônico.
#include <Wire.h> // TwoWire entregue pelo executor I2C.
#include "I2cRecovery.h" // Estado do serviço e classificação de falhas Wire.

namespace obc_bh1750 {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint8_t addresses[] = {0x23, 0x5C};
    constexpr size_t channel_count = sizeof(addresses) / sizeof(addresses[0]);
    constexpr uint32_t acquisition_period_ms = 1000;
    constexpr uint64_t conversion_wait_ms = 200;
    constexpr uint64_t stale_after_ms = 3000;
    constexpr uint32_t queue_wait_ms = 100;
    constexpr uint8_t request_priority = 0;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    enum class ACTION { WAIT, CONFIGURE, READ };

    /** @brief Estado de um canal na geração I2C que executou seu último pedido. */
    struct CHANNEL {
        uint64_t recovery_cycles;
        uint64_t configured_ms;
        uint64_t sampled_ms;
        uint16_t raw;
        bool configured;
        bool valid;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Programa MTreg nominal e alta resolução contínua dentro do callback I2C. */
    i2c::OPERATION_RESULT Configure(
        TwoWire& wire,
        uint8_t address
    );

    /** @brief Lê a contagem MSB primeiro. Preserva raw em falha. */
    i2c::OPERATION_RESULT ReadRaw(
        TwoWire& wire,
        uint8_t address,
        uint16_t& raw
    );

    /** @brief Converte a contagem com MTreg nominal em lux. */
    float ToLux(
        uint16_t raw
    );

    /** @brief Escolhe o pedido de uma rodada sem acessar o barramento. */
    ACTION Next(
        const CHANNEL& channel,
        uint64_t now_ms,
        i2c::SERVICE_STATE service,
        uint64_t recovery_cycles
    );

    /** @brief Aplica a conclusão de CONFIGURE ou READ ao canal. Falha exige reconfiguração. */
    void Complete(
        CHANNEL& channel,
        ACTION action,
        uint64_t completed_ms,
        uint64_t recovery_cycles,
        bool success,
        uint16_t raw
    );

    /** @brief Revalida a amostra copiada contra o serviço, a geração e o relógio atuais. */
    bool IsAvailable(
        const CHANNEL& channel,
        uint64_t now_ms,
        i2c::SERVICE_STATE service,
        uint64_t recovery_cycles
    );

#ifdef OBC_BH1750_BENCH
    namespace esp32 {
        /** @brief Inicia o produtor dos dois canais depois do boot I2C. */
        bool Init();

        /** @brief Informa a amostra de cada canal pela USB. */
        void ReportDiagnostics();
    }
#endif

} // namespace obc_bh1750
