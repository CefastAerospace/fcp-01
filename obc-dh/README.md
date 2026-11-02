# OBC-DH — On-Board Computer & Data Handling

Núcleo de processamento do FCP-01: um ESP32 DevKit V1 coordena todos os subsistemas via FreeRTOS e organiza comandos, telemetria e registros da missão (§§3.4, 7.2).

## Estado local — 01/11/2026

Estão implementados o serviço I2C, os modelos/validadores de dados, a referência temporal por boot, o adaptador timer/NVS para ESP32 e o codec de registros com CRC-32. O [leitor nativo](tools/README.md) interpreta arquivos no computador, exporta JSON Lines, reconstrói horários e relata trechos inválidos sem alterar a origem. Testes nativos e build ESP32 passaram; isso não comprova funcionamento físico ou integração.

DAD-002 está concluída no escopo local. DAD-001 continua em andamento até o aceite do contrato pelos produtores; DAD-003 aguarda essa consolidação e a resolução dos estados duráveis de retomada. `setup()`/`loop()` continuam sem inicializar esses módulos. UART do Payload, integração TT&C, armazenamento físico e atuação do Burn Wire conservam tarefas próprias.

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
- **Dados e persistência:** [contrato OBC v1 proposto](../common/protocol/obc-data.md), com disponibilidade explícita, sessão/sequência e registros binários de 36–105 bytes. O formato persistido não determina o quadro UART e não altera o JSON TT&C.

## Build

Abrir `obc-dh/` no PlatformIO, compilar o env `obc` (`esp32doit-devkit-v1`, Arduino). Deps de terceiros em `lib_deps` com versão fixada.

Os módulos novos usam apenas recursos já disponíveis e não acrescentam dependências. Os testes nativos são executados pelos comandos registrados nas tasks; a compilação do codec/leitor e sua verificação estão no [guia de ferramentas](tools/README.md). O ambiente `obc` não configura esses testes automaticamente por `pio test`.

## Aprofundar

- [.agents/README.md](.agents/README.md) — mapa de documentação e organização do módulo.
- [.agents/TODO.md](.agents/TODO.md) — estado, dependências e links para evidências das tarefas.
- [Arquitetura](.agents/ARCHITECTURE.md), [requisitos](.agents/REQUISITOS.md) e [pinagem](.agents/PINAGEM.md).
- Design Package §§3.1, 3.4, 7.2, 7.3 + Apêndice A (REQ-OBC), conforme as referências da equipe; a versão Markdown citada não está neste checkout.
