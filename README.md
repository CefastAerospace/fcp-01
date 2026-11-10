# FCP-01: Falcão-Peregrino

CubeSat 2U desenvolvido pela CEFAST Aerospace (equipe aeroespacial do CEFET-MG) para o CubeDesign 2026, competição que acontece em novembro em Buenos Aires, Argentina.

O FCP-01 é o primeiro satélite da linha Falcão-Peregrino, a linha de CubeSats 2U da equipe. As demais linhas são Magela (CubeSats 1U) e Harpia (CanSats).

A missão primária do FCP-01 é a recepção e o processamento de sinais **ADS-B em 1090 MHz**, em uma janela contínua de dez minutos, com até vinte aeronaves. A missão secundária, opcional, é a caracterização do noise floor em LEO.

Este repositório é **privado** e contém o código de desenvolvimento do satélite. Documentação de engenharia completa (requisitos, arquitetura, resultados de testes) fica no Google Drive da equipe: [Link do Drive](https://drive.google.com/drive/folders/1zuqoZDisrKwNKC3qj_JZJbrycwYCYJCP).

O Design Package e os ICDs/ConOps vigentes são as referências de engenharia. A versão `FCP-01_Design_Package_CubeDesign_2026.md` citada pelos módulos não está presente neste checkout; as seções indicadas abaixo seguem as referências registradas no remoto. Este README distingue decisões de projeto do estado implementado.

**Revisão documental OBC de 10/11/2026:** o resumo abaixo reflete `839a186`. Evidências e históricos das entregas permanecem nas tarefas do [painel OBC](obc-dh/.agents/TODO.md); esta revisão não acrescenta ensaios ou aceites.

## Arquitetura e estado da integração

O FCP-01 tem arquitetura centralizada planejada: ESP32/Arduino/FreeRTOS, com ADCS ainda a integrar, e Payload no Raspberry Pi Zero W por UART. O firmware OBC atual inicia Serial a 115200 baud, reserva boot/NVS, inicializa I2C e tenta montar o SD. Depois inicia TT&C e o fluxo Payload–tempo–registros; `loop()` atende TCs e diagnósticos, sem a antiga espera de 100 ms.

A primeira etapa INT-006 recebe snapshots JSON em FCPU local, aplica referências de tempo e usa diário binário por boot/segmento. O leitor aceita v1/v2/v3 e exporta JSON Lines. `ping`, `get_status`, `request_tm` e `set_time` estão implementados; `request_log`, envio agregado de aeronaves, sensores, modos, atuações e supervisão completa conservam suas tarefas próprias. Boot de voo e estados elétricos continuam pendentes.

As verificações locais registradas incluem computador, ESP/USB e bancada com dados em flash/SD simulado. Não comprovam troca com Pi, cartão físico ou precisão RF. COM-004/005, DAD-001/003 e TES-004/007 encerraram escopos locais; aceites e ensaios continuam nas integrações/TES destinatárias. Consultar [contratos](common/protocol/obc-data.md) e [procedimentos](obc-dh/tools/README.md).

| Subsistema | Hardware / interface prevista |
|---|---|
| **OBC-DH** | ESP32 e microSD FAT32; tempo monotônico por boot com referências recebidas pelo LoRa. RTC externo fora da configuração atual. |
| **TT&C** | LoRa SX1276, 921 MHz SF7; biblioteca JSON, umbilical UART0, estação terrena e testes próprios. Burn Wire libera fisicamente a antena. |
| **EPS / Térmico** | 3x INA219 e 5x LM75A previstos. Biblioteca EPS_INA219_Standard existente; endereços finais, aquisição/proteção e integração central dependem dos contratos. |
| **ADCS** | SimpleFOC, MPU9250, BH1750 e encoder; controle no mesmo ESP32 do OBC. |
| **Payload** | RTL-SDR v3 e Raspberry Pi Zero W; UART ao OBC ainda sem transporte implementado no script. |
| **Estruturas** | CAD e mecanismo físico de liberação da antena. |

A alocação de projeto utiliza **VSPI para LoRa** e **HSPI para SD**; o mapa anterior invertido é histórico. A UART do Payload utiliza TX = 17 e RX = 16, a 115,2 kbps 8N1. Confirmar disponibilidade dos GPIOs no módulo, montagem e estados elétricos antes da integração; documentação não comprova ligação física ou teste.

Consultar [arquitetura vigente](obc-dh/.agents/ARCHITECTURE.md), [requisitos e referências](obc-dh/.agents/REQUISITOS.md), [checklist e pendências](obc-dh/.agents/TODO.md) e [pinagem](obc-dh/.agents/PINAGEM.md). A documentação resumida do OBC tem seu [mapa de leitura](obc-dh/.agents/README.md).

## ConOps — sequência e modos de projeto

Sequência operacional registrada no remoto, com referência à §2.4 do Design Package:

| Fase | Nome | Resumo p/ dev |
|---|---|---|
| P0 | Integração/bancada | Alimentação externa via umbilical USB-C, RBF inserido |
| P1 | Inicialização | OBC auto-inicia, self-test, entra em SAFE/INIT |
| P2 | Detumbling | ADCS reduz giro até < 5 rpm (telecomando) |
| P3 | Apontamento solar | Controle 1 eixo por sensores de luz (telecomando) |
| P4 | Missão ADS-B | Payload recebe/decodifica; OBC carimba, armazena e prepara TM |
| P5 | Downlink | TM desce / TC sobe durante passes (concorrente com P4) |
| P6 | Low-power/safe | Watchdog de energia/temperatura; só tarefas críticas |

Modos e transições registrados com referência à §2.5:

| Modo | Entrada | Saída |
|---|---|---|
| UMBILICAL/BENCH | Configuração de bancada via USB-C | Testes ok → config de voo |
| SAFE/INIT | Boot ou reset por watchdog | TC específico → DETUMBLING / SUN-POINTING / MISSION |
| DETUMBLING | TC + condições de segurança | Taxa < 5 rpm → SAFE/INIT |
| SUN-POINTING | TC + sensores de luz válidos | Estabilidade ok → SAFE/INIT |
| MISSION (ADS-B) | TC + checks ok | Fim da janela, TC stop ou falha → SAFE/LOW-POWER |
| LOW-POWER | Limite energia/temperatura | Recuperação → SAFE/INIT |

Regras registradas: qualquer modo vai a LOW-POWER por watchdog; qualquer falha com restart volta a SAFE/INIT; passe de solo não troca modo. Conforme decisão TT&C, LOW-POWER mantém o rádio em RX para receber o TC de recuperação. A execução dessas transições no OBC ainda está pendente; a integração deve conferir o ConOps vigente sem substituir critérios dos setores.

## Interfaces de projeto — alterações exigem ICD

Alocação registrada na §3.1 do Design Package e nas constantes de `obc-dh/headers/Pinout.h`:

| Interface | Pinos ESP32 | Uso |
|---|---|---|
| UART0 | GPIO 1 / 3 | Umbilical USB-C (TC/TM bancada) |
| UART payload | RX 16 / TX 17, 115,2 kbps 8N1 | Snapshots JSON completos; FCPU implementado localmente; aceite, produtor Pi e tráfego pendentes em INT-006 |
| I2C de sensores | SDA 21 / SCL 22 | 3x INA219, 5x LM75A, MPU-9250 (`0x68`), AS5600 (`0x36`), 2x BH1750 (`0x23`/`0x5C`) |
| VSPI | MOSI 23 / MISO 19 / SCK 18, CS 5, DIO0 4, RST 14 | LoRa SX1276 (921 MHz, §7.1) |
| HSPI | MOSI 32 / MISO 35 / SCK 15, CS 13 | microSD (missão + logs) |
| PWM + EN | U/V/W 25 / 26 / 27, EN 12 | SimpleFOC Mini; desabilitar fora do detumbling, consumo residual a medir |
| Saída digital | GPIO 33 | Burn-wire das duas antenas; orçamento remoto de 2,5 A / 12,5 W transitório |

Payload fica fora do I2C. MPU-9250 substitui menções antigas a MPU6050; Burn Wire substitui o servo. MISO do SD usa GPIO 35 por segurança de boot. A configuração atual dispensa RTC externo: referências LoRa permitem reconstruir horário a partir do tempo monotônico; instalar RTC só será reconsiderado se esse método não atender à validação temporal acordada.

Detalhes e restrições em [pinout compartilhado](common/interfaces/pinout.md) e [pinagem OBC](obc-dh/.agents/PINAGEM.md). Endereços finais de EPS/Térmico e condições do Burn Wire continuam dependentes dos responsáveis.

## Contratos de dados

- **Payload → OBC:** snapshots JSON completos com dados ADS-B já decodificados via UART, conforme decisão de 05/11/2026. I/Q cru permanece no Pi. FCPU tem implementação local; seu cabeçalho e controles binários atravessam o cabo. Aceite do enquadramento, produtor Pi e tráfego continuam em INT-006. `id` é opaco, não SeqID.
- **Tempo:** OBC associa ms desde o boot e referências absolutas recebidas via `set_time`; preserva o timestamp local do Payload como informação de bancada, sem confundi-lo com a referência de voo.
- **TM/TC:** [contrato JSON TT&C](common/protocol/ttc-json.md), LoRa 921 MHz SF7 e umbilical. `Ttc::Send()` aceita corpo de até 245 bytes sem `n`; o quadro final tem até 255 bytes, sem fragmentação. Descida sob demanda e ACK prioritário; envio agregado de aeronaves ainda pertence à segunda etapa INT-006. Os registros recebidos têm gravação integrada, sujeita a falhas/descartes explícitos.
- **SD:** FAT32, fechamento controlado e monitoramento de alimentação (§7.3); integridade após interrupção de escrita ainda exige evidência de validação.
- **Registros OBC:** contrato binário local v1/v2/v3, separado dos transportes UART/LoRa; sete tipos de 36–105 bytes, limite global de 512 bytes. Codec portátil, leitor no computador e serviço de arquivos SD/HSPI implementados localmente; snapshots e extras têm representação v2 de 37–64497 bytes, separada do limite v1 de 512 bytes. TM v3 tem 99 bytes e disponibilidade de contadores. Recepção e gravação já estão ligadas; extração pelo umbilical, integração de missão e ensaios físicos continuam pendentes.

## Estrutura do repositório

```text
fcp-01/
├── obc-dh/                 # Boot, TT&C e fluxo Payload–registros; missão completa pendente.
│   ├── .agents/            # Painel TODO, requisitos e guias.
│   ├── .tasks/             # Comunicação, dados/tempo, integração e testes; done/ por grupo.
│   ├── headers/            # Interfaces de serviços, modelos, transporte e integração.
│   ├── src/                # Módulos e boot de bancada; sem missão integrada.
│   ├── tests/              # Verificações nativas e roteiros USB por versão/ambiente.
│   ├── tools/              # Leitor C++ de arquivos OBC e exportação JSON Lines.
│   └── platformio.ini      # Ambiente ESP32/Arduino.
├── adcs/                   # Firmware de bancada; integração central pendente.
├── eps-tc/                 # Biblioteca EPS_INA219_Standard; integração central pendente.
├── tt-c/                   # Biblioteca de enlace LoRa.
│   ├── include/            # API e protocolo compartilhado com o solo.
│   ├── src/                # Rádio, protocolo e enlace.
│   ├── ground-station/     # Firmware da estação terrena de bancada.
│   │   └── app/            # Notebook: interface web, logs, verificações e testes.
│   ├── test/               # Testes próprios do protocolo e do enlace.
│   └── examples/link-test/ # Exemplo do enlace no lado do satélite.
├── payload/                # Coleta/registro no Raspberry Pi; UART pendente.
├── str/                    # Estruturas e arquivos CAD exportados.
└── common/
    ├── protocol/           # JSON TT&C, dados/registros OBC e FCPU local; aceites pendentes.
    └── interfaces/pinout.md
```

## Organização da execução

O controle frequente do motor será separado das operações demoradas de rádio e SD, com acesso coordenado aos sensores/encoder I2C. Reutilizar filas e protocolo TT&C: `Ttc::Start()` cria a tarefa de enlace no Core 0, prioridade padrão 10, conforme decisão desse módulo. UART2, processamento e escritor já têm tarefas separadas na primeira etapa INT-006, descritas na arquitetura OBC. A integração das tarefas ADCS depende dos limites acordados e das medições; o antigo mapa completo por núcleo não representa implementação validada.

ADCS possui firmware de bancada e será integrado ao OBC preservando seus algoritmos. TT&C já possui biblioteca e firmwares próprios de exemplo/estação. A biblioteca TT&C já está ligada ao firmware central; seus exemplos independentes não comprovam os ensaios físicos integrados.

O firmware central ESP32 é o ambiente `obc` em `obc-dh/platformio.ini`; o Payload executa no Raspberry Pi, fora do PlatformIO. Bancadas ADCS, TT&C e estação possuem ambientes próprios. Para compilar a base OBC, executar `pio run -e obc` dentro de `obc-dh/`; configuração e uso estão no [guia PlatformIO](obc-dh/.agents/FUNCIONAMENTO_PIO.md).

## Onde aprofundar

| Assunto | Referência |
|---|---|
| Requisitos e rastreabilidade | Design Package §4 + Apêndice A; `obc-dh/.agents/REQUISITOS.md` |
| Orçamentos | Design Package §5; `obc-dh/.agents/DEFINICOES_OBC_DP.md` |
| Dados, registros e análise OBC | [Contrato v1/v2/v3](common/protocol/obc-data.md) e [codec/leitor](obc-dh/tools/README.md) |
| Payload e snapshots UART | Decisão de 05/11/2026 em `payload/README.md`; binário anterior no Design Package §6 |
| Rádio, execução, SD e estação | Design Package §7; `tt-c/README.md` e contrato JSON compartilhado |
| V&V e resultados | Design Package §8; evidências dos setores |
| Status e roadmap UNSAM | Design Package §9 |
| Pendências de integração | `obc-dh/.agents/TODO.md` |

## Fluxo de desenvolvimento (Git)

Fluxo padrão para mudanças dentro do seu próprio subsistema:

```bash
git pull
git status
# edite os arquivos
git add .
git commit -m "nome do subsistema: descrição da alteração"
git push
```

Os nomes dos subsistemas devem seguir o padrão de nomenclatura das pastas do repositório.

A `main` está protegida contra force-push e deleção, mas não exige review obrigatório.

## Bibliotecas e dependências

Dependências de terceiros devem ser declaradas no ambiente consumidor via `lib_deps`. O OBC atual consome `tt-c` do checkout e fixa ArduinoJson 7.0.4/LoRa 0.8.0. A plataforma `espressif32` não está fixada; as versões observadas nas evidências não garantem uma instalação nova idêntica. A integração ADCS permanece pendente. Nunca copie bibliotecas de terceiros para dentro do repositório.

## CAD

Arquivos nativos do SolidWorks (`.SLDPRT`, `.SLDASM`, `.SLDDRW`) **não** entram no repositório, ficam no Google Drive. Apenas exports em `.STEP` (`str/exports/`) e desenhos em `.PDF` (`str/drawings/`) são versionados aqui.
