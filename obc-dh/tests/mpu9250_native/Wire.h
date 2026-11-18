/**
 * @file Wire.h
 * @brief Simula os registradores e a sequência START/STOP usada pelo MPU9250.
 */
#pragma once

#include <cassert> // Invariantes de cada transferência.
#include <stddef.h> // Tamanhos de buffers.
#include <stdint.h> // Bytes e endereços.

class TwoWire {
public:
    struct WRITE {
        uint8_t reg;
        uint8_t value;
    };

    uint8_t registers[128] = {};
    WRITE writes[16] = {};
    size_t write_count = 0;
    size_t read_requests = 0;
    unsigned stops = 0;
    uint8_t select_status = 0;
    uint8_t write_status = 0;
    int fail_buffer_at = -1;
    int fail_read_at = -1;
    size_t read_limit = 14;
    bool held = false;

    /** @brief Aceita apenas o MPU9250 com AD0 em GND. */
    void beginTransmission(
        uint8_t address
    ) {
        assert(address == 0x68 && !held);
        held = true;
        length_ = 0;
    }

    /** @brief Permite injetar falha em qualquer byte de seleção ou escrita. */
    size_t write(
        uint8_t value
    ) {
        assert(held && length_ < sizeof(tx_));
        if (static_cast<int>(length_) == fail_buffer_at) return 0;
        tx_[length_++] = value;
        return 1;
    }

    /** @brief Seleciona um registrador para leitura ou grava bytes consecutivos. */
    uint8_t endTransmission(
        bool stop
    ) {
        assert(held);
        if (!stop) {
            assert(length_ == 1);
            selected_ = tx_[0];
            return select_status;
        }
        held = false;
        ++stops;
        if (write_status != 0) return write_status;
        for (size_t index = 1; index < length_; ++index) {
            const size_t reg = tx_[0] + index - 1;
            assert(reg < sizeof(registers) && write_count < sizeof(writes) / sizeof(writes[0]));
            if (reg == 0x6B && tx_[index] == 0x80) assert(identity_read_);
            registers[reg] = tx_[index];
            writes[write_count++] = {static_cast<uint8_t>(reg), tx_[index]};
        }
        return 0;
    }

    /** @brief Exige repeated START com um registrador conhecido e STOP ao concluir. */
    size_t requestFrom(
        uint8_t address,
        size_t count,
        bool stop
    ) {
        assert(address == 0x68 && held && stop && select_status == 0);
        assert((selected_ == 0x75 && count == 1) || (selected_ == 0x3B && count == 14));
        held = false;
        ++stops;
        ++read_requests;
        cursor_ = 0;
        readable_ = read_limit < count ? read_limit : count;
        return readable_;
    }

    /** @brief Leitura curta ou byte inválido retorna -1 sem fabricar dado. */
    int read(
    ) {
        if (cursor_ == readable_) return -1;
        const size_t index = cursor_++;
        if (static_cast<int>(index) == fail_read_at) return -1;
        if (selected_ == 0x75) identity_read_ = true;
        return registers[selected_ + index];
    }

private:
    uint8_t tx_[5] = {};
    uint8_t selected_ = 0;
    size_t length_ = 0;
    size_t cursor_ = 0;
    size_t readable_ = 0;
    bool identity_read_ = false;
};
