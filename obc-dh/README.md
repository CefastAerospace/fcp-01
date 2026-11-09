# OBC-DH — On-Board Computer & Data Handling

Núcleo de processamento do FCP-01: um ESP32 DevKit V1 coordena todos os subsistemas via FreeRTOS e organiza comandos, telemetria e registros da missão (§§3.4, 7.2).

## Estado local — 05/11/2026

Estão implementados o serviço I2C, os modelos/validadores de dados, a referência temporal por boot, o adaptador timer/NVS para ESP32, o codec de registros com CRC-32 e o serviço de arquivos com adaptador SD/HSPI. O [leitor nativo](tools/README.md) interpreta arquivos no computador, exporta JSON Lines, reconstrói horários e relata trechos inválidos sem alterar a origem. Testes nativos e build ESP32 passaram; isso não comprova funcionamento físico ou integração.

DAD-001, DAD-002 e DAD-003 concluíram seus escopos locais; aceites dos produtores e estados duráveis seguem nas integrações correspondentes. INT-003 acrescenta o boot de bancada: Serial a 115200 baud, reserva de boot/NVS, I2C e tentativa de montagem SD, com relatório de falhas independentes. INT-004 inicia depois o transporte TT&C e atende comandos no `loop()`, reutilizando a tarefa do enlace no Core 0, prioridade 10. `ping` recebe ACK; `get_status` e `request_tm` recebem ACK e TM com `null` para dados indisponíveis. A bancada USB foi verificada no ESP sem rádio/SD; o aceite conjunto TT&C permanece pendente. UART do Payload, aquisição, registros e atuação do Burn Wire conservam suas integrações próprias.

