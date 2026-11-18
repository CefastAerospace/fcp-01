#include "Lm75a.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "Lm75a.h".
 */

namespace CEFAST {
namespace thermal {
namespace {

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Valida antes de tocar Wire; não faz varredura nem altera a seleção física.
    bool CheckAddress(
        uint8_t address,
        DIAGNOSTIC& diagnostic
    ) {
        diagnostic = {};
        if (address >= 0x48 && address <= 0x4F) return true;
        diagnostic.status = STATUS::INVALID_ADDRESS;
        return false;
    }

    // ReadRaw/Configure usam somente um ou dois bytes; STOP encerra todo caminho.
    STATUS ReadRegister(
        TwoWire& wire,
        uint8_t address,
        uint8_t reg,
        uint8_t length,
        uint16_t& value,
        DIAGNOSTIC& diagnostic
    ) {
        diagnostic.phase = PHASE::SELECT_REGISTER;
        diagnostic.reg = reg;
        wire.beginTransmission(address);
        if (wire.write(reg) != 1) {
            diagnostic.phase = PHASE::BUFFER;
            diagnostic.wire_status = wire.endTransmission(true);
            return diagnostic.status = STATUS::BUFFER_ERROR;
        }
        diagnostic.wire_status = wire.endTransmission(false);
        if (diagnostic.wire_status != 0) {
            wire.endTransmission(true);
            return diagnostic.status = STATUS::I2C_ERROR;
        }

        diagnostic.phase = PHASE::READ;
        diagnostic.received = wire.requestFrom(
            static_cast<int>(address), static_cast<int>(length), static_cast<int>(true));
        if (diagnostic.received != length || wire.available() < length) {
            while (wire.available() > 0) wire.read();
            return diagnostic.status = STATUS::READ_INCOMPLETE;
        }
        uint16_t decoded = 0;
        for (uint8_t index = 0; index < length; ++index) {
            const int byte = wire.read();
            if (byte < 0) {
                while (wire.available() > 0) wire.read();
                return diagnostic.status = STATUS::READ_INCOMPLETE;
            }
            decoded = static_cast<uint16_t>((decoded << 8) | static_cast<uint8_t>(byte));
        }
        value = decoded;
        return diagnostic.status = STATUS::OK;
    }

    // Conf possui um byte; nenhuma escrita alcança Tos/Thyst.
    STATUS WriteConfiguration(
        TwoWire& wire,
        uint8_t address,
        uint8_t configuration,
        DIAGNOSTIC& diagnostic
    ) {
        diagnostic = {};
        diagnostic.phase = PHASE::WRITE;
        diagnostic.reg = 0x01;
        wire.beginTransmission(address);
        if (wire.write(0x01) != 1 || wire.write(configuration) != 1) {
            diagnostic.phase = PHASE::BUFFER;
            diagnostic.wire_status = wire.endTransmission(true);
            return diagnostic.status = STATUS::BUFFER_ERROR;
        }
        diagnostic.wire_status = wire.endTransmission(true);
        return diagnostic.status = diagnostic.wire_status == 0 ? STATUS::OK : STATUS::I2C_ERROR;
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

STATUS Configure(
    TwoWire& wire,
    uint8_t address,
    DIAGNOSTIC& diagnostic
) {
    if (!CheckAddress(address, diagnostic)) return diagnostic.status;
    uint16_t configuration = 0;
    if (ReadRegister(wire, address, 0x01, 1, configuration, diagnostic) != STATUS::OK) {
        return diagnostic.status;
    }
    return WriteConfiguration(wire, address, static_cast<uint8_t>(configuration & 0x1E), diagnostic);
}

STATUS ReadRaw(
    TwoWire& wire,
    uint8_t address,
    uint16_t& raw,
    DIAGNOSTIC& diagnostic
) {
    if (!CheckAddress(address, diagnostic)) return diagnostic.status;
    return ReadRegister(wire, address, 0x00, 2, raw, diagnostic);
}

bool DecodeTemperature(
    uint16_t raw,
    float& temperature_c
) {
    // A extensão de sinal é explícita: não desloca um inteiro negativo.
    int32_t counts = raw >> 5;
    if (counts & 0x400) counts -= 0x800;
    const float value = static_cast<float>(counts) * 0.125f;
    if (value < -55.0f || value > 125.0f) return false;
    temperature_c = value;
    return true;
}

} // namespace thermal
} // namespace CEFAST
