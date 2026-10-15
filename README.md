# FCP-01: Falcão-Peregrino

CubeSat 2U desenvolvido pela CEFAST Aerospace (equipe aeroespacial do CEFET-MG) para o CubeDesign 2026, competição que acontece em novembro em Buenos Aires, Argentina.

O FCP-01 é o primeiro satélite da linha Falcão-Peregrino, a linha de CubeSats 2U da equipe. As demais linhas são Magela (CubeSats 1U) e Harpia (CanSats).

A missão primária do FCP-01 é a recepção e o processamento de sinais **ADS-B em 1090 MHz**, em uma janela contínua de dez minutos, com até vinte aeronaves. A missão secundária, opcional, é a caracterização do noise floor em LEO.

Este repositório é **privado** e contém o código de desenvolvimento do satélite. Documentação de engenharia completa (requisitos, arquitetura, resultados de testes) fica no Google Drive da equipe: [Link do Drive](https://drive.google.com/drive/folders/1zuqoZDisrKwNKC3qj_JZJbrycwYCYJCP).

O Design Package e os ICDs/ConOps vigentes são as referências de engenharia. A versão `FCP-01_Design_Package_CubeDesign_2026.md` citada pelos módulos não está presente neste checkout; as seções indicadas abaixo seguem as referências registradas no remoto. Este README distingue decisões de projeto do estado implementado.

## Arquitetura e estado da integração

O FCP-01 tem arquitetura centralizada planejada: um ESP32 executará o firmware integrado do OBC, incluindo ADCS, sobre Arduino/FreeRTOS. O Payload executa em Raspberry Pi Zero W e se comunica com o OBC por UART. O firmware do OBC ainda contém `setup()`/`loop()` vazios; a descrição abaixo é de integração prevista, não de funcionalidades já validadas.

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
| UART payload | RX 16 / TX 17, 115,2 kbps 8N1 | Pi ↔ OBC, binário ADS-B com SeqID + NACK (§6.4) |
| I2C de sensores | SDA 21 / SCL 22 | 3x INA219, 5x LM75A, MPU-9250 (`0x68`), AS5600 (`0x36`), 2x BH1750 (`0x23`/`0x5C`) |
| VSPI | MOSI 23 / MISO 19 / SCK 18, CS 5, DIO0 4, RST 14 | LoRa SX1276 (921 MHz, §7.1) |
| HSPI | MOSI 32 / MISO 35 / SCK 15, CS 13 | microSD (missão + logs) |
| PWM + EN | U/V/W 25 / 26 / 27, EN 12 | SimpleFOC Mini; desabilitar fora do detumbling, consumo residual a medir |
| Saída digital | GPIO 33 | Burn-wire das duas antenas; orçamento remoto de 2,5 A / 12,5 W transitório |

Payload fica fora do I2C. MPU-9250 substitui menções antigas a MPU6050; Burn Wire substitui o servo. MISO do SD usa GPIO 35 por segurança de boot. A configuração atual dispensa RTC externo: referências LoRa permitem reconstruir horário a partir do tempo monotônico; instalar RTC só será reconsiderado se esse método não atender à validação temporal acordada.

Detalhes e restrições em [pinout compartilhado](common/interfaces/pinout.md) e [pinagem OBC](obc-dh/.agents/PINAGEM.md). Endereços finais de EPS/Térmico e condições do Burn Wire continuam dependentes dos responsáveis.

## Contratos de dados

- **Payload → OBC:** dados ADS-B já decodificados em binário compacto via UART; SeqID + NACK trata gaps (§6.3–6.4). I/Q cru permanece no Pi. Enquadramento final e transporte no script aguardam implementação/validação.
- **Tempo:** OBC associa ms desde o boot e referências absolutas recebidas via `set_time`; preserva o timestamp local do Payload como informação de bancada, sem confundi-lo com a referência de voo.
- **TM/TC:** [contrato JSON TT&C](common/protocol/ttc-json.md), LoRa 921 MHz SF7 e umbilical. `Ttc::Send()` aceita corpo de até 245 bytes sem `n`; o quadro final tem até 255 bytes, sem fragmentação. Descida sob demanda, ACK prioritário e agregação por aeronave; log completo no SD.
- **SD:** FAT32, fechamento controlado e monitoramento de alimentação (§7.3); integridade após interrupção de escrita ainda exige evidência de validação.

## Estrutura do repositório

```text
fcp-01/
├── obc-dh/                 # Firmware central a implementar.
│   ├── .agents/            # Planejamento, requisitos e guias.
│   ├── headers/Pinout.h    # Constantes, sem inicialização de hardware.
│   ├── src/main.cpp        # Pontos de entrada ainda vazios.
│   ├── tests/              # Ainda sem testes implementados.
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
    ├── protocol/           # Contrato JSON TT&C; binário UART do Payload a detalhar.
    └── interfaces/pinout.md
```

## Organização da execução

O controle frequente do motor será separado das operações demoradas de rádio e SD, com acesso coordenado aos sensores/encoder I2C. Reutilizar filas e protocolo TT&C: `Ttc::Start()` cria a tarefa de enlace no Core 0, prioridade padrão 10, conforme decisão desse módulo. Quantidade, prioridades e afinidade das demais tarefas dependem dos limites acordados com ADCS e das medições; o antigo mapa completo por núcleo não representa implementação validada.

ADCS possui firmware de bancada e será integrado ao OBC preservando seus algoritmos. TT&C já possui biblioteca e firmwares próprios de exemplo/estação. Esses ambientes independentes não comprovam integração no firmware central.

O firmware central ESP32 é o ambiente `obc` em `obc-dh/platformio.ini`; o Payload executa no Raspberry Pi, fora do PlatformIO. Bancadas ADCS, TT&C e estação possuem ambientes próprios. Para compilar a base OBC, executar `pio run -e obc` dentro de `obc-dh/`; configuração e uso estão no [guia PlatformIO](obc-dh/.agents/FUNCIONAMENTO_PIO.md).

## Onde aprofundar

| Assunto | Referência |
|---|---|
| Requisitos e rastreabilidade | Design Package §4 + Apêndice A; `obc-dh/.agents/REQUISITOS.md` |
| Orçamentos | Design Package §5; `obc-dh/.agents/DEFINICOES_OBC_DP.md` |
| Payload e binário UART | Design Package §6; `payload/README.md` |
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

Dependências de terceiros devem ser declaradas no ambiente consumidor via `lib_deps`, com versões validadas fixadas. O ambiente OBC atual ainda não declara dependências de periféricos nem integração das bibliotecas irmãs; o checklist registra esse trabalho. Reutilizar a biblioteca `tt-c/` e integrar o ADCS conforme os contratos. Nunca copie bibliotecas de terceiros para dentro do repositório.

## CAD

Arquivos nativos do SolidWorks (`.SLDPRT`, `.SLDASM`, `.SLDDRW`) **não** entram no repositório, ficam no Google Drive. Apenas exports em `.STEP` (`str/exports/`) e desenhos em `.PDF` (`str/drawings/`) são versionados aqui.
