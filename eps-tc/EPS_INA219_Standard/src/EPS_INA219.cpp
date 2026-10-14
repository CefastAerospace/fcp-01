#include "EPS_INA219.h"

#include <math.h>

namespace CEFAST {
namespace EPS {

EPS_INA219::EPS_INA219()
    : _wire(nullptr),
      _initialized(false),
      _sensors{
          {SOLAR_ADDRESS, false, false, +1},
          {BATTERY_ADDRESS, false, false, +1},
          {GENERAL_ADDRESS, false, false, +1}
      } {
}

bool EPS_INA219::begin(TwoWire& wire) {
    /*
     * O barramento pertence ao OBC.
     *
     * Por isso NÃO chamamos Wire.begin() aqui.
     * O OBC deve inicializar SDA/SCL antes desta função.
     */
    _wire = &wire;
    _initialized = true;

    bool allOk = true;

    for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
        const bool ok = reinitialize(static_cast<INAChannel>(i));
        allOk = allOk && ok;
    }

    return allOk;
}

bool EPS_INA219::reinitialize(INAChannel channel) {
    if (!_initialized || _wire == nullptr || !validChannel(channel)) {
        return false;
    }

    const bool ok = configureStandard(channel);

    const uint8_t index = indexOf(channel);
    _sensors[index].online = ok;
    _sensors[index].powerDown = false;

    return ok;
}

bool EPS_INA219::isOnline(INAChannel channel) const {
    if (!validChannel(channel)) {
        return false;
    }

    return _sensors[indexOf(channel)].online;
}

uint8_t EPS_INA219::address(INAChannel channel) const {
    if (!validChannel(channel)) {
        return 0;
    }

    return _sensors[indexOf(channel)].address;
}

INAReading EPS_INA219::read(INAChannel channel) {
    INAReading out;

    out.channel = channel;

    if (!validChannel(channel)) {
        out.status = INAStatus::InvalidChannel;
        return out;
    }

    const uint8_t index = indexOf(channel);
    SensorState& sensor = _sensors[index];

    out.address = sensor.address;

    if (!_initialized || _wire == nullptr) {
        out.status = INAStatus::NotInitialized;
        return out;
    }

    if (!sensor.online) {
        out.status = INAStatus::DeviceOffline;
        return out;
    }

    if (sensor.powerDown) {
        /*
         * Em Power-Down os registradores mantêm a última conversão.
         * Para evitar que o OBC trate dado antigo como leitura nova,
         * retornamos explicitamente PowerDown.
         */
        out.status = INAStatus::PowerDown;
        return out;
    }

    uint16_t busRaw = 0;
    uint16_t shuntRawUnsigned = 0;
    uint16_t currentRawUnsigned = 0;

    // --------------------------------------------------------
    // BUS VOLTAGE
    // --------------------------------------------------------
    if (!readRegister(sensor.address, REG_BUS, busRaw)) {
        markOffline(channel);
        out.status = INAStatus::I2CError;
        return out;
    }

    /*
     * Bus Voltage Register:
     * bits 15:3 = tensão
     * bit 1     = Conversion Ready (CNVR)
     * bit 0     = Math Overflow (OVF)
     *
     * Cada bit útil de tensão representa 4 mV.
     */
    out.conversionReady = (busRaw & 0x0002U) != 0;
    out.overflow = (busRaw & 0x0001U) != 0;

    const uint16_t busCounts = (busRaw >> 3);
    out.busVoltage_V = static_cast<float>(busCounts) * BUS_LSB_V;

    // --------------------------------------------------------
    // SHUNT VOLTAGE
    // --------------------------------------------------------
    if (!readRegister(sensor.address, REG_SHUNT, shuntRawUnsigned)) {
        markOffline(channel);
        out.status = INAStatus::I2CError;
        return out;
    }

    const int16_t shuntRaw = static_cast<int16_t>(shuntRawUnsigned);

    /*
     * Shunt Voltage LSB = 10 uV = 0,01 mV.
     */
    out.shuntVoltage_mV =
        static_cast<float>(shuntRaw) * SHUNT_LSB_MV;

    // --------------------------------------------------------
    // CURRENT
    // --------------------------------------------------------

    /*
     * Reescrevemos CAL antes de ler CURRENT.
     *
     * Se o INA219 sofrer reset, o Calibration Register volta ao valor
     * de power-on e CURRENT deixa de representar corrente corretamente.
     * Reaplicar CAL torna a leitura mais robusta.
     */
    if (!writeRegister(
            sensor.address,
            REG_CALIBRATION,
            CALIBRATION_VALUE)) {
        markOffline(channel);
        out.status = INAStatus::I2CError;
        return out;
    }

    if (!readRegister(
            sensor.address,
            REG_CURRENT,
            currentRawUnsigned)) {
        markOffline(channel);
        out.status = INAStatus::I2CError;
        return out;
    }

    const int16_t currentRaw =
        static_cast<int16_t>(currentRawUnsigned);

    /*
     * Current_LSB = 100 uA/bit = 0,0001 A/bit.
     */
    out.current_A =
        static_cast<float>(currentRaw) *
        CURRENT_LSB_A *
        static_cast<float>(sensor.currentSign);

    // --------------------------------------------------------
    // SOURCE VOLTAGE
    // --------------------------------------------------------

    /*
     * O INA219 mede Vbus em VIN-.
     *
     * Vshunt = VIN+ - VIN-
     *
     * Portanto:
     *   VIN+ = Vbus + Vshunt
     */
    out.sourceVoltage_V =
        out.busVoltage_V +
        (out.shuntVoltage_mV / 1000.0f);

    // --------------------------------------------------------
    // POWER
    // --------------------------------------------------------

    /*
     * Para telemetria bidirecional, calculamos potência com o sinal
     * da corrente.
     *
     * power_W:
     *   potência no lado VIN- do shunt.
     *
     * sourcePower_W:
     *   potência no lado VIN+ do shunt.
     *
     * A diferença entre elas corresponde aproximadamente à perda
     * dissipada no shunt.
     */
    out.power_W =
        out.busVoltage_V *
        out.current_A;

    out.sourcePower_W =
        out.sourceVoltage_V *
        out.current_A;

    if (!isfinite(out.busVoltage_V) ||
        !isfinite(out.sourceVoltage_V) ||
        !isfinite(out.shuntVoltage_mV) ||
        !isfinite(out.current_A) ||
        !isfinite(out.power_W) ||
        !isfinite(out.sourcePower_W)) {

        markOffline(channel);
        out.status = INAStatus::I2CError;
        return out;
    }

    /*
     * O datasheet informa que, com OVF ativo, os cálculos de corrente
     * e potência podem não ser válidos.
     */
    if (out.overflow) {
        out.status = INAStatus::MathOverflow;
        out.valid = false;
        return out;
    }

    out.status = INAStatus::Ok;
    out.valid = true;

    return out;
}

INAReading EPS_INA219::readSolar() {
    return read(INAChannel::Solar);
}

INAReading EPS_INA219::readBattery() {
    return read(INAChannel::Battery);
}

INAReading EPS_INA219::readGeneral() {
    return read(INAChannel::General);
}

uint8_t EPS_INA219::readAll(INAReading (&readings)[3]) {
    readings[0] = read(INAChannel::Solar);
    readings[1] = read(INAChannel::Battery);
    readings[2] = read(INAChannel::General);

    uint8_t validCount = 0;

    for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
        if (readings[i].valid) {
            ++validCount;
        }
    }

