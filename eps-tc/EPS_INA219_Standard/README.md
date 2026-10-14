# EPS_INA219 — CEFAST Aerospace

API de aquisição dos três sensores **INA219** do subsistema EPS.

Esta versão usa **calibração padrão nominal em todos os três sensores**.  
Não contém a calibração experimental do INA Solar.

## Arquitetura

| Canal | Endereço proposto | Posição |
|---|---:|---|
| Solar | `0x40` | Painéis → INA219 → CN3722/MPPT |
| Bateria | `0x41` | Caminho comum de carga/descarga da bateria |
| Geral | `0x44` | RBF → INA219 → reguladores/cargas |

Os três sensores compartilham o mesmo barramento I2C do OBC.

No ESP32 DevKit V1 atualmente definido no projeto:

- SDA: GPIO 21
- SCL: GPIO 22

A **inicialização e a arbitragem do I2C pertencem ao OBC**, não a esta biblioteca.

## Objetivo da API

O módulo não define frequência de leitura.

O OBC escolhe quando realizar cada aquisição:

```cpp
INAReading solar = epsIna.readSolar();
INAReading battery = epsIna.readBattery();
INAReading general = epsIna.readGeneral();
```

Isso permite que a política de aquisição seja alterada de acordo com:

- modo nominal;
- economia de energia;
- hibernação;
- diagnóstico;
- prioridades de outros dispositivos no mesmo I2C.

## Calibração padrão

Os três canais utilizam:

- shunt nominal: `R100 = 0,1 ohm`
- `Current_LSB = 100 uA/bit`
- `Calibration Register = 4096`
- `Power_LSB = 2 mW/bit`
- PGA: `±320 mV`
- ADC de bus: 12 bits
- ADC de shunt: 12 bits

O Configuration Register usado em operação contínua é `0x399F`.

A indicação **32 V** representa a faixa de escala configurada no registrador.
O limite físico do INA219 permanece em **26 V**.

## Inicialização

```cpp
#include <Wire.h>
#include <EPS_INA219.h>

using CEFAST::EPS::EPS_INA219;

EPS_INA219 epsIna;

void setup() {
    Wire.begin(21, 22);

    bool allOnline = epsIna.begin(Wire);
}
```

`begin()` retorna `true` somente se os três INA219 responderem.

Mesmo que um deles falhe, os demais permanecem utilizáveis.

## Leitura individual

```cpp
INAReading solar = epsIna.readSolar();

if (solar.valid) {
    float voltage = solar.sourceVoltage_V;
    float current = solar.current_A;
    float power = solar.sourcePower_W;
}
```

## Campos retornados

`INAReading` fornece:

- `busVoltage_V`: tensão em VIN-
- `sourceVoltage_V`: tensão calculada em VIN+
- `shuntVoltage_mV`: queda no shunt
- `current_A`: corrente com sinal
- `power_W`: potência no lado VIN-
- `sourcePower_W`: potência no lado VIN+
- `conversionReady`: bit CNVR
- `overflow`: bit OVF
- `status`: estado da operação
- `valid`: indica leitura válida

## Leitura dos três canais

```cpp
INAReading readings[3];

uint8_t validCount =
    epsIna.readAll(readings);
```

O OBC pode preferir leituras individuais para controlar melhor o uso do barramento.

## Convenção de sinal

Por padrão:

```text
VIN+ -> VIN- = corrente positiva
```

Para inverter um canal:

```cpp
epsIna.setCurrentSign(
    INAChannel::Battery,
    -1
);
```

Isso permite que a equipe defina posteriormente, por exemplo:

```text
descarga da bateria = positiva
carga da bateria    = negativa
```

sem alterar a lógica de aquisição.

## Power-Down

O OBC pode colocar sensores em baixo consumo:

```cpp
epsIna.setPowerSave(
    INAChannel::Solar,
    true
);
```

Ou os três:

```cpp
epsIna.setPowerSaveAll(true);
```

Para acordar:

```cpp
epsIna.setPowerSaveAll(false);
```

Depois de acordar, o OBC deve esperar uma nova conversão antes de consumir a leitura.

A configuração usada realiza aproximadamente:

```text
532 us shunt + 532 us bus = 1064 us
```

A constante:

```cpp
EPS_INA219::NOMINAL_CONVERSION_TIME_US
```

fornece essa referência.

## Recuperação de falha

Uma falha I2C marca apenas aquele canal como offline.

O OBC decide quando tentar recuperar:

```cpp
epsIna.reinitialize(
    INAChannel::Battery
);
```

Não há tentativa automática nem timer interno.

## Concorrência / FreeRTOS

A API não cria mutex para o barramento I2C.

Se mais de uma task acessar `Wire`, a proteção do barramento deve ser feita
pela camada de I2C do OBC.

Essa decisão é intencional: o OBC é o único componente com visão global de
todos os dispositivos do barramento.

## Dependências

- Arduino framework
- `Wire`

Esta implementação fala diretamente com os registradores do INA219.
Ela não depende de `Adafruit_INA219`, evitando que o módulo reinicialize
o barramento I2C internamente.

## Próxima versão

Uma versão separada poderá aplicar os coeficientes de calibração experimental
obtidos em bancada, mantendo a mesma interface pública para o OBC.
