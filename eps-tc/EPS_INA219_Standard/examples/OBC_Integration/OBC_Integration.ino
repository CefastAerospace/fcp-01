/*
 * Exemplo de integração com o OBC.
 *
 * Este arquivo demonstra apenas a API.
 *
 * O agendamento, a frequência de leitura e a arbitragem do I2C
 * devem ser definidos pelo firmware principal do OBC.
 */

#include <Arduino.h>
#include <Wire.h>
#include <EPS_INA219.h>

using CEFAST::EPS::EPS_INA219;
using CEFAST::EPS::INAChannel;
using CEFAST::EPS::INAReading;

EPS_INA219 epsIna;

void printReading(const INAReading& r) {
    Serial.print(EPS_INA219::channelName(r.channel));
    Serial.print(" | status=");
    Serial.print(EPS_INA219::statusName(r.status));

    if (!r.valid) {
        Serial.println();
        return;
    }

    Serial.print(" | Vbus=");
    Serial.print(r.busVoltage_V, 3);

    Serial.print(" V | Vin+=");
    Serial.print(r.sourceVoltage_V, 3);

    Serial.print(" V | I=");
    Serial.print(r.current_A, 4);

    Serial.print(" A | Pbus=");
    Serial.print(r.power_W, 3);

    Serial.print(" W | Psource=");
    Serial.print(r.sourcePower_W, 3);

    Serial.println(" W");
}

void setup() {
    Serial.begin(115200);

    /*
     * O OBC inicializa o barramento.
     *
     * Os três INA219 e os demais dispositivos I2C compartilham
     * SDA/SCL. O módulo EPS_INA219 não escolhe os pinos.
     */
    Wire.begin(21, 22);

    const bool allOnline =
        epsIna.begin(Wire);

    Serial.print("INA219 init: ");
    Serial.println(allOnline ? "3/3 OK" : "verificar canais");

    /*
     * Exemplo de convenção:
     *
     * Se a orientação física do INA da bateria exigir inversão
     * para que descarga seja positiva:
     *
     * epsIna.setCurrentSign(INAChannel::Battery, -1);
     */
}

void loop() {
    /*
     * SOMENTE DEMONSTRAÇÃO.
     *
     * No firmware real, o OBC chamará estas funções dentro de
     * sua própria política de escalonamento/modos de missão.
     */

    INAReading solar =
        epsIna.readSolar();

    INAReading battery =
        epsIna.readBattery();

    INAReading general =
        epsIna.readGeneral();

    printReading(solar);
    printReading(battery);
    printReading(general);

    Serial.println();

    delay(1000);
}

/*
 * Exemplo de economia de energia controlada pelo OBC:
 *
 * epsIna.setPowerSaveAll(true);
 *
 * ... OBC permanece em modo econômico ...
 *
 * epsIna.setPowerSaveAll(false);
 *
 * // Após acordar, aguardar uma nova conversão.
 * delayMicroseconds(EPS_INA219::NOMINAL_CONVERSION_TIME_US + 100);
 *
 * INAReading battery = epsIna.readBattery();
 */
