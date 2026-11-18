/**
 * @file Lm75a.h
 * @brief Aquisição NXP LM75A, referência provisória LM75A_4 de 10/07/2007.
 *
 * @details O chamador inicializa e arbitra Wire. Configure e ReadRaw devem
 * executar no proprietário do barramento; DecodeTemperature não faz I/O.
 * Leituras podem reconhecer OS em modo interrupt; OS não é usado na bancada.
 */
#pragma once

#include <cstddef> // size_t para quantidade de bytes.
#include <cstdint> // Inteiros dos registradores.
#include <Wire.h> // TwoWire fornecido pelo chamador.

namespace CEFAST {
namespace thermal {

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Resultado do driver; não transforma leitura curta em timeout. */
    enum class STATUS {
        NOT_ATTEMPTED, OK, INVALID_ADDRESS, BUFFER_ERROR,
        I2C_ERROR, READ_INCOMPLETE, OUT_OF_RANGE
    };

    /** @brief Etapa da última operação, independente do resultado. */
    enum class PHASE { NONE, SELECT_REGISTER, READ, WRITE, BUFFER };

    /** @brief Evidência Wire da última operação; zero não significa tentativa. */
    struct DIAGNOSTIC {
        PHASE phase;
        uint8_t reg;
        uint8_t wire_status;
        uint8_t received;
        STATUS status;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Habilita conversão normal em endereço de sete bits 0x48–0x4F.
     * @details Preserva Conf[4:1], zera os bits reservados e SHUTDOWN.
     * Não escreve Tos/Thyst, não espera conversão e não controla OS.
     */
    STATUS Configure(
        TwoWire& wire,
        uint8_t address,
        DIAGNOSTIC& diagnostic
    );

    /** @brief Lê Temp em dois bytes; preserva raw em qualquer falha. */
    STATUS ReadRaw(
        TwoWire& wire,
        uint8_t address,
        uint16_t& raw,
        DIAGNOSTIC& diagnostic
    );

    /**
     * @brief Converte os 11 bits significativos em °C, ignorando cinco LSB.
     * @return true para −55 a +125 °C; false preserva temperature_c.
     */
    bool DecodeTemperature(
        uint16_t raw,
        float& temperature_c
    );

} // namespace thermal
} // namespace CEFAST
