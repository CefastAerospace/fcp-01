/**
 * @file Wire.h
 * @brief Simula transações de um comando ou dois bytes para o par BH1750.
 */
#pragma once

#include <cassert> // Invariantes de endereço, transação e STOP.
#include <stddef.h> // Tamanho dos pedidos.
#include <stdint.h> // Bytes, endereços e contagem bruta.

class TwoWire {
public:
    struct WRITE {
        uint8_t address;
        uint8_t command;
    };

    WRITE writes[16] = {};
    size_t write_count = 0;
    unsigned read_requests = 0;
    unsigned stops = 0;
    uint8_t last_read_address = 0;
    int fail_command_at = -1;
    int fail_buffer_at = -1;
    uint8_t wire_status = 2;
    size_t reported_bytes = 2;
    int readable_bytes = 2;
    int fail_read_at = -1;
    uint16_t raw = 0;
    bool held = false;

    /** @brief Somente os dois endereços previstos participam da bancada. */
    void beginTransmission(
        uint8_t address
    ) {
        assert(!held && (address == 0x23 || address == 0x5C));
        address_ = address;
        buffered_ = false;
        held = true;
    }

    /** @brief Aceita no máximo um comando por transação. */
    size_t write(
        uint8_t command
    ) {
        assert(held && !buffered_);
        if (static_cast<int>(write_count) == fail_buffer_at) return 0;
        command_ = command;
        buffered_ = true;
        return 1;
    }

    /** @brief Exige STOP mesmo após falha do buffer ou NACK do dispositivo. */
    uint8_t endTransmission(
        bool stop
    ) {
        assert(held && stop);
        held = false;
        ++stops;
        if (!buffered_) return 0;
        assert(write_count < sizeof(writes) / sizeof(writes[0]));
        const size_t index = write_count++;
        writes[index] = {address_, command_};
        return static_cast<int>(index) == fail_command_at ? wire_status : 0;
    }

    /** @brief Lê dois bytes diretamente, sem seleção prévia de registrador. */
    size_t requestFrom(
        uint8_t address,
        size_t count,
        bool stop
    ) {
        assert(!held && (address == 0x23 || address == 0x5C) && count == 2 && stop);
        ++read_requests;
        ++stops;
        last_read_address = address;
        cursor_ = 0;
        return reported_bytes;
    }

    /** @brief Retorna -1 quando a quantidade real fica abaixo da declarada. */
    int read(
    ) {
        if (cursor_ >= readable_bytes) return -1;
        const int index = cursor_++;
        if (index == fail_read_at) return -1;
        return index == 0 ? raw >> 8 : raw & 0xFF;
    }

private:
    uint8_t address_ = 0;
    uint8_t command_ = 0;
    int cursor_ = 2;
    bool buffered_ = false;
};
