/**
 * @file Wire.h
 * @brief Simula somente a seleção e a leitura do RAW ANGLE para a regressão do AS5600.
 */
#pragma once

#include <cassert> // Invariantes da sequência START/STOP.
#include <stddef.h> // Tamanho da leitura.
#include <stdint.h> // Endereço e bytes do registrador.

class TwoWire {
public:
    uint8_t high = 0;
    uint8_t low = 0;
    uint8_t select_status = 0;
    bool fail_buffer = false;
    size_t reported_bytes = 2;
    int readable_bytes = 2;
    int fail_read_at = -1;
    bool held = false;
    unsigned read_requests = 0;
    unsigned stops = 0;

    /** @brief Exige o endereço de sete bits do encoder sem transação pendente. */
    void beginTransmission(
        uint8_t address
    ) {
        assert(address == 0x36 && !held);
        held = true;
        pointer_written_ = false;
        repeated_ = false;
    }

    /** @brief Aceita apenas a seleção do registrador, sem escrita de dados. */
    size_t write(
        uint8_t value
    ) {
        assert(held && !pointer_written_ && value == 0x0C);
        if (fail_buffer) return 0;
        pointer_written_ = true;
        return 1;
    }

    /** @brief Fecha uma operação abortada ou prepara o repeated START. */
    uint8_t endTransmission(
        bool stop
    ) {
        assert(held);
        if (stop) {
            held = false;
            ++stops;
            return 0;
        }
        assert(pointer_written_ && !repeated_);
        repeated_ = true;
        return select_status;
    }

    /** @brief Fecha a leitura com STOP e permite injetar quantidades curtas. */
    size_t requestFrom(
        uint8_t address,
        size_t length,
        bool stop
    ) {
        assert(address == 0x36 && length == 2 && held && repeated_ && stop && select_status == 0);
        held = false;
        cursor_ = 0;
        ++read_requests;
        ++stops;
        return reported_bytes;
    }

    int available() const { return readable_bytes - cursor_; }

    /** @brief Retorna os bytes em ordem ou injeta falha apesar de available. */
    int read(
    ) {
        if (available() == 0) return -1;
        const int index = cursor_++;
        if (index == fail_read_at) return -1;
        return index == 0 ? high : low;
    }

private:
    bool pointer_written_ = false;
    bool repeated_ = false;
    int cursor_ = 2;
};
