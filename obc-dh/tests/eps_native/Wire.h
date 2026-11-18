/** @file Wire.h
 * @brief Registradores INA219 controlados pelo teste; não simula temporização física.
 */
#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>

class TwoWire {
public:
    bool present[128] = {};
    uint16_t registers[128][6] = {};
    uint8_t stop_error = 0;
    uint8_t select_error = 0;
    uint8_t read_bytes = 2;
    bool fail_write = false;
    bool held = false;
    uint32_t transactions[128] = {};

    void beginTransmission(
        uint8_t address
    ) {
        assert(!held);
        assert(address < 128);
        address_ = address;
        length_ = 0;
        held = true;
    }

    size_t write(
        uint8_t value
    ) {
        assert(held);
        if (fail_write) { fail_write = false; return 0; }
        assert(length_ < 3);
        tx_[length_++] = value;
        return 1;
    }

    uint8_t endTransmission(
        bool stop
    ) {
        assert(held);
        if (!stop) {
            selected_ = tx_[0];
            const uint8_t result = select_error;
            select_error = 0;
            return result;
        }
        held = false;
        ++transactions[address_];
        const uint8_t result = stop_error ? stop_error : present[address_] ? 0 : 2;
        stop_error = 0;
        if (!result && length_ == 3) {
            assert(tx_[0] < 6);
            registers[address_][tx_[0]] = static_cast<uint16_t>((tx_[1] << 8) | tx_[2]);
        }
        return result;
    }

    uint8_t requestFrom(
        int address,
        int count,
        int stop
    ) {
        assert(held && address == address_ && count == 2 && stop == 1);
        assert(selected_ < 6);
        held = false;
        ++transactions[address_];
        remaining_ = present[address_] ? read_bytes : 0;
        read_bytes = 2;
        cursor_ = 0;
        rx_ = registers[address_][selected_];
        return remaining_;
    }

    int available(
    ) { return remaining_; }

    int read(
    ) {
        if (!remaining_) return -1;
        --remaining_;
        return cursor_++ == 0 ? rx_ >> 8 : rx_ & 0xff;
    }

private:
    uint8_t address_ = 0;
    uint8_t length_ = 0;
    uint8_t selected_ = 0;
    uint8_t tx_[3] = {};
    uint8_t remaining_ = 0;
    uint8_t cursor_ = 0;
    uint16_t rx_ = 0;
};