    return validCount;
}

bool EPS_INA219::setPowerSave(
    INAChannel channel,
    bool enabled
) {
    if (!_initialized || _wire == nullptr || !validChannel(channel)) {
        return false;
    }

    const uint8_t index = indexOf(channel);
    SensorState& sensor = _sensors[index];

    if (!sensor.online) {
        return false;
    }

    const uint16_t config =
        enabled
            ? CONFIG_STANDARD_POWERDOWN
            : CONFIG_STANDARD_CONTINUOUS;

    /*
     * Ao acordar, garantimos também que CAL esteja correto.
     */
    if (!enabled) {
        if (!writeRegister(
                sensor.address,
                REG_CALIBRATION,
                CALIBRATION_VALUE)) {

            markOffline(channel);
            return false;
        }
    }

    if (!writeRegister(
            sensor.address,
            REG_CONFIG,
            config)) {

        markOffline(channel);
        return false;
    }

    sensor.powerDown = enabled;

    return true;
}

bool EPS_INA219::setPowerSaveAll(bool enabled) {
    bool allOk = true;

    for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
        const bool ok =
            setPowerSave(
                static_cast<INAChannel>(i),
                enabled
            );

        allOk = allOk && ok;
    }

    return allOk;
}

bool EPS_INA219::setCurrentSign(
    INAChannel channel,
    int8_t sign
) {
    if (!validChannel(channel)) {
        return false;
    }

    _sensors[indexOf(channel)].currentSign =
        (sign < 0) ? -1 : +1;

    return true;
}

