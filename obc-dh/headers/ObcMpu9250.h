/**
 * @file ObcMpu9250.h
 * @brief Aquisição de bancada do acelerômetro e do giroscópio MPU9250.
 */
#pragma once

#include <stddef.h> // Quantidade de eixos.
#include <stdint.h> // Contagens, endereço e relógio monotônico.
#include <Wire.h> // TwoWire entregue pelo executor I2C.
#include "I2cRecovery.h" // Resultados tipados e estado do barramento.

namespace obc_mpu9250 {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint8_t address = 0x68;
    constexpr size_t axis_count = 3;
    constexpr uint32_t acquisition_period_ms = 1000;
    constexpr uint64_t startup_wait_ms = 100;
    constexpr uint64_t first_sample_wait_ms = 200;
    constexpr uint64_t stale_after_ms = 3000;
    constexpr uint32_t queue_wait_ms = 100;
    constexpr uint8_t request_priority = 0;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    enum class ACTION { IDENTIFY_RESET, WAKE, CONFIGURE, READ, WAIT };

    /** @brief Identidade observada, inclusive quando difere do MPU9250 esperado. */
    struct IDENTITY {
        uint8_t value;
        bool available;
    };

    /** @brief Seis contagens provenientes da mesma leitura. */
    struct RAW_SAMPLE {
        int16_t acceleration[axis_count];
        int16_t angular_rate[axis_count];
    };

    /** @brief Produção por geração I2C. completed_ms é o timestamp quando valid=true. */
    struct STATE {
        ACTION next;
        uint64_t recovery_cycles;
        uint64_t completed_ms;
        RAW_SAMPLE sample;
        bool valid;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Confere WHO_AM_I antes de solicitar reset ao MPU9250. */
    i2c::OPERATION_RESULT IdentifyAndReset(
        TwoWire& wire,
        IDENTITY& identity
    );

    /** @brief Acorda o sensor e habilita todos os eixos, sem espera no callback. */
    i2c::OPERATION_RESULT Wake(
        TwoWire& wire
    );

    /** @brief Aplica faixas, filtros e divisor usados pelo ADCS. */
    i2c::OPERATION_RESULT Configure(
        TwoWire& wire
    );

    /** @brief Lê os eixos em bloco. Preserva a amostra de saída em falha. */
    i2c::OPERATION_RESULT ReadRaw(
        TwoWire& wire,
        RAW_SAMPLE& sample
    );

    /** @brief Converte uma contagem de acelerômetro da faixa ±2 g. */
    float ToAccelerationG(
        int16_t raw
    );

    /** @brief Converte uma contagem de giroscópio da faixa ±250 graus/s. */
    float ToAngularRateDps(
        int16_t raw
    );

    /** @brief Escolhe a próxima operação conforme a geração e o tempo de estabilização. */
    ACTION Next(
        const STATE& state,
        uint64_t now_ms,
        i2c::SERVICE_STATE service,
        uint64_t recovery_cycles
    );

    /** @brief Avança uma etapa concluída. Qualquer falha exige nova identificação. */
    void Complete(
        STATE& state,
        ACTION action,
        uint64_t completed_ms,
        uint64_t recovery_cycles,
        bool success,
        const RAW_SAMPLE& sample
    );

    /** @brief Revalida a amostra quanto ao serviço, geração e idade. */
    bool IsAvailable(
        const STATE& state,
        uint64_t now_ms,
        i2c::SERVICE_STATE service,
        uint64_t recovery_cycles
    );

#ifdef OBC_MPU9250_BENCH
    namespace esp32 {
        /** @brief Inicia o produtor de bancada após o boot I2C. */
        bool Init();

        /** @brief Publica a última amostra válida e o diagnóstico pela USB. */
        void ReportDiagnostics();
    }
#endif

} // namespace obc_mpu9250
