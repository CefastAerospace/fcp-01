#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace CEFAST {
namespace EPS {

/**
 * Canais de monitoramento de energia do EPS.
 *
 * A ordem dos valores é usada internamente como índice.
 */
enum class INAChannel : uint8_t {
    Solar = 0,
    Battery = 1,
    General = 2
};

/**
 * Estado retornado por uma leitura/operação.
 */
enum class INAStatus : uint8_t {
    Ok = 0,
    NotInitialized,
    InvalidChannel,
    DeviceOffline,
    PowerDown,
    I2CError,
    MathOverflow
};

/**
 * Dados retornados por uma leitura.
 *
 * Observação:
 * - busVoltage_V é a tensão em VIN- em relação ao GND.
 * - sourceVoltage_V é a tensão aproximada em VIN+:
 *      VIN+ = Vbus + Vshunt
 * - power_W usa Vbus * I e representa a potência no lado VIN-.
 * - sourcePower_W usa VIN+ * I e representa a potência no lado VIN+.
 *
 * O sinal da corrente pode ser invertido por canal com setCurrentSign().
 */
struct INAReading {
    INAChannel channel = INAChannel::Solar;
    INAStatus status = INAStatus::NotInitialized;

    uint8_t address = 0;

    bool valid = false;
    bool conversionReady = false;
    bool overflow = false;

    float busVoltage_V = 0.0f;
    float sourceVoltage_V = 0.0f;
    float shuntVoltage_mV = 0.0f;

    float current_A = 0.0f;

    float power_W = 0.0f;
    float sourcePower_W = 0.0f;
};

/**
 * Driver/API dos três INA219 do EPS.
 *
 * IMPORTANTE:
 * - Esta classe NÃO controla frequência de leitura.
 * - Esta classe NÃO possui loop próprio.
 * - Esta classe NÃO chama Wire.begin().
 * - O OBC é responsável por inicializar e arbitrar o barramento I2C.
 * - O OBC decide quando read(), readSolar(), readBattery() ou readGeneral()
 *   serão chamados.
 *
 * Todos os três canais usam a configuração padrão R100 / 32V / 2A:
 *
 *   Rshunt       = 0,1 ohm
 *   Current_LSB  = 100 uA/bit
 *   CAL          = 4096
 *   Power_LSB    = 2 mW/bit
 *   PGA          = +/-320 mV
 *   Bus ADC      = 12 bits
 *   Shunt ADC    = 12 bits
 *
 * A faixa "32 V" é a escala do ADC. O limite físico do INA219 continua
 * sendo 26 V.
 */
class EPS_INA219 {
public:
    // Endereços I2C propostos para os três sensores.
    static constexpr uint8_t SOLAR_ADDRESS   = 0x40;
    static constexpr uint8_t BATTERY_ADDRESS = 0x41;
    static constexpr uint8_t GENERAL_ADDRESS = 0x44;

    // Calibração padrão para shunt R100 = 0,1 ohm.
    static constexpr float SHUNT_RESISTANCE_OHM = 0.1f;
    static constexpr float CURRENT_LSB_A = 0.0001f;      // 100 uA/bit
    static constexpr float POWER_LSB_W = 0.002f;         // 2 mW/bit
    static constexpr float SHUNT_LSB_MV = 0.01f;         // 10 uV/bit
    static constexpr float BUS_LSB_V = 0.004f;           // 4 mV/bit
    static constexpr uint16_t CALIBRATION_VALUE = 4096;  // 0x1000

    /**
     * Tempo nominal aproximado de uma conversão completa de shunt + bus
     * usando 12 bits sem averaging:
     *
     * 532 us (shunt) + 532 us (bus) ~= 1064 us.
     *
     * Para acordar do Power-Down e imediatamente fazer uma nova leitura,
     * o OBC deve prever margem acima deste valor.
     */
    static constexpr uint32_t NOMINAL_CONVERSION_TIME_US = 1064;

    EPS_INA219();

    /**
     * Associa o módulo a um barramento I2C já inicializado pelo OBC e
     * configura os três INA219.
     *
     * Exemplo no ESP32:
     *   Wire.begin(21, 22);
     *   epsIna.begin(Wire);
     *
     * Retorna true apenas se os três sensores forem configurados com sucesso.
     * Mesmo se retornar false, os sensores encontrados continuam disponíveis
     * individualmente e podem ser consultados com isOnline().
     */
    bool begin(TwoWire& wire);