int8_t EPS_INA219::currentSign(INAChannel channel) const {
    if (!validChannel(channel)) {
        return +1;
    }

    return _sensors[indexOf(channel)].currentSign;
}

const char* EPS_INA219::channelName(INAChannel channel) {
    switch (channel) {
        case INAChannel::Solar:
            return "SOLAR";

        case INAChannel::Battery:
            return "BATTERY";

        case INAChannel::General:
            return "GENERAL";

        default:
            return "UNKNOWN";
    }
}

const char* EPS_INA219::statusName(INAStatus status) {
    switch (status) {
        case INAStatus::Ok:
            return "OK";

        case INAStatus::NotInitialized:
            return "NOT_INITIALIZED";

        case INAStatus::InvalidChannel:
            return "INVALID_CHANNEL";

        case INAStatus::DeviceOffline:
            return "DEVICE_OFFLINE";

        case INAStatus::PowerDown:
            return "POWER_DOWN";

        case INAStatus::I2CError:
            return "I2C_ERROR";

        case INAStatus::MathOverflow:
            return "MATH_OVERFLOW";

        default:
            return "UNKNOWN";
    }
}

bool EPS_INA219::validChannel(INAChannel channel) const {
    return static_cast<uint8_t>(channel) < SENSOR_COUNT;
}

uint8_t EPS_INA219::indexOf(INAChannel channel) const {
    return static_cast<uint8_t>(channel);
}

bool EPS_INA219::configureStandard(INAChannel channel) {
    if (!validChannel(channel)) {
        return false;
    }

    SensorState& sensor =
        _sensors[indexOf(channel)];

    /*
     * CAL = 4096 para:
     *
     *   Current_LSB = 100 uA/bit
     *   Rshunt      = 0,1 ohm
     *
     * CAL = 0,04096 / (Current_LSB * Rshunt)
     */
    if (!writeRegister(
            sensor.address,
            REG_CALIBRATION,
            CALIBRATION_VALUE)) {

        return false;
    }

    /*
     * Configuração padrão:
     * 32 V scale / +/-320 mV / 12-bit / continuous.
     */
    if (!writeRegister(
            sensor.address,
            REG_CONFIG,
            CONFIG_STANDARD_CONTINUOUS)) {

        return false;
    }

    return true;
}

bool EPS_INA219::writeRegister(
    uint8_t address,
    uint8_t reg,
    uint16_t value
) {
    if (_wire == nullptr) {
        return false;
    }

    _wire->beginTransmission(address);

    if (_wire->write(reg) != 1) {
        _wire->endTransmission(true);
        return false;
    }

    if (_wire->write(
            static_cast<uint8_t>(value >> 8)) != 1) {
        _wire->endTransmission(true);
        return false;
    }

    if (_wire->write(
            static_cast<uint8_t>(value & 0xFF)) != 1) {
        _wire->endTransmission(true);
        return false;
    }

    return _wire->endTransmission(true) == 0;
}

bool EPS_INA219::readRegister(
    uint8_t address,
    uint8_t reg,
    uint16_t& value
) {
    if (_wire == nullptr) {
        return false;
    }

    /*
     * Seleciona o registrador sem liberar o barramento
     * (repeated START).
     */
    _wire->beginTransmission(address);

    if (_wire->write(reg) != 1) {
        _wire->endTransmission(true);
        return false;
    }

    if (_wire->endTransmission(false) != 0) {
        return false;
    }

    const uint8_t received =
        _wire->requestFrom(
            static_cast<int>(address),
            2,
            static_cast<int>(true)
        );

    if (received != 2 || _wire->available() < 2) {
        while (_wire->available()) {
            _wire->read();
        }

        return false;
    }

    const uint8_t msb =
        static_cast<uint8_t>(_wire->read());

    const uint8_t lsb =
        static_cast<uint8_t>(_wire->read());

    value =
        (static_cast<uint16_t>(msb) << 8) |
        static_cast<uint16_t>(lsb);

    return true;
}

void EPS_INA219::markOffline(INAChannel channel) {
    if (!validChannel(channel)) {
        return;
    }

    SensorState& sensor =
        _sensors[indexOf(channel)];

    sensor.online = false;
    sensor.powerDown = false;
}

} // namespace EPS
} // namespace CEFAST
