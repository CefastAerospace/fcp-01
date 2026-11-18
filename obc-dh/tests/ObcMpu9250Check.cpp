/**
 * @file ObcMpu9250Check.cpp
 * @brief Exercita o driver MPU9250 e a sequência de aquisição sem hardware.
 */
#include "ObcMpu9250.h" // Operações e política de validade do driver real.
#include <cassert> // Invariantes de resultados e estado.
#include <cstdio> // Resultado da regressão.

namespace {

    using namespace obc_mpu9250;
    using RESULT = i2c::OPERATION_RESULT;
    using SERVICE = i2c::SERVICE_STATE;

    // Identidade divergente bloqueia escritas; configuração válida preserva a ordem.
    void CheckConfiguration(
    ) {
        TwoWire wire;
        IDENTITY identity = {};
        wire.registers[0x75] = 0x73;
        assert(IdentifyAndReset(wire, identity) == RESULT::OPERATION_FAILED);
        assert(identity.available && identity.value == 0x73 && wire.write_count == 0 && !wire.held);
        wire.registers[0x75] = 0x71;
        assert(IdentifyAndReset(wire, identity) == RESULT::OK && identity.available && identity.value == 0x71);
        assert(Wake(wire) == RESULT::OK);
        assert(Configure(wire) == RESULT::OK);
        constexpr TwoWire::WRITE expected[] = {
            {0x6B, 0x80}, {0x6B, 0x01}, {0x6C, 0x00}, {0x1A, 0x06},
            {0x1B, 0x00}, {0x1C, 0x00}, {0x1D, 0x06}, {0x19, 99}
        };
        assert(!wire.held && wire.write_count == 8 && wire.stops == 6 && wire.read_requests == 2);
        for (size_t index = 0; index < wire.write_count; ++index) {
            assert(wire.writes[index].reg == expected[index].reg && wire.writes[index].value == expected[index].value);
        }
    }

    // Distingue identidade ausente de falha na escrita e fecha todas as transferências.
    void CheckTransportFailures(
    ) {
        TwoWire wire;
        IDENTITY identity = {0x71, true};
        wire.select_status = 2;
        assert(IdentifyAndReset(wire, identity) == RESULT::DEVICE_NACK);
        assert(!identity.available && !wire.held && wire.write_count == 0 && wire.read_requests == 0);
        wire.select_status = 0;
        wire.read_limit = 0;
        assert(IdentifyAndReset(wire, identity) == RESULT::READ_INCOMPLETE);
        assert(!identity.available && !wire.held && wire.write_count == 0);

        wire.read_limit = 1;
        wire.registers[0x75] = 0x71;
        wire.write_status = 5;
        assert(IdentifyAndReset(wire, identity) == RESULT::TRANSFER_TIMEOUT);
        assert(identity.available && identity.value == 0x71 && !wire.held && wire.write_count == 0);

        wire.write_status = 2;
        const auto before_config = wire.stops;
        assert(Configure(wire) == RESULT::DEVICE_NACK);
        assert(!wire.held && wire.stops == before_config + 1 && wire.write_count == 0);

        wire.write_status = 0;
        wire.fail_buffer_at = 0;
        assert(IdentifyAndReset(wire, identity) == RESULT::OPERATION_FAILED);
        assert(!identity.available && !wire.held && wire.write_count == 0);
    }