    /**
     * Reconfigura apenas um sensor usando a calibração padrão.
     *
     * Útil para recuperação de falha decidida pelo OBC.
     */
    bool reinitialize(INAChannel channel);

    /**
     * Informa se o canal foi inicializado e está marcado como online.
     */
    bool isOnline(INAChannel channel) const;

    /**
     * Retorna o endereço I2C associado ao canal.
     */
    uint8_t address(INAChannel channel) const;

    /**
     * Lê um canal sob demanda.
     *
     * Não há temporizador interno: cada chamada realiza uma aquisição
     * imediatamente no barramento I2C.
     */
    INAReading read(INAChannel channel);

    // Atalhos para os três canais.
    INAReading readSolar();
    INAReading readBattery();
    INAReading readGeneral();

    /**
     * Lê os três canais sequencialmente.
     *
     * Retorna a quantidade de leituras válidas (0 a 3).
     *
     * OBCs que precisam de controle fino do barramento podem preferir
     * chamar read() individualmente.
     */
    uint8_t readAll(INAReading (&readings)[3]);

    /**
     * Coloca/retira um canal do modo Power-Down.
     *
     * Esta função apenas executa a ordem solicitada.
     * A política de quando dormir/acordar pertence ao OBC.
     *
     * Ao sair de Power-Down, aguarde uma nova conversão antes de consumir
     * a leitura atual. NOMINAL_CONVERSION_TIME_US fornece a referência.
     */
    bool setPowerSave(INAChannel channel, bool enabled);

    /**
     * Aplica Power-Down/ativo aos três sensores.
     *
     * Retorna true somente se todos responderem.
     */
    bool setPowerSaveAll(bool enabled);

    /**
     * Define a convenção de sinal da corrente de um canal.
     *
     * sign >= 0 -> VIN+ para VIN- é positivo.
     * sign < 0  -> sinal é invertido.
     *
     * Isso é particularmente útil no INA da bateria para o OBC decidir
     * se carga ou descarga será representada como corrente positiva.
     */
    bool setCurrentSign(INAChannel channel, int8_t sign);

    /**
     * Retorna +1 ou -1 conforme a convenção atual.
     */
    int8_t currentSign(INAChannel channel) const;

    /**
     * Nome curto do canal para logs/telemetria do OBC.
     */
    static const char* channelName(INAChannel channel);

    /**
     * Texto curto do status para debug/log.
     */
    static const char* statusName(INAStatus status);

private:
    static constexpr uint8_t SENSOR_COUNT = 3;

    // Registradores do INA219.
    static constexpr uint8_t REG_CONFIG      = 0x00;
    static constexpr uint8_t REG_SHUNT       = 0x01;
    static constexpr uint8_t REG_BUS         = 0x02;
    static constexpr uint8_t REG_CURRENT     = 0x04;
    static constexpr uint8_t REG_CALIBRATION = 0x05;

    /**
     * Configuração equivalente ao perfil padrão 32V / 2A:
     *
     * BRNG = 1        -> faixa de escala 32 V
     * PGA  = /8       -> +/-320 mV no shunt
     * BADC = 12 bits
     * SADC = 12 bits / 1 amostra
     * MODE = 111      -> shunt + bus contínuos
     */
    static constexpr uint16_t CONFIG_STANDARD_CONTINUOUS = 0x399F;

    /**
     * Mesma configuração, porém MODE = 000 (Power-Down).
     */
    static constexpr uint16_t CONFIG_STANDARD_POWERDOWN = 0x3998;

    struct SensorState {
        uint8_t address;
        bool online;
        bool powerDown;
        int8_t currentSign;
    };

    TwoWire* _wire;
    bool _initialized;

    SensorState _sensors[SENSOR_COUNT];

    bool validChannel(INAChannel channel) const;
    uint8_t indexOf(INAChannel channel) const;

    bool configureStandard(INAChannel channel);

    bool writeRegister(uint8_t address, uint8_t reg, uint16_t value);
    bool readRegister(uint8_t address, uint8_t reg, uint16_t& value);

    void markOffline(INAChannel channel);
};

} // namespace EPS
} // namespace CEFAST
