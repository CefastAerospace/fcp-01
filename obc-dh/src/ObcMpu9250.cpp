#include "ObcMpu9250.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcMpu9250.h".
 */

namespace obc_mpu9250 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint8_t who_am_i_register = 0x75;
    constexpr uint8_t expected_identity = 0x71;
    constexpr uint8_t power_management_register = 0x6B;
    constexpr uint8_t reset = 0x80;
    constexpr uint8_t power_configuration[] = {0x01, 0x00};
    constexpr uint8_t config_register = 0x1A;
    constexpr uint8_t range_and_filters[] = {0x06, 0x00, 0x00, 0x06};
    constexpr uint8_t sample_divider_register = 0x19;
    constexpr uint8_t sample_divider = 99;
    constexpr uint8_t first_output_register = 0x3B;
    constexpr size_t output_bytes = 14;
    constexpr size_t gyro_offset = 8;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Encerra com STOP mesmo quando algum byte não cabe no buffer Wire.
    i2c::OPERATION_RESULT WriteRegisters(
        TwoWire& wire,
        uint8_t reg,
        const uint8_t* values,
        size_t length
    ) {
        wire.beginTransmission(address);
        bool buffered = wire.write(reg) == 1;
        for (size_t index = 0; buffered && index < length; ++index) {
            buffered = wire.write(values[index]) == 1;
        }
        const uint8_t wire_status = wire.endTransmission(true);
        return i2c::MapWireFailure(!buffered || wire_status != 0, !buffered, false, wire_status);
    }

    // O repeated START usa o mesmo endereço e a leitura final libera Wire.
    i2c::OPERATION_RESULT ReadRegisters(
        TwoWire& wire,
        uint8_t reg,
        uint8_t* values,
        size_t length
    ) {
        wire.beginTransmission(address);
        if (wire.write(reg) != 1) {
            wire.endTransmission(true);
            return i2c::OPERATION_RESULT::OPERATION_FAILED;
        }
        const uint8_t wire_status = wire.endTransmission(false);
        if (wire_status != 0) {
            wire.endTransmission(true);
            return i2c::MapWireFailure(true, false, false, wire_status);
        }

        bool complete = wire.requestFrom(address, length, true) == length;
        for (size_t index = 0; index < length; ++index) {
            const int value = wire.read();
            if (value < 0) complete = false;
            else values[index] = static_cast<uint8_t>(value);
        }
        return complete ? i2c::OPERATION_RESULT::OK : i2c::OPERATION_RESULT::READ_INCOMPLETE;
    }

    // Expande o complemento de dois sem conversão fora da faixa do int16_t.
    int16_t ReadSignedAxis(
        const uint8_t* bytes
    ) {
        const int32_t value = (static_cast<int32_t>(bytes[0]) << 8) | bytes[1];
        return static_cast<int16_t>(value <= INT16_MAX ? value : value - 65536);
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

i2c::OPERATION_RESULT IdentifyAndReset(
    TwoWire& wire,
    IDENTITY& identity
) {
    uint8_t value = 0;
    const auto result = ReadRegisters(wire, who_am_i_register, &value, 1);
    identity = {value, result == i2c::OPERATION_RESULT::OK};
    if (!identity.available) return result;
    if (identity.value != expected_identity) return i2c::OPERATION_RESULT::OPERATION_FAILED;
    return WriteRegisters(wire, power_management_register, &reset, 1);
}

i2c::OPERATION_RESULT Wake(
    TwoWire& wire
) {
    return WriteRegisters(wire, power_management_register, power_configuration, sizeof(power_configuration));
}

i2c::OPERATION_RESULT Configure(
    TwoWire& wire
) {
    const auto result = WriteRegisters(wire, config_register, range_and_filters, sizeof(range_and_filters));
    if (result != i2c::OPERATION_RESULT::OK) return result;
    return WriteRegisters(wire, sample_divider_register, &sample_divider, 1);
}

i2c::OPERATION_RESULT ReadRaw(
    TwoWire& wire,
    RAW_SAMPLE& sample
) {
    uint8_t bytes[output_bytes] = {};
    const auto result = ReadRegisters(wire, first_output_register, bytes, sizeof(bytes));
    if (result != i2c::OPERATION_RESULT::OK) return result;

    for (size_t axis = 0; axis < axis_count; ++axis) {
        sample.acceleration[axis] = ReadSignedAxis(bytes + 2 * axis);
        sample.angular_rate[axis] = ReadSignedAxis(bytes + gyro_offset + 2 * axis);
    }
    return i2c::OPERATION_RESULT::OK;
}

float ToAccelerationG(
    int16_t raw
) {
    return raw / 16384.0f;
}

float ToAngularRateDps(
    int16_t raw
) {
    return raw / 131.0f;
}

ACTION Next(
    const STATE& state,
    uint64_t now_ms,
    i2c::SERVICE_STATE service,
    uint64_t recovery_cycles
) {
    if (service != i2c::SERVICE_STATE::READY) return ACTION::WAIT;
    if (state.recovery_cycles != recovery_cycles || state.next == ACTION::IDENTIFY_RESET) {
        return ACTION::IDENTIFY_RESET;
    }
    if (now_ms < state.completed_ms) return ACTION::WAIT;
    if (state.valid) return ACTION::READ;
    const uint64_t wait_ms = state.next == ACTION::READ ? first_sample_wait_ms : startup_wait_ms;
    if (now_ms - state.completed_ms < wait_ms) return ACTION::WAIT;
    return state.next;
}

void Complete(
    STATE& state,
    ACTION action,
    uint64_t completed_ms,
    uint64_t recovery_cycles,
    bool success,
    const RAW_SAMPLE& sample
) {
    if (action == ACTION::WAIT) return;
    if (!success) {
        state = {};
        state.recovery_cycles = recovery_cycles;
        return;
    }

    state.recovery_cycles = recovery_cycles;
    state.completed_ms = completed_ms;
    state.valid = action == ACTION::READ;
    if (action == ACTION::IDENTIFY_RESET) state.next = ACTION::WAKE;
    else if (action == ACTION::WAKE) state.next = ACTION::CONFIGURE;
    else state.next = ACTION::READ;
    if (state.valid) state.sample = sample;
}

bool IsAvailable(
    const STATE& state,
    uint64_t now_ms,
    i2c::SERVICE_STATE service,
    uint64_t recovery_cycles
) {
    return state.valid && service == i2c::SERVICE_STATE::READY
        && state.recovery_cycles == recovery_cycles && now_ms >= state.completed_ms
        && now_ms - state.completed_ms <= stale_after_ms;
}

} // namespace obc_mpu9250
