#include "ObcBh1750.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcBh1750.h".
 */

namespace obc_bh1750 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint8_t power_on = 0x01;
    constexpr uint8_t nominal_mtreg = 69;
    constexpr uint8_t continuous_high_resolution = 0x10;
    constexpr uint8_t configuration[] = {
        power_on,
        static_cast<uint8_t>(0x40 | (nominal_mtreg >> 5)),
        static_cast<uint8_t>(0x60 | (nominal_mtreg & 0x1F)),
        continuous_high_resolution
    };

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

i2c::OPERATION_RESULT Configure(
    TwoWire& wire,
    uint8_t address
) {
    for (uint8_t command : configuration) {
        wire.beginTransmission(address);
        const bool buffer_failure = wire.write(command) != 1;
        const uint8_t wire_status = wire.endTransmission(true);
        const auto result = i2c::MapWireFailure(buffer_failure || wire_status != 0,
            buffer_failure, false, wire_status);
        if (result != i2c::OPERATION_RESULT::OK) return result;
    }
    return i2c::OPERATION_RESULT::OK;
}

i2c::OPERATION_RESULT ReadRaw(
    TwoWire& wire,
    uint8_t address,
    uint16_t& raw
) {
    constexpr size_t length = 2;
    const size_t received = wire.requestFrom(address, length, true);
    const int high = wire.read();
    const int low = wire.read();
    if (received != length || high < 0 || low < 0) return i2c::OPERATION_RESULT::READ_INCOMPLETE;

    raw = static_cast<uint16_t>((high << 8) | low);
    return i2c::OPERATION_RESULT::OK;
}

float ToLux(
    uint16_t raw
) {
    return raw / 1.2f;
}

ACTION Next(
    const CHANNEL& channel,
    uint64_t now_ms,
    i2c::SERVICE_STATE service,
    uint64_t recovery_cycles
) {
    if (service != i2c::SERVICE_STATE::READY) return ACTION::WAIT;
    if (!channel.configured || channel.recovery_cycles != recovery_cycles) return ACTION::CONFIGURE;
    if (now_ms < channel.configured_ms || now_ms - channel.configured_ms < conversion_wait_ms) {
        return ACTION::WAIT;
    }
    return ACTION::READ;
}

void Complete(
    CHANNEL& channel,
    ACTION action,
    uint64_t completed_ms,
    uint64_t recovery_cycles,
    bool success,
    uint16_t raw
) {
    if (!success || action == ACTION::CONFIGURE) {
        channel = {};
        channel.recovery_cycles = recovery_cycles;
        channel.configured = success;
        if (success) channel.configured_ms = completed_ms;
        return;
    }

    channel.sampled_ms = completed_ms;
    channel.raw = raw;
    channel.valid = true;
}

bool IsAvailable(
    const CHANNEL& channel,
    uint64_t now_ms,
    i2c::SERVICE_STATE service,
    uint64_t recovery_cycles
) {
    return channel.valid && service == i2c::SERVICE_STATE::READY
        && channel.recovery_cycles == recovery_cycles && now_ms >= channel.sampled_ms
        && now_ms - channel.sampled_ms <= stale_after_ms;
}

} // namespace obc_bh1750
