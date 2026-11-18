/**
 * @file ObcAs5600Check.cpp
 * @brief Regressão nativa da leitura e da disponibilidade do AS5600.
 */
#include "ObcAs5600.h" // Leitura e validade reais com Wire simulado.
#include <cassert> // Verifica estados e valores sem hardware.
#include <cstdio> // Resultado do processo de regressão.

namespace {

    using RESULT = i2c::OPERATION_RESULT;
    using SERVICE = i2c::SERVICE_STATE;

    // Verifica limites, máscara e ordem dos bytes com uma transação por amostra.
    void CheckRead(
    ) {
        TwoWire wire;
        uint16_t count = 123;
        assert(obc_as5600::ReadRaw(wire, count) == RESULT::OK && count == 0);
        wire.high = 0xAF;
        wire.low = 0xFF;
        assert(obc_as5600::ReadRaw(wire, count) == RESULT::OK && count == 4095);
        wire.high = 0x02;
        wire.low = 0x71;
        assert(obc_as5600::ReadRaw(wire, count) == RESULT::OK && count == 0x271);
        assert(!wire.held && wire.read_requests == 3 && wire.stops == 3 && wire.available() == 0);
    }

    // Erros de seleção e buffer encerram a transação antes da leitura.
    void CheckSelectionFailures(
    ) {
        TwoWire wire;
        uint16_t count = 123;
        wire.fail_buffer = true;
        assert(obc_as5600::ReadRaw(wire, count) == RESULT::OPERATION_FAILED);
        assert(!wire.held && wire.read_requests == 0 && wire.stops == 1 && count == 123);

        wire.fail_buffer = false;
        wire.select_status = 2;
        assert(obc_as5600::ReadRaw(wire, count) == RESULT::DEVICE_NACK);
        assert(!wire.held && wire.read_requests == 0 && wire.stops == 2 && count == 123);

        wire.select_status = 5;
        assert(obc_as5600::ReadRaw(wire, count) == RESULT::TRANSFER_TIMEOUT);
        assert(!wire.held && wire.read_requests == 0 && wire.stops == 3 && count == 123);
    }

    // Quantidade curta ou byte inválido preservam a saída e permitem novo pedido.
    void CheckReadFailures(
    ) {
        TwoWire wire;
        uint16_t count = 123;
        wire.reported_bytes = 1;
        wire.readable_bytes = 1;
        assert(obc_as5600::ReadRaw(wire, count) == RESULT::READ_INCOMPLETE);
        assert(!wire.held && wire.available() == 0 && count == 123);

        wire.reported_bytes = 2;
        assert(obc_as5600::ReadRaw(wire, count) == RESULT::READ_INCOMPLETE);
        assert(!wire.held && wire.available() == 0 && count == 123);

        wire.readable_bytes = 2;
        wire.fail_read_at = 1;
        assert(obc_as5600::ReadRaw(wire, count) == RESULT::READ_INCOMPLETE);
        assert(!wire.held && wire.available() == 0 && count == 123);

        wire.fail_read_at = -1;
        assert(obc_as5600::ReadRaw(wire, count) == RESULT::OK && count == 0);
        assert(!wire.held && wire.stops == 4);
    }

    // Uma geração recuperada precisa de nova amostra, inclusive para contagem zero.
    void CheckAvailability(
    ) {
        obc_as5600::SAMPLE sample = {};
        assert(!obc_as5600::IsAvailable(sample, 0, SERVICE::READY, 0));
        sample = {0, 0, 0, true};
        assert(obc_as5600::IsAvailable(sample, 0, SERVICE::READY, 0));
        assert(obc_as5600::IsAvailable(sample, 3000, SERVICE::READY, 0));
        assert(!obc_as5600::IsAvailable(sample, 3001, SERVICE::READY, 0));
        assert(!obc_as5600::IsAvailable(sample, 1, SERVICE::RECOVERING, 0));
        assert(!obc_as5600::IsAvailable(sample, 1, SERVICE::UNAVAILABLE, 1));
        assert(!obc_as5600::IsAvailable(sample, 2, SERVICE::READY, 1));
        sample = {2, 1, 0, false};
        assert(!obc_as5600::IsAvailable(sample, 2, SERVICE::READY, 1));
        sample = {3, 1, 0, true};
        assert(!obc_as5600::IsAvailable(sample, 2, SERVICE::READY, 1));
        assert(obc_as5600::IsAvailable(sample, 3, SERVICE::READY, 1));
    }

} // namespace

int main(
) {
    CheckRead();
    CheckSelectionFailures();
    CheckReadFailures();
    CheckAvailability();
    std::puts("AS5600: leitura bruta, erros, STOP, validade e geracao aprovados.");
}
