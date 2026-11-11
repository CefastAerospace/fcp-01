# OBC-DH — On-Board Computer & Data Handling

Núcleo de processamento do FCP-01: um ESP32 DevKit V1 coordena todos os subsistemas via FreeRTOS e organiza comandos, telemetria e registros da missão (§§3.4, 7.2).

## Estado local — atualização de 11/11/2026

Espressif32 fixado em 6.6.0. As novas bancadas de capacidade/memória
passaram com um ESP32/USB: saturação de 32 entregas, perdas explícitas,
retomada após drenagem, JSON de 65.536 bytes, snapshot denso e NO_MEMORY
preservando o estado anterior. Heap interno mínimo de 1.040 bytes no
esgotamento; aprovação limitada à amostra. Dez builds e regressões
nativas/USB passaram. Procedimentos e evidências no
[guia](tools/README.md#limites-de-capacidade-e-memória--11112026).

Estresse adicional aprovado com ESP32/USB: timeout e erros de leitura,
confirmações desconhecida/duplicada/atrasada e falha de gravação simulada,
em `int006-stress-check`. Os 17 testes TT&C existentes também passaram
na placa com transportes falsos. obc restaurado e verificado. Procedimento
e evidências no [guia](tools/README.md#estresse-em-um-esp32-e-usb--11112026).
Não encerra ensaios com cartão, RF, Pi ou reinício da estação.

O firmware central inicializa tempo/NVS, I2C e SD com o coordenador de bancada existente; depois inicia TT&C e a primeira etapa do fluxo Payload–tempo–registros. Boot de voo, sensores, missão e atuadores conservam suas integrações próprias.

`ping` recebe ACK. `set_time` aplica uma referência UTC em RAM e tenta registrá-la. `get_status` e `request_tm` recebem ACK e TM sob demanda: `ts` é desconhecido antes da referência, `ac` depende do último snapshot válido e `msg` permanece indisponível porque o produtor não informa essa contagem. Os demais dados ausentes continuam como `null`.

UART2 possui tarefa própria de transporte; processamento de snapshots e gravação SD usam tarefas separadas. A recepção mantém FCPU e ACK de aceitação em RAM. A fila e o bloco de gravação são limitados; SD ausente/lento não impede recepção ou TM, e os registros não gravados são contabilizados. Os nomes são exclusivos por boot/segmento, sem sobrescrever histórico.

O [contrato de dados](../common/protocol/obc-data.md) conserva v1 e snapshots v2 e acrescenta TM v3 com contadores opcionais. O buffer de snapshot usa dois blocos para caber na RAM fragmentada do ESP32. O [leitor nativo](tools/README.md) aceita arquivos mistos v1/v2/v3, reconstrói horários e relata corrupção sem alterar a origem.

As verificações locais incluem regressões nativas com sanitizadores, TT&C no computador/ESP e os roteiros USB da [INT-006](.tasks/03-integracao/INT-006-fluxo-de-dados.md). A bancada `int006-check` simula ADS-B e SD lento; o firmware normal foi verificado sem SD/Pi. Isso não confirma interoperabilidade com Payload, persistência física do cartão ou precisão RF.

INT-006 permanece em andamento: envio identificado de aeronaves, eventos TX, confirmações `cf` e extração `request_log` estão integrados no OBC. Após corrigir a admissão por memória acessível por byte, a bancada USB nova e o firmware normal passaram com o roteiro original; relatórios e reprovação anterior estão preservados na tarefa. Timeout de confirmação vale 10 s provisórios. Aceites Payload/TT&C, cartão/radio/Pi reais e preservação de cada atualização ADS-B intermediária continuam pendentes. INT-003 continua aberta pelo boot de voo; INT-004 conserva o aceite conjunto TT&C pendente. DAD-001, DAD-002, DAD-003, COM-004 e COM-005 encerraram seus escopos locais, com encaminhamentos nas integrações e ensaios.

## Barramentos (resumo — referência compartilhada em `../common/interfaces/pinout.md`)

| Interface | Pinos | Uso |
|---|---|---|
| UART0 | 1 / 3 | Umbilical de bancada pelo USB da placa; conferir conector |
| UART payload | 16 / 17, 115,2 kbps | Snapshots JSON completos em FCPU; aceite e ensaios com Pi pendentes |
| I2C sensores | 21 / 22 | INA219, LM75A, MPU-9250, AS5600, BH1750 |
| VSPI | 23/19/18, CS 5, DIO0 4, RST 14 | LoRa SX1276 |
| HSPI | 32/35/15, CS 13 | microSD |
| PWM + EN | 25/26/27, EN 12 | Roda de reação |
| Digital | 33 | Burn-wire das antenas |

`headers/Pinout.h` espelha essa tabela em `constexpr int` (não inicializa hardware). Sem RTC externo: tempo = timer interno + TC (§6.4).

## Contratos

- **Payload → OBC:** snapshots JSON completos com dados ADS-B já decodificados, por decisão de 05/11/2026. O fluxo planejado associa tempo OBC, armazena e prepara TM; recepção e gravação estão ligadas na primeira etapa da INT-006; o aceite com o produtor e os ensaios físicos permanecem pendentes.
- **OBC ↔ TT&C:** biblioteca existente para TC, ACK prioritário e TM JSON; corpo de até 245 bytes sem `n`, quadro final de até 255 bytes (§7.1–7.2).
- **Modos:** ver ConOps no `../README.md` (detalhe em §2.5 do Design Package).
- **Dados e persistência:** [contrato OBC](../common/protocol/obc-data.md), com modelos de snapshot em memória e registros binários v1 de 36–105 bytes. Sessão/sequência pertencem às atualizações individuais v1, não ao snapshot. Os snapshots têm codec v2 próprio, sem alterar o JSON TT&C.

## Build

Abrir `obc-dh/` no PlatformIO, compilar o env `obc` (`esp32doit-devkit-v1`, Arduino). Deps de terceiros em `lib_deps` com versão fixada.

O ambiente `obc` consome a biblioteca local `tt-c`, LoRa 0.8.0 e ArduinoJson 7.0.4 com o patch controlado existente. Desde 11/11/2026, a plataforma também está fixada em `espressif32@6.6.0`, correspondente ao Arduino ESP32 2.0.14 validado. A biblioteca local acompanha o checkout. Consultar o [guia PlatformIO](.agents/FUNCIONAMENTO_PIO.md) para as versões e os procedimentos. Os ambientes UART conservam dependências isoladas; `pio test` no ambiente `obc` não executa automaticamente as verificações nativas.

## Aprofundar

- [.agents/README.md](.agents/README.md) — mapa de documentação e organização do módulo.
- [.agents/TODO.md](.agents/TODO.md) — estado, dependências e links para evidências das tarefas.
- [Encaminhamentos de 11/11](encaminhamentos-2026-11-11.md) — mensagens preparadas para os outros setores, sem envio externo.
- [Arquitetura](.agents/ARCHITECTURE.md), [requisitos](.agents/REQUISITOS.md) e [pinagem](.agents/PINAGEM.md).
- Design Package §§3.1, 3.4, 7.2, 7.3 + Apêndice A (REQ-OBC), conforme as referências da equipe; a versão Markdown citada não está neste checkout.

## Verificação compatível com o firmware

O complemento `tests/ObcStationCheck.py PORTA --bench`, com
`int006-ttc-check`, usa o cliente TT&C real para confirmar entregas e
baixar arquivos, e verifica TM/tempo. Sem --bench usa `obc` sem periféricos.
Os dois recortes passaram com um ESP32/USB; comandos e limitações estão
no [guia](tools/README.md#cliente-real-da-estação-e-tmtempo--11112026).
O roteiro de perda deliberada de confirmação abaixo permanece independente.

Usar `tests/ObcDownlinkCheck.py PORTA` com o firmware atual `obc`, ou com `--bench` após upload de `int006-ttc-check`, conforme o [procedimento](tools/README.md#integração-ttc-e-request_log--11112026). O solo do roteiro é simulado, sem deduplicação persistente. Os roteiros `ObcTtcCheck.py` e `ObcDataFlowCheck.py` conservam suas versões históricas; o segundo espera `request_log = invalid_cmd` e não aceita quadros `ac`, portanto não é compatível com a integração nova. Suas entradas e expectativas não foram alteradas. A verificação nova é focada em entregas/extração e não substitui os demais critérios da TES-008. Os relatórios em `/tmp` são saídas locais regeneráveis, não anexos permanentes.

O enquadramento FCPU é transmitido pelo cabo ao Payload e ainda precisa de aceite desse setor. O armazenamento binário no SD é uma representação interna distinta. Melhorias documentais não validam Pi, cartão, RF ou estados seguros de voo.
