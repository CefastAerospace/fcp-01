# Pinout e alocação de barramentos: FCP-01

Registro oficial de pinos, barramentos SPI, I2C e endereços — espelha a §3.1 do Design Package (Oct/2026) e `obc-dh/headers/Pinout.h`.
Atualizar aqui antes de integrar qualquer hardware novo. Divergências antigas (SPI invertido, Payload por I2C, RTC externo, `0x80`) estão obsoletas — ver histórico no git.

## UART

| Pinos ESP32 | Uso | Observação |
|---|---|---|
| GPIO 1 (TX0) / 3 (RX0) | UART0, umbilical USB-C de bancada | TC/TM nível básico (§7.1) |
| GPIO 16 (RX) / 17 (TX), 115,2 kbps 8N1 | UART dedicada OBC ↔ Payload (Raspberry Pi) | Pacotes binários ADS-B com Sequence ID + NACK (§6.4); ~11,5 kB/s sustentados |

O Payload **não** está no barramento I2C.

## Barramentos SPI (ESP32)

| Barramento | Pinos | Dispositivo | Subsistema |
|---|---|---|---|
| VSPI | MOSI 23 / MISO 19 / SCK 18, CS 5, DIO0 4, RST 14 | LoRa SX1276 (921 MHz) | TT&C |
| HSPI | MOSI 32 / MISO 35 / SCK 15, CS 13 | microSD (missão + logs) | OBC&DH |

LoRa e SD em barramentos separados para não colidirem. MISO do SD no GPIO 35 (somente entrada) por segurança de boot. DIO0 sinaliza RX/TX Done; RST permite reset físico do rádio.

## Barramento I2C (compartilhado, sensores)

SDA = GPIO 21, SCL = GPIO 22. Endereços em 7 bits:

| Endereço | Dispositivo | Subsistema | Observação |
|---|---|---|---|
| 0x68 | MPU-9250 (IMU, AD0 em GND) | ADCS | Taxa p/ detumbling; magnetômetro não usado no controle |
| 0x36 | AS5600 (encoder do motor) | ADCS | Feedback FOC da roda de reação |
| 0x23 | BH1750 #1 (ADDR em GND) | ADCS | Par diferencial p/ azimute solar (com baffles) |
| 0x5C | BH1750 #2 (ADDR em VCC) | ADCS | Par diferencial p/ azimute solar (com baffles) |
| 3x INA219 | Monitor bateria / rail principal / painéis | EPS | Tensão, corrente e potência p/ OBC |
| 5x LM75A | Sensores de temperatura | Thermal | Bateria + pontos críticos; watchdog via OBC |

Sem RTC externo no barramento (tempo = timer interno ESP32, §6.4). Sem endereço `0x80` (fora da faixa 7 bits; era proposta antiga do Payload, descartada com a migração para UART).

## Atuação

| Recurso | Pino | Subsistema | Observação |
|---|---|---|---|
| SimpleFOC Mini fases U/V/W | GPIO 25 / 26 / 27 (PWM) | ADCS | Motor GM2804 da roda de reação |
| Driver EN | GPIO 12 | ADCS | Desliga o driver fora do detumbling (zera idle) |
| Burn-wire antenas | GPIO 33 (saída, trigger compartilhado) | Estruturas / EPS / OBC-DH | 2 circuitos nichrome (5 V, 4 Ω cada; 2,5 A / 12,5 W transitório); acionar fora de TX |

GPIO 34/36/39 livres (somente entrada, sem pulls internos) para expansões futuras.

## Referências

- `FCP-01_Design_Package_CubeDesign_2026.md` §3.1 (pinagem), §6.4 (UART + timestamps), §7.1 (rádio/SD)
- `obc-dh/headers/Pinout.h` (constantes `pinout::`)
- `obc-dh/.agents/PINAGEM.md` (notas de integração/strapping pins)
