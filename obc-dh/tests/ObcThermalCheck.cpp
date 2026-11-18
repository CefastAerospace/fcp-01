/**
 * @file ObcThermalCheck.cpp
 * @brief Driver LM75A real, falhas de registradores e política temporal de bancada.
 */
#include "Lm75a.h" // Driver real com Wire restrito aos registradores de teste.
#include "ObcThermal.h" // Mesma política e guarda usadas pelo adaptador ESP32.
#include "ObcEps.h" // Classificador Wire compartilhado sem duplicação.
#include <cassert> // Critérios independentes do resultado da implementação.
#include <cstdio> // Resultado do executável nativo.
#include <limits> // Fronteira de tempo monotônico.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "Lm75a.h" e "ObcThermal.h".
 */

namespace {

    // Vetores do formato de 11 bits, sem usar o conversor para gerar entradas.
    void CheckTemperatures(
    ) {
        const uint16_t raw_values[] = {0x0000, 0x0020, 0xFFE0, 0x1900, 0xE700, 0xC900, 0x7D00};
        const float expected[] = {0.0f, 0.125f, -0.125f, 25.0f, -25.0f, -55.0f, 125.0f};
        for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index) {
            float value = 999;
            assert(CEFAST::thermal::DecodeTemperature(raw_values[index], value));
            assert(value == expected[index]);
        }
        float value = 999;
        assert(CEFAST::thermal::DecodeTemperature(0x191F, value) && value == 25.0f);
        value = 999;
        assert(!CEFAST::thermal::DecodeTemperature(0x7D20, value) && value == 999);
        assert(!CEFAST::thermal::DecodeTemperature(0xC8E0, value) && value == 999);
        assert(!CEFAST::thermal::DecodeTemperature(0x8000, value) && value == 999);
    }

    // Conf usa um byte; reconfiguração conserva os bits OS e ambos os limites.
    void CheckRegisterProtocol(
    ) {
        using CEFAST::thermal::STATUS;
        TwoWire wire;
        CEFAST::thermal::DIAGNOSTIC diagnostic = {};
        for (uint8_t address = 0x48; address <= 0x4F; ++address) {
            wire.present[address] = true;
            wire.registers[address][0] = 0xE700;
            wire.registers[address][1] = 0xFF;
            wire.registers[address][2] = 0x4B00;
            wire.registers[address][3] = 0x5000;
            assert(CEFAST::thermal::Configure(wire, address, diagnostic) == STATUS::OK);
            assert(!wire.held && wire.registers[address][1] == 0x1E);
            assert(wire.configuration_writes[address] == 1);
            assert(wire.registers[address][2] == 0x4B00 && wire.registers[address][3] == 0x5000);
            uint16_t raw = 0;
            assert(CEFAST::thermal::ReadRaw(wire, address, raw, diagnostic) == STATUS::OK);
            assert(raw == 0xE700 && diagnostic.received == 2 && !wire.held);
        }
        uint16_t raw = 0xBEEF;
        const uint32_t before = wire.transactions[0x48];
        const uint8_t invalid_addresses[] = {0x00, 0x47, 0x50, 0xFF};
        for (uint8_t address : invalid_addresses) {
            assert(CEFAST::thermal::Configure(wire, address, diagnostic) == STATUS::INVALID_ADDRESS);
            assert(CEFAST::thermal::ReadRaw(wire, address, raw, diagnostic) == STATUS::INVALID_ADDRESS);
            assert(raw == 0xBEEF && !wire.held);
        }
        assert(wire.transactions[0x48] == before);
    }

    // Cada falha termina o barramento e preserva saída; um pedido seguinte funciona.
    void CheckFailures(
    ) {
        using CEFAST::thermal::STATUS;
        using i2c::OPERATION_RESULT;
        TwoWire wire;
        wire.present[0x48] = true;
        wire.registers[0x48][0] = 0x1900;
        CEFAST::thermal::DIAGNOSTIC diagnostic = {};
        uint16_t raw = 0xBEEF;
        const uint8_t errors[] = {2, 3, 4, 5};
        const OPERATION_RESULT expected[] = {OPERATION_RESULT::DEVICE_NACK,
            OPERATION_RESULT::DEVICE_NACK, OPERATION_RESULT::BUS_ERROR,
            OPERATION_RESULT::TRANSFER_TIMEOUT};
        for (size_t index = 0; index < 4; ++index) {
            wire.select_error = errors[index];
            assert(CEFAST::thermal::ReadRaw(wire, 0x48, raw, diagnostic) == STATUS::I2C_ERROR);
            assert(diagnostic.wire_status == errors[index] && raw == 0xBEEF && !wire.held);
            assert(obc_eps::MapWireFailure(true, false, false, diagnostic.wire_status) == expected[index]);
        }
        wire.present[0x48] = false;
        assert(CEFAST::thermal::ReadRaw(wire, 0x48, raw, diagnostic) == STATUS::I2C_ERROR);
        assert(diagnostic.wire_status == 2 && !wire.held && raw == 0xBEEF);
        wire.present[0x48] = true;
        for (int count = 0; count <= 1; ++count) {
            wire.read_bytes = count;
            assert(CEFAST::thermal::ReadRaw(wire, 0x48, raw, diagnostic) == STATUS::READ_INCOMPLETE);
            assert(wire.available() == 0 && !wire.held && raw == 0xBEEF);
            assert(obc_eps::MapWireFailure(true, false, true, diagnostic.wire_status)
                == OPERATION_RESULT::READ_INCOMPLETE);
        }
        wire.available_bytes = 1;
        assert(CEFAST::thermal::ReadRaw(wire, 0x48, raw, diagnostic) == STATUS::READ_INCOMPLETE);
        assert(wire.available() == 0 && raw == 0xBEEF && !wire.held);
        wire.fail_read_at = 1;
        assert(CEFAST::thermal::ReadRaw(wire, 0x48, raw, diagnostic) == STATUS::READ_INCOMPLETE);
        assert(wire.available() == 0 && raw == 0xBEEF && !wire.held);
        wire.fail_write_at = 0;
        assert(CEFAST::thermal::ReadRaw(wire, 0x48, raw, diagnostic) == STATUS::BUFFER_ERROR);
        assert(raw == 0xBEEF && !wire.held);
        wire.fail_write_at = 1;
        assert(CEFAST::thermal::Configure(wire, 0x48, diagnostic) == STATUS::BUFFER_ERROR);
        assert(!wire.held);
        wire.write_error = 5;
        assert(CEFAST::thermal::Configure(wire, 0x48, diagnostic) == STATUS::I2C_ERROR);
        assert(diagnostic.wire_status == 5 && !wire.held);
        wire.read_bytes = 0;
        assert(CEFAST::thermal::Configure(wire, 0x48, diagnostic) == STATUS::READ_INCOMPLETE);
        assert(!wire.held);
        assert(CEFAST::thermal::Configure(wire, 0x48, diagnostic) == STATUS::OK);
        assert(CEFAST::thermal::ReadRaw(wire, 0x48, raw, diagnostic) == STATUS::OK);
        assert(raw == 0x1900 && !wire.held);
    }

    // Relógio controlado cobre espera inicial, obsolescência, erro e geração nova.
    void CheckTimingAndRecovery(
    ) {
        using namespace obc_thermal;
        Channel channel;
        assert(channel.Next(0, false, 0) == ACTION::WAIT);
        assert(!channel.GetState().attempted && !channel.GetState().has_valid);
        assert(channel.Next(0, true, 0) == ACTION::CONFIGURE);
        channel.Complete(ACTION::CONFIGURE, 0, 0, false, true);
        assert(channel.GetState().configured && !channel.GetState().has_valid);
        assert(channel.Next(999, true, 0) == ACTION::WAIT);
        assert(channel.Next(1000, true, 0) == ACTION::READ);
        channel.Complete(ACTION::READ, 1000, 0, true, true);
        assert(IsAvailable(channel.GetState(), 1000, true, 0));
        assert(channel.Next(1999, true, 0) == ACTION::WAIT);
        assert(channel.Next(2000, true, 0) == ACTION::READ);
        assert(IsAvailable(channel.GetState(), 4000, true, 0));
        assert(!IsAvailable(channel.GetState(), 4001, true, 0));
        assert(!IsAvailable(channel.GetState(), 999, true, 0));
        assert(!IsAvailable(channel.GetState(), 1000, false, 0));
        assert(!IsAvailable(channel.GetState(), 1000, true, 1));

        assert(!CanExecute(ACTION::READ, 0, 1, true));
        assert(CanExecute(ACTION::CONFIGURE, 0, 1, true));
        assert(!CanExecute(ACTION::READ, 0, 0, false));
        assert(!CanExecute(ACTION::WAIT, 0, 0, true));
        assert(channel.Next(2001, true, 1) == ACTION::CONFIGURE);
        assert(channel.GetState().has_valid && !channel.GetState().latest_valid);
        channel.Complete(ACTION::CONFIGURE, 2001, 1, false, true);
        assert(channel.Next(3000, true, 1) == ACTION::WAIT);
        assert(channel.Next(3001, true, 1) == ACTION::READ);
        assert(CanExecute(ACTION::READ, 1, 1, true));
        channel.Complete(ACTION::READ, 3001, 1, false, false);
        assert(channel.GetState().last_valid_ms == 1000 && channel.GetState().has_valid);
        assert(channel.Next(8000, true, 1) == ACTION::WAIT);
        assert(channel.Next(8001, true, 1) == ACTION::CONFIGURE);
        channel.Complete(ACTION::CONFIGURE, 8001, 1, false, true);
        // Dado fora da faixa invalida a amostra, mas não declara falha do I2C.
        channel.Complete(ACTION::READ, 9001, 1, false, true);
        assert(channel.GetState().configured && !channel.GetState().latest_valid);
        assert(channel.Next(10000, true, 1) == ACTION::WAIT);
        assert(channel.Next(10001, true, 1) == ACTION::READ);
        channel.Complete(ACTION::READ, 10001, 1, true, true);
        assert(IsAvailable(channel.GetState(), 10001, true, 1));
        assert(channel.GetState().last_valid_ms == 10001);
        channel.Complete(ACTION::WAIT, 10002, 9, false, false);
        assert(channel.GetState().last_valid_ms == 10001 && channel.GetState().recovery_cycles == 1);

        CHANNEL_STATE zero = {};
        zero.configured = zero.has_valid = zero.latest_valid = true;
        assert(IsAvailable(zero, 0, true, 0));
        const uint64_t maximum = std::numeric_limits<uint64_t>::max();
        channel.Complete(ACTION::CONFIGURE, maximum - 1, 1, false, true);
        assert(channel.GetState().next_due_ms == maximum);
    }

    // A indisponibilidade de um dispositivo não impede os outros quatro.
    void CheckIndependentChannels(
    ) {
        using namespace obc_thermal;
        using CEFAST::thermal::STATUS;
        Channel channels[channel_count];
        TwoWire wire;
        CEFAST::thermal::DIAGNOSTIC diagnostic = {};
        for (size_t index = 0; index < channel_count; ++index) {
            const uint8_t address = addresses[index];
            wire.present[address] = index != 2;
            wire.registers[address][0] = static_cast<uint16_t>((20 + index) << 8);
            assert(channels[index].Next(0, true, 0) == ACTION::CONFIGURE);
            const bool configured = CEFAST::thermal::Configure(wire, address, diagnostic) == STATUS::OK;
            channels[index].Complete(ACTION::CONFIGURE, 0, 0, false, configured);
            assert(configured == (index != 2));
        }
        for (size_t index = 0; index < channel_count; ++index) {
            const ACTION action = channels[index].Next(1000, true, 0);
            if (index == 2) {
                assert(action == ACTION::WAIT && !channels[index].GetState().has_valid);
                continue;
            }
            assert(action == ACTION::READ);
            uint16_t raw = 0;
            float value = 0;
            assert(CEFAST::thermal::ReadRaw(wire, addresses[index], raw, diagnostic) == STATUS::OK);
            assert(CEFAST::thermal::DecodeTemperature(raw, value) && value == 20.0f + index);
            channels[index].Complete(action, 1000, 0, true, true);
            assert(IsAvailable(channels[index].GetState(), 1000, true, 0));
        }
        assert(channels[2].Next(4999, true, 0) == ACTION::WAIT);
        wire.present[addresses[2]] = true;
        assert(channels[2].Next(5000, true, 0) == ACTION::CONFIGURE);
        assert(CEFAST::thermal::Configure(wire, addresses[2], diagnostic) == STATUS::OK);
        channels[2].Complete(ACTION::CONFIGURE, 5000, 0, false, true);
        assert(channels[2].Next(5999, true, 0) == ACTION::WAIT);
        assert(channels[2].Next(6000, true, 0) == ACTION::READ);
    }

} // namespace

int main(
) {
    CheckTemperatures();
    CheckRegisterProtocol();
    CheckFailures();
    CheckTimingAndRecovery();
    CheckIndependentChannels();
    std::puts("LM75A: driver real, conversao, falhas, cinco canais, cadencia e recuperacao aprovados");
}
