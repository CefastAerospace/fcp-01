/**
 * @file Wire.h
 * @brief Registradores NXP LM75A para teste do driver real, sem hardware.
 */
#pragma once

#include <cassert> // Invariantes das transações do teste.
#include <cstddef> // size_t compatível com Wire.
#include <cstdint> // Bytes e registradores.

class TwoWire {
public:
    bool present[128] = {};
    uint16_t registers[128][4] = {};
    uint32_t transactions[128] = {};
    uint32_t configuration_writes[128] = {};
    uint8_t select_error = 0;
    uint8_t write_error = 0;
    int fail_write_at = -1;
    int read_bytes = -1;
    int available_bytes = -1;
    int fail_read_at = -1;
    bool held = false;

    /** @brief Inicia transação exclusiva e limpa o buffer de transmissão. */
    void beginTransmission(
        uint8_t address
    ) {
        assert(!held && address < 128);
        address_value = address;
        length = 0;
        held = true;
    }

    /** @brief Injeta também falta de espaço ao escrever ponteiro/dado. */
    size_t write(
        uint8_t value
    ) {
        assert(held && length < 3);
        if (static_cast<int>(length) == fail_write_at) {
            fail_write_at = -1;
            return 0;
        }
        tx[length++] = value;
        return 1;
    }

    /** @brief Conf é um byte; limites possuem dois e permitem conferir preservação. */
    uint8_t endTransmission(
        bool stop
    ) {
        assert(held);
        ++transactions[address_value];
        if (!stop) {
            assert(length == 1 && tx[0] < 4);
            selected = tx[0];
            const uint8_t result = select_error ? select_error : present[address_value] ? 0 : 2;
            select_error = 0;
            return result;
        }
        held = false;
        const uint8_t result = write_error ? write_error : present[address_value] ? 0 : 2;
        write_error = 0;
        if (result == 0 && length >= 2) {
            assert(tx[0] < 4);
            if (tx[0] == 1) {
                assert(length == 2);
                registers[address_value][1] = tx[1];
                ++configuration_writes[address_value];
            } else {
                assert(length == 3 && tx[0] >= 2);
                registers[address_value][tx[0]] = static_cast<uint16_t>((tx[1] << 8) | tx[2]);
            }
        }
        return result;
    }

    /** @brief Leitura sempre termina com STOP, inclusive quando faltam bytes. */
    uint8_t requestFrom(
        int address,
        int count,
        int stop
    ) {
        assert(held && address == address_value && stop == 1);
        assert(count == (selected == 1 ? 1 : 2));
        held = false;
        ++transactions[address_value];
        const int reported = present[address_value] ? (read_bytes >= 0 ? read_bytes : count) : 0;
        readable = available_bytes >= 0 ? available_bytes : reported < count ? reported : count;
        assert(readable >= 0 && readable <= 2);
        read_bytes = available_bytes = -1;
        cursor = 0;
        const uint16_t raw = registers[address_value][selected];
        rx[0] = static_cast<uint8_t>(selected == 1 ? raw : raw >> 8);
        rx[1] = static_cast<uint8_t>(raw);
        return static_cast<uint8_t>(reported);
    }

    /** @brief Quantidade ainda não consumida, independente do retorno requestFrom. */
    int available(
    ) { return readable - cursor; }

    /** @brief Injeta retorno negativo mesmo após uma indicação de disponibilidade. */
    int read(
    ) {
        if (cursor >= readable) return -1;
        const int index = cursor++;
        if (index == fail_read_at) {
            fail_read_at = -1;
            return -1;
        }
        return rx[index];
    }

private:
    uint8_t address_value = 0;
    uint8_t selected = 0;
    uint8_t length = 0;
    uint8_t tx[3] = {};
    uint8_t rx[2] = {};
    int readable = 0;
    int cursor = 0;
};
