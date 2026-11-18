/**
 * @file ObcBh1750Check.cpp
 * @brief Regressão nativa da aquisição de dois BH1750 com Wire simulado.
 */
#include "ObcBh1750.h" // Driver, conversão e estado portáteis.
#include <cassert> // Invariantes da leitura e da validade.
#include <cmath> // Tolerância do vetor de conversão para lux.
#include <cstdio> // Resultado da regressão.

namespace {

    using namespace obc_bh1750;
    using RESULT = i2c::OPERATION_RESULT;
    using SERVICE = i2c::SERVICE_STATE;

    // Prova a sequência completa de cada canal com um STOP por comando.
    void CheckConfiguration(
    ) {
        TwoWire wire;
        assert(Configure(wire, 0x23) == RESULT::OK);
        assert(Configure(wire, 0x5C) == RESULT::OK);
        constexpr uint8_t expected[] = {0x01, 0x42, 0x65, 0x10};
        assert(!wire.held && wire.write_count == 8 && wire.stops == 8);
        for (size_t index = 0; index < 4; ++index) {
            assert(wire.writes[index].address == 0x23 && wire.writes[index].command == expected[index]);
            assert(wire.writes[index + 4].address == 0x5C && wire.writes[index + 4].command == expected[index]);
        }
    }

    // Um erro encerra a transação e cancela os comandos restantes daquele canal.
    void CheckConfigurationFailures(
    ) {
        TwoWire wire;
        wire.fail_command_at = 1;
        assert(Configure(wire, 0x23) == RESULT::DEVICE_NACK);
        assert(!wire.held && wire.write_count == 2 && wire.stops == 2);
        wire.fail_buffer_at = 2;
        assert(Configure(wire, 0x23) == RESULT::OPERATION_FAILED);
        assert(!wire.held && wire.write_count == 2 && wire.stops == 3);
        wire.fail_buffer_at = -1;
        assert(Configure(wire, 0x5C) == RESULT::OK);
        assert(!wire.held && wire.write_count == 6 && wire.stops == 7);
    }

    // Confere contagem zero, byte mais significativo primeiro e falhas de leitura.
    void CheckRead(
    ) {
        TwoWire wire;
        uint16_t raw = 24;
        assert(ReadRaw(wire, 0x23, raw) == RESULT::OK && raw == 0 && ToLux(raw) == 0.0f);
        wire.raw = 0x0258;
        assert(ReadRaw(wire, 0x5C, raw) == RESULT::OK && raw == 600);
        assert(wire.last_read_address == 0x5C && std::fabs(ToLux(raw) - 500.0f) < 0.001f);
        wire.raw = 0xFFFF;
        wire.reported_bytes = 1;
        wire.readable_bytes = 1;
        assert(ReadRaw(wire, 0x23, raw) == RESULT::READ_INCOMPLETE && raw == 600);
        wire.reported_bytes = 2;
        assert(ReadRaw(wire, 0x23, raw) == RESULT::READ_INCOMPLETE && raw == 600);
        wire.readable_bytes = 2;
        wire.fail_read_at = 1;
        assert(ReadRaw(wire, 0x23, raw) == RESULT::READ_INCOMPLETE && raw == 600);
        wire.fail_read_at = -1;
        assert(ReadRaw(wire, 0x23, raw) == RESULT::OK && raw == 65535);
        assert(!wire.held && wire.write_count == 0 && wire.stops == 6 && wire.read_requests == 6);
    }

    // Exercita conversão inicial, falha isolada, expiração e retorno após nova geração.
    void CheckChannels(
    ) {
        CHANNEL first = {};
        CHANNEL second = {};
        assert(Next(first, 0, SERVICE::UNAVAILABLE, 0) == ACTION::WAIT);
        assert(Next(first, 0, SERVICE::READY, 0) == ACTION::CONFIGURE);
        Complete(first, ACTION::CONFIGURE, 100, 0, true, 0);
        assert(Next(first, 99, SERVICE::READY, 0) == ACTION::WAIT);
        assert(Next(first, 299, SERVICE::READY, 0) == ACTION::WAIT);
        assert(Next(first, 300, SERVICE::READY, 0) == ACTION::READ);
        assert(!IsAvailable(first, 300, SERVICE::READY, 0));
        Complete(first, ACTION::READ, 300, 0, true, 0);
        Complete(second, ACTION::CONFIGURE, 100, 0, false, 0);
        assert(IsAvailable(first, 300, SERVICE::READY, 0));
        assert(!IsAvailable(second, 300, SERVICE::READY, 0));
        assert(Next(second, 300, SERVICE::READY, 0) == ACTION::CONFIGURE);

        Complete(second, ACTION::CONFIGURE, 400, 0, true, 0);
        Complete(second, ACTION::READ, 600, 0, true, 120);
        Complete(first, ACTION::READ, 1000, 0, false, 0);
        assert(!IsAvailable(first, 1000, SERVICE::READY, 0));
        assert(Next(first, 1000, SERVICE::READY, 0) == ACTION::CONFIGURE);
        assert(IsAvailable(second, 3600, SERVICE::READY, 0));
        assert(!IsAvailable(second, 3601, SERVICE::READY, 0));
        assert(!IsAvailable(second, 999, SERVICE::RECOVERING, 0));
        assert(!IsAvailable(second, 1000, SERVICE::READY, 1));
        assert(Next(second, 1000, SERVICE::READY, 1) == ACTION::CONFIGURE);

        Complete(second, ACTION::CONFIGURE, 1000, 1, true, 0);
        assert(Next(second, 1199, SERVICE::READY, 1) == ACTION::WAIT);
        assert(Next(second, 1200, SERVICE::READY, 1) == ACTION::READ);
        Complete(second, ACTION::READ, 1200, 1, true, 120);
        assert(IsAvailable(second, 1200, SERVICE::READY, 1));
    }

} // namespace

int main(
) {
    CheckConfiguration();
    CheckConfigurationFailures();
    CheckRead();
    CheckChannels();
    std::puts("BH1750: comandos, STOP, lux, falhas por canal e recuperacao aprovados.");
}