    // Uma única leitura MSB primeiro conserva o sinal dos seis eixos e ignora temperatura.
    void CheckMeasurement(
    ) {
        TwoWire wire;
        constexpr uint8_t output[] = {
            0x40, 0x00, 0xC0, 0x00, 0x80, 0x00, 0xFA, 0xCE,
            0x00, 0x83, 0xFF, 0x7D, 0x7F, 0xFF
        };
        for (size_t index = 0; index < sizeof(output); ++index) wire.registers[0x3B + index] = output[index];
        RAW_SAMPLE sample = {{5, 6, 7}, {8, 9, 10}};
        wire.read_limit = 13;
        assert(ReadRaw(wire, sample) == RESULT::READ_INCOMPLETE && !wire.held);
        assert(sample.acceleration[0] == 5 && sample.angular_rate[2] == 10);
        wire.read_limit = 14;
        wire.fail_read_at = 0;
        assert(ReadRaw(wire, sample) == RESULT::READ_INCOMPLETE && !wire.held);
        assert(sample.acceleration[0] == 5 && sample.angular_rate[2] == 10);
        wire.fail_read_at = -1;
        assert(ReadRaw(wire, sample) == RESULT::OK && !wire.held && wire.read_requests == 3);
        assert(sample.acceleration[0] == 16384 && sample.acceleration[1] == -16384 && sample.acceleration[2] == -32768);
        assert(sample.angular_rate[0] == 131 && sample.angular_rate[1] == -131 && sample.angular_rate[2] == 32767);
        assert(ToAccelerationG(sample.acceleration[0]) == 1.0f && ToAccelerationG(sample.acceleration[1]) == -1.0f);
        assert(ToAccelerationG(sample.acceleration[2]) == -2.0f);
        assert(ToAngularRateDps(sample.angular_rate[0]) == 1.0f && ToAngularRateDps(sample.angular_rate[1]) == -1.0f);
    }

    // Cada intervalo começa na conclusão da etapa anterior, fora do callback.
    void CheckStages(
    ) {
        STATE state = {};
        const RAW_SAMPLE sample = {{1, 2, 3}, {4, 5, 6}};
        assert(Next(state, 0, SERVICE::UNAVAILABLE, 0) == ACTION::WAIT);
        assert(Next(state, 0, SERVICE::READY, 0) == ACTION::IDENTIFY_RESET);
        Complete(state, ACTION::IDENTIFY_RESET, 0, 0, true, {});
        assert(Next(state, 99, SERVICE::READY, 0) == ACTION::WAIT);
        assert(Next(state, 100, SERVICE::READY, 0) == ACTION::WAKE);
        Complete(state, ACTION::WAKE, 100, 0, true, {});
        assert(Next(state, 199, SERVICE::READY, 0) == ACTION::WAIT);
        assert(Next(state, 200, SERVICE::READY, 0) == ACTION::CONFIGURE);
        Complete(state, ACTION::CONFIGURE, 200, 0, true, {});
        assert(Next(state, 399, SERVICE::READY, 0) == ACTION::WAIT);
        assert(Next(state, 400, SERVICE::READY, 0) == ACTION::READ);
        assert(!IsAvailable(state, 400, SERVICE::READY, 0));
        Complete(state, ACTION::READ, 400, 0, true, sample);
        assert(IsAvailable(state, 3400, SERVICE::READY, 0) && state.sample.angular_rate[2] == 6);
        assert(!IsAvailable(state, 3401, SERVICE::READY, 0));
        assert(!IsAvailable(state, 400, SERVICE::RECOVERING, 0));
        assert(!IsAvailable(state, 401, SERVICE::READY, 1));
        assert(Next(state, 401, SERVICE::READY, 1) == ACTION::IDENTIFY_RESET);

        Complete(state, ACTION::IDENTIFY_RESET, 500, 1, true, {});
        Complete(state, ACTION::WAKE, 600, 1, true, {});
        Complete(state, ACTION::CONFIGURE, 700, 1, true, {});
        assert(!IsAvailable(state, 900, SERVICE::READY, 1));
        Complete(state, ACTION::READ, 900, 1, true, sample);
        assert(IsAvailable(state, 900, SERVICE::READY, 1));
        Complete(state, ACTION::READ, 1900, 1, false, {});
        assert(!IsAvailable(state, 1900, SERVICE::READY, 1));
        assert(Next(state, 1900, SERVICE::READY, 1) == ACTION::IDENTIFY_RESET);
    }

} // namespace

int main(
) {
    CheckConfiguration();
    CheckTransportFailures();
    CheckMeasurement();
    CheckStages();
    std::puts("MPU9250: identidade, reset, escalas, falhas, tempos e recuperacao aprovados.");
}
