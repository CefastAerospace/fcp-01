# Pinout e alocação de barramentos — FCP-01

Registro compartilhado de endereços, barramentos e pinos. Consolida a alocação de projeto registrada no remoto com referência à §3.1 do Design Package e às constantes de `obc-dh/headers/Pinout.h`. Restrições elétricas: [PINAGEM.md](../../obc-dh/.agents/PINAGEM.md). Definições abertas e evidências: [TODO.md](../../obc-dh/.agents/TODO.md). Decisão de projeto, proposta de endereço e montagem validada são estados distintos.

## UART

| Pinos ESP32 | Uso | Observação |
|---|---|---|
| TX0 = 1 / RX0 = 3 | UART0, umbilical USB-C de bancada | TC/TM JSON, um objeto por linha; contrato TT&C §7.1. |
| RX = 16 / TX = 17, 115,2 kbps 8N1 | UART dedicada OBC ↔ Raspberry Pi | Binário ADS-B compacto com SeqID + NACK (§6.4); capacidade serial teórica de 11,52 kB/s antes do protocolo. |

O Payload não participa do I2C. Confirmar UART/porta no Pi, TX/RX cruzados, GND comum, níveis de 3,3 V, disponibilidade de 16/17 no módulo/flash/PSRAM e comportamento com um lado desligado. Parâmetros de projeto não comprovam o transporte no script nem validação da montagem; consultar [COM-004](../../obc-dh/.tasks/01-comunicacao/COM-004-uart-payload.md).

## SPI

| Barramento | Pinos ESP32 | Dispositivo | Subsistema |
|---|---|---|---|
| VSPI | MOSI 23 / MISO 19 / SCK 18, CS 5, DIO0 4, RST 14 | LoRa SX1276, 921 MHz | TT&C |
| HSPI | MOSI 32 / MISO 35 / SCK 15, CS 13 | microSD | OBC-DH |

O mapa anterior VSPI/SD e HSPI/LoRa foi substituído como decisão de projeto. Conferência elétrica, montagem e validação permanecem em [COM-005](../../obc-dh/.tasks/01-comunicacao/COM-005-armazenamento-sd.md). O rádio usa `SPI` global/VSPI; o SD deverá utilizar instância própria de HSPI. DIO0 sinaliza RX/TX Done e RST permite reset físico; MISO do SD usa GPIO 35, somente entrada, por segurança de boot.

## I2C de sensores e encoder

SDA = GPIO 21; SCL = GPIO 22. Endereços em sete bits:

| Endereço de 7 bits | Dispositivo | Subsistema | Estado |
|---|---|---|---|
| `0x68` | MPU9250 | ADCS | Confirmado; AD0 em GND. |
| `0x23` | BH1750 #1 | ADCS | Confirmado; ADDR em GND. |
| `0x5C` | BH1750 #2 | ADCS | Confirmado; ADDR em VCC. |
| `0x36` | AS5600 | ADCS | Encoder definido; conferir montagem e aquisição coordenada. |
| A confirmar | 3x INA219 | EPS | Bateria, rail principal e painéis. `0x40`/`0x41` são propostas anteriores, não mapa final dos três módulos; confirmar A0/A1 com EPS. |
| A confirmar | 5x LM75A | Térmico | Bateria e pontos críticos. `0x48` é proposta anterior, não mapa final dos cinco módulos; confirmar A0–A2 e localização. |
| A definir, se houver interface | BMS | EPS | Conferir aquisição prevista com EPS; não presumir endereço I2C. |
| Fora da configuração atual | RTC externo | OBC-DH | DS3231 histórico; reconsiderar apenas se o método de tempo monotônico/referências LoRa não atender à validação. Seu `0x68` conflita com a IMU no mesmo barramento. |

Conferir [INT-005](../../obc-dh/.tasks/03-integracao/INT-005-sensores-e-adcs.md), [COM-003](../../obc-dh/.tasks/01-comunicacao/COM-003-recuperacao-i2c.md) e [DAD-002](../../obc-dh/.tasks/02-dados-e-tempo/DAD-002-tempo-e-boots.md). Não alterar endereços confirmados por suposição. O Payload utiliza UART e não recebe endereço I2C; a antiga proposta `0x80` e a decisão substituída constam do [histórico](../../obc-dh/.agents/REQUISITOS.md#histórico-de-decisões-de-interface).

## Atuação e restrições

| Recurso | GPIOs / ligação | Estado |
|---|---|---|
| Motor — ADCS | EN = 12; U/V/W = 25/26/27 | ADCS no mesmo ESP32; EN confirmado, fases/encoder/estados ainda dependem de [INT-005](../../obc-dh/.tasks/03-integracao/INT-005-sensores-e-adcs.md). |
| Burn Wire das antenas | Trigger = 33, MOSFET compartilhado | Dois circuitos nichrome, referência remota de 5 V / 4 Ω cada, 2,5 A / 12,5 W transitório; acionar fora de TX. Confirmar autorização, duração, montagem e estados em reset com os responsáveis. |

Detalhes em [INT-005](../../obc-dh/.tasks/03-integracao/INT-005-sensores-e-adcs.md) e [INT-007](../../obc-dh/.tasks/03-integracao/INT-007-comandos-modos-e-protecoes.md). Motor GM2804 com SimpleFOC Mini: desabilitar fora do detumbling é a decisão de operação; consumo residual precisa de medição. EN/fases do código de bancada ainda divergem da alocação acima.

GPIOs 5, 12 e 15 são strapping pins; conferir níveis durante boot/reset. GPIO 35 não tem pulls internos. GPIOs 34/36/39 estão sem alocação no mapa e são somente entrada, sem pulls internos; confirmar disponibilidade física antes de reservá-los. Burn Wire substitui menções históricas a servo.

## Manutenção

Atualizar a referência antes de integrar novo dispositivo ou ligação, com decisão dos responsáveis e ICD correspondente. Registrar decisão e evidência no checklist; conservar pendências de montagem e validação sem reabrir decisões de interface já tomadas. Sincronizar os mapas sem presumir alteração física ou teste concluído.

## Referências

- Design Package §3.1 (pinagem), §6.4 (UART e tempo), §7.1 (rádio/umbilical), conforme referências dos setores no remoto; arquivo Markdown citado não disponível neste checkout.
- [Pinout.h](../../obc-dh/headers/Pinout.h): constantes, sem inicialização de hardware.
- [Contrato JSON TT&C](../protocol/ttc-json.md): protocolo distinto do binário UART do Payload.
- [EPS/Térmico](../../eps-tc/README.md) e [ADCS](../../adcs/README.md): decisões e contratos dos responsáveis.