Em 05/11/2026, o responsável confirmou snapshots JSON completos diretamente pela UART do Payload. DAD-001 acrescenta `PAYLOAD_SNAPSHOT`, `AIRCRAFT_STATE` e `EXTRA_FIELD` aos modelos existentes, com campos opcionais, extras textuais e limites fixos. `tests/PayloadSnapshotCheck.cpp` verifica esses modelos sem parser ou hardware. O [contrato](../common/protocol/obc-data.md#0-snapshot-do-payload--modelo-em-memória) descreve mapeamento, limites e pendências. O codec v1 continua cobrindo os modelos anteriores; recepção e gravação integrada dos snapshots no SD permanecem pendentes, assim como a compatibilização dos critérios por mensagem ADS-B. Os testes novos, os testes anteriores de dados/codec/leitor e o build ESP32 passaram. O snapshot ocupa 64672 bytes tanto no computador como na toolchain ESP32, sem instância alocada pelo firmware central.

COM-005 implementa `ObcStorage` e `ObcStorageEsp32`: criação exclusiva sem apagar histórico, sincronização a cada bloco, até três tentativas no mesmo offset e leitura por blocos. O teste `ObcStorageCheck.cpp` verifica arquivos reais e falhas injetadas; build ESP32 aprovado. INT-003 chama sua montagem no boot de bancada, sem abrir arquivos, gerar registros ou implementar `request_log`. A [tarefa](.tasks/01-comunicacao/done/COM-005-armazenamento-sd.md) permanece em andamento até confirmação de módulo/alimentação com EPS; os ensaios físicos estão em TES-006/TES-009.

O teste `ObcStartupCheck.cpp` verifica ordem, continuidade e relatório nas oito combinações de resultados dos serviços; o build ESP32 também passou. Logs `[OBC]` mostram o ID válido ou indisponível, resultado de I2C/SD e tempo monotônico separado de UTC desconhecido. Não há repetição/reset automático, inicialização de atuadores ou modo de voo implementado. A [INT-003](.tasks/03-integracao/INT-003-inicializacao-central.md) permanece em andamento pelas condições externas do boot integrado; nenhum upload ou ensaio físico foi realizado nesta entrega.

DAD-003 implementa persistência dos snapshots e extras em registros v2 de 37–64497 bytes, usando o modelo existente e buffers do chamador. O codec v1 e seus layouts permanecem compatíveis; o leitor exporta arquivos v1/v2/mistos e reconstrói horários pelas referências existentes. `SnapshotCodecCheck.cpp` e `SnapshotRecordsCheck.py` verificam vetores independentes, limites, corrupção, truncamento e preservação das saídas/origem. Testes anteriores e build ESP32 também passaram. A [DAD-003](.tasks/02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) permanece em andamento por aceite de DAD-001 e estados duráveis de retomada; recepção e gravação integrada no SD ainda não estão ligadas ao boot.

## Barramentos (resumo — oficial em `../common/interfaces/pinout.md`)

| Interface | Pinos | Uso |
|---|---|---|
| UART0 | 1 / 3 | Umbilical USB-C de bancada |
| UART payload | 16 / 17, 115,2 kbps | Snapshots JSON completos; enquadramento e perdas/repetição pendentes em COM-004 |
| I2C sensores | 21 / 22 | INA219, LM75A, MPU-9250, AS5600, BH1750 |
| VSPI | 23/19/18, CS 5, DIO0 4, RST 14 | LoRa SX1276 |
| HSPI | 32/35/15, CS 13 | microSD |
| PWM + EN | 25/26/27, EN 12 | Roda de reação |
| Digital | 33 | Burn-wire das antenas |

`headers/Pinout.h` espelha essa tabela em `constexpr int` (não inicializa hardware). Sem RTC externo: tempo = timer interno + TC (§6.4).

## Contratos

- **Payload → OBC:** snapshots JSON completos com dados ADS-B já decodificados, por decisão de 05/11/2026. O fluxo planejado associa tempo OBC, armazena e prepara TM; o codec de snapshots v2 está implementado, enquanto recepção e gravação integrada no SD permanecem pendentes.
- **OBC ↔ TT&C:** filas `filaTC` (TC validado) e `filaTM` (telemetria JSON ~150 B) — picos de um lado não travam o outro (§7.1–7.2).
- **Modos:** ver ConOps no `../README.md` (detalhe em §2.5 do Design Package).
- **Dados e persistência:** [contrato OBC](../common/protocol/obc-data.md), com modelos de snapshot em memória e registros binários v1 de 36–105 bytes. Sessão/sequência pertencem às atualizações individuais v1, não ao snapshot. Os snapshots têm codec v2 próprio, sem alterar o JSON TT&C.

## Build

Abrir `obc-dh/` no PlatformIO, compilar o env `obc` (`esp32doit-devkit-v1`, Arduino). Deps de terceiros em `lib_deps` com versão fixada.

O ambiente `obc` consome a biblioteca local `tt-c`, LoRa 0.8.0 e ArduinoJson 7.0.4 com o patch controlado existente. Os ambientes UART conservam suas dependências isoladas. Os testes nativos são executados pelos comandos registrados nas tasks; a compilação do codec/leitor e sua verificação estão no [guia de ferramentas](tools/README.md). O ambiente `obc` não configura esses testes automaticamente por `pio test`. A verificação USB da INT-004 usa `python3 tests/ObcTtcCheck.py /dev/ttyUSB0`, com pyserial; entradas, respostas e tempos são gravados em `/tmp/fcp01-int004-usb.json`.

## Aprofundar

- [.agents/README.md](.agents/README.md) — mapa de documentação e organização do módulo.
- [.agents/TODO.md](.agents/TODO.md) — estado, dependências e links para evidências das tarefas.
- [Arquitetura](.agents/ARCHITECTURE.md), [requisitos](.agents/REQUISITOS.md) e [pinagem](.agents/PINAGEM.md).
- Design Package §§3.1, 3.4, 7.2, 7.3 + Apêndice A (REQ-OBC), conforme as referências da equipe; a versão Markdown citada não está neste checkout.
