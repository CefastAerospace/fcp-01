# OBC-DH — On-Board Computer & Data Handling

Núcleo de processamento do FCP-01: um ESP32 DevKit V1 coordena todos os subsistemas via FreeRTOS e organiza comandos, telemetria e registros da missão (§§3.4, 7.2).

## Barramentos (resumo — oficial em `../common/interfaces/pinout.md`)

| Interface | Pinos | Uso |
|---|---|---|
| UART0 | 1 / 3 | Umbilical USB-C de bancada |
| UART payload | 16 / 17, 115,2 kbps | Binário ADS-B + SeqID/NACK (§6.4) |
| I2C sensores | 21 / 22 | INA219, LM75A, MPU-9250, AS5600, BH1750 |
| VSPI | 23/19/18, CS 5, DIO0 4, RST 14 | LoRa SX1276 |
| HSPI | 32/35/15, CS 13 | microSD |
| PWM + EN | 25/26/27, EN 12 | Roda de reação |
| Digital | 33 | Burn-wire das antenas |

`headers/Pinout.h` espelha essa tabela em `constexpr int` (não inicializa hardware). Sem RTC externo: tempo = timer interno + TC (§6.4).

## Contratos

- **Payload → OBC:** só dado decodificado em binário; OBC carimba (ms), armazena no SD e prepara TM (§§6.4, 7.3).
- **OBC ↔ TT&C:** filas `filaTC` (TC validado) e `filaTM` (telemetria JSON ~150 B) — picos de um lado não travam o outro (§7.1–7.2).
- **Modos:** ver ConOps no `../README.md` (detalhe em §2.5 do Design Package).

## Build

Abrir `obc-dh/` no PlatformIO, compilar o env `obc` (`esp32doit-devkit-v1`, Arduino). Deps de terceiros em `lib_deps` com versão fixada.

## Aprofundar

- `.agents/` — arquitetura, pinagem, pendências e checklist do OBC-DH.
- `../FCP-01_Design_Package_CubeDesign_2026.md` §§3.1, 3.4, 7.2, 7.3 + Apêndice A (REQ-OBC).
