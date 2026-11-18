#include "ObcAs5600.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcAs5600.h".
 */

namespace obc_as5600 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint8_t raw_angle_register = 0x0C;
    constexpr uint8_t raw_angle_high_mask = 0x0F;
    constexpr uint8_t angle_bytes = 2;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Descarta no máximo os dois bytes solicitados, mesmo se read falhar.
    i2c::OPERATION_RESULT DiscardIncompleteRead(
        TwoWire& wire
    ) {
        for (uint8_t index = 0; index < angle_bytes && wire.available() > 0; ++index) {
            wire.read();
        }
        return i2c::OPERATION_RESULT::READ_INCOMPLETE;
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

i2c::OPERATION_RESULT ReadRaw(
    TwoWire& wire,
    uint16_t& raw_count
) {
    wire.beginTransmission(address);
    if (wire.write(raw_angle_register) != 1) {
        wire.endTransmission(true);
        return i2c::OPERATION_RESULT::OPERATION_FAILED;
    }
    const uint8_t status = wire.endTransmission(false);
    if (status != 0) {
        wire.endTransmission(true);
        return i2c::MapWireFailure(true, false, false, status);
    }

    const auto received = wire.requestFrom(address, static_cast<size_t>(angle_bytes), true);
    if (received != angle_bytes || wire.available() < angle_bytes) return DiscardIncompleteRead(wire);
    const int high = wire.read();
    const int low = wire.read();
    if (high < 0 || low < 0) return DiscardIncompleteRead(wire);

    raw_count = static_cast<uint16_t>(((high & raw_angle_high_mask) << 8) | low);
    return i2c::OPERATION_RESULT::OK;
}

bool IsAvailable(
    const SAMPLE& sample,
    uint64_t now_ms,
    i2c::SERVICE_STATE service,
    uint64_t recovery_cycles
) {
    return sample.valid && service == i2c::SERVICE_STATE::READY
        && sample.recovery_cycles == recovery_cycles && now_ms >= sample.sampled_ms
        && now_ms - sample.sampled_ms <= stale_after_ms;
}

} // namespace obc_as5600
