/** @file ObcEpsCheck.cpp
 * @brief Driver EPS real, causas I2C e política de aquisição com relógio controlado.
 */
#include "ObcEps.h" // Mesma política utilizada na bancada ESP32.
#include "EPS_INA219.h" // Driver real, com Wire restrito aos registradores de ensaio.
#include <cassert>
#include <cmath>
#include <cstdio>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcEps.h".
 */

namespace {

    // Prepara um dispositivo com 4 V, shunt negativo e corrente de -100 mA.
    void SetDevice(
        TwoWire& wire,
        uint8_t address
    ) {
        wire.present[address] = true;
        wire.registers[address][2] = static_cast<uint16_t>((1000U << 3) | 2U);
        wire.registers[address][1] = static_cast<uint16_t>(static_cast<int16_t>(-100));
        wire.registers[address][4] = static_cast<uint16_t>(static_cast<int16_t>(-1000));
    }

    // Confere valores do driver sem depender de sua implementação aritmética interna.
    bool Near(
        float left,
        float right
    ) { return std::fabs(left - right) < 0.00001f; }

} // namespace

int main(
) {
    using namespace CEFAST::eps;
    using namespace obc_eps;
    using RESULT = i2c::OPERATION_RESULT;
    TwoWire wire;
    EPS_INA219 driver;
    SetDevice(wire, 0x40);
    SetDevice(wire, 0x41);
    SetDevice(wire, 0x44);

    // A sobrecarga configura somente o sensor indicado, sem Wire.begin disponível.
    assert(driver.begin(wire, INAChannel::Solar));
    assert(wire.transactions[0x40] == 2 && wire.transactions[0x41] == 0);
    assert(wire.registers[0x40][5] == 4096 && wire.registers[0x40][0] == 0x399f);
    assert(!driver.isOnline(INAChannel::Battery));
    assert(driver.begin(wire)); // API antiga conserva os três canais.
    INAReading readings[3];
    assert(driver.readAll(readings) == 3);
    for (const auto& reading : readings) {
        assert(reading.valid && reading.conversionReady && !reading.overflow);
        assert(Near(reading.busVoltage_V, 4.0f));
        assert(Near(reading.shuntVoltage_mV, -1.0f));
        assert(Near(reading.sourceVoltage_V, 3.999f));
        assert(Near(reading.current_A, -0.1f));
        assert(Near(reading.power_W, -0.4f));
        assert(Near(reading.sourcePower_W, -0.3999f));
    }
    assert(driver.setCurrentSign(INAChannel::Battery, -1));
    assert(Near(driver.readBattery().current_A, 0.1f));
    assert(driver.setPowerSave(INAChannel::Battery, true));
    assert(driver.readBattery().status == INAStatus::PowerDown);
    assert(driver.setPowerSave(INAChannel::Battery, false));
    assert(driver.readBattery().valid);

    // Falha parcial não destrói os outros canais; novas causas são adicionais.
    wire.present[0x41] = false;
    assert(!driver.begin(wire));
    assert(driver.readSolar().valid && driver.readGeneral().valid);
    assert(driver.readBattery().status == INAStatus::DeviceOffline);
    assert(!driver.begin(wire, INAChannel::Battery));
    auto diagnostic = driver.GetI2cDiagnostic();
    assert(diagnostic.failed && diagnostic.phase == I2C_PHASE::WRITE && diagnostic.status == 2);
    assert(!wire.held);
    wire.present[0x41] = true;
    assert(driver.begin(wire, INAChannel::Battery));

    wire.stop_error = 5;
    assert(!driver.reinitialize(INAChannel::Solar));
    diagnostic = driver.GetI2cDiagnostic();
    assert(diagnostic.status == 5 && diagnostic.failed && !wire.held);
    assert(driver.begin(wire, INAChannel::Solar));
    wire.read_bytes = 1;
    assert(driver.readSolar().status == INAStatus::I2CError);
    diagnostic = driver.GetI2cDiagnostic();
    assert(diagnostic.phase == I2C_PHASE::READ && diagnostic.received == 1);
    assert(diagnostic.failed && diagnostic.status == 0 && !wire.held && wire.available() == 0);
    assert(driver.readGeneral().valid);
    assert(driver.begin(wire, INAChannel::Solar));
    wire.select_error = 4;
    assert(driver.readSolar().status == INAStatus::I2CError);
    diagnostic = driver.GetI2cDiagnostic();
    assert(diagnostic.phase == I2C_PHASE::SELECT_REGISTER && diagnostic.status == 4 && !wire.held);
    assert(driver.begin(wire, INAChannel::Solar));
    wire.fail_write = true;
    assert(driver.readSolar().status == INAStatus::I2CError);
    assert(driver.GetI2cDiagnostic().phase == I2C_PHASE::BUFFER && !wire.held);
    assert(driver.begin(wire, INAChannel::Solar));
    wire.registers[0x40][2] |= 1;
    assert(driver.readSolar().status == INAStatus::MathOverflow);
    assert(!driver.readSolar().valid && driver.isOnline(INAChannel::Solar));
    wire.registers[0x40][2] &= static_cast<uint16_t>(~3U);
    const auto not_ready = driver.readSolar();
    assert(not_ready.valid && !not_ready.conversionReady); // Sem alterar a API antiga.
    assert(!driver.begin(wire, static_cast<INAChannel>(3)));

    assert(MapWireFailure(false, false, false, 0) == RESULT::OK);
    assert(MapWireFailure(true, false, false, 2) == RESULT::DEVICE_NACK);
    assert(MapWireFailure(true, false, false, 3) == RESULT::DEVICE_NACK);
    assert(MapWireFailure(true, false, false, 5) == RESULT::TRANSFER_TIMEOUT);
    assert(MapWireFailure(true, false, false, 4) == RESULT::BUS_ERROR);
    assert(MapWireFailure(true, false, true, 0) == RESULT::READ_INCOMPLETE);
    assert(MapWireFailure(true, true, false, 5) == RESULT::OPERATION_FAILED);

    // Cadência por canal, conversão fora do executor e validade explícita.
    Channel channels[3];
    assert(channels[0].Next(0, true, 0) == ACTION::CONFIGURE);
    channels[0].Complete(ACTION::CONFIGURE, 0, 0, false, true);
    assert(channels[0].Next(1, true, 0) == ACTION::WAIT);
    assert(channels[0].Next(2, true, 0) == ACTION::READ);
    channels[0].Complete(ACTION::READ, 2, 0, true, true);
    assert(IsAvailable(channels[0].GetState(), 2));
    assert(IsAvailable(channels[0].GetState(), 3002));
    assert(!IsAvailable(channels[0].GetState(), 3003));
    assert(channels[0].Next(1001, true, 0) == ACTION::WAIT);
    assert(channels[0].Next(1002, true, 0) == ACTION::READ);
    assert(channels[1].Next(1002, true, 0) == ACTION::CONFIGURE);
    channels[1].Complete(ACTION::CONFIGURE, 1002, 0, false, false);
    assert(channels[1].Next(6001, true, 0) == ACTION::WAIT);
    assert(channels[1].Next(6002, true, 0) == ACTION::CONFIGURE);
    channels[0].Complete(ACTION::READ, 1002, 0, false, true);
    assert(channels[0].GetState().has_valid && !IsAvailable(channels[0].GetState(), 1002));
    assert(channels[0].GetState().last_valid_ms == 2);
    assert(channels[0].Next(1010, true, 1) == ACTION::CONFIGURE);
    assert(!channels[0].GetState().configured);
    channels[0].Complete(ACTION::CONFIGURE, 1010, 1, false, true);
    assert(channels[0].Next(1012, true, 1) == ACTION::READ);
    channels[0].Complete(ACTION::READ, 1012, 1, true, true);
    assert(IsAvailable(channels[0].GetState(), 1012));
    assert(channels[0].Next(1013, false, 1) == ACTION::WAIT);
    assert(!IsAvailable(channels[0].GetState(), 1013));
    assert(channels[0].Next(1014, true, 1) == ACTION::CONFIGURE);
    assert(!channels[2].GetState().has_valid);
    std::puts("INA219: driver real, disponibilidade parcial, erros, cadencia e retomada aprovados");
}
