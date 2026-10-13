# FCP-01: Falcão-Peregrino

CubeSat 2U da **CEFAST Aerospace (CEFET-MG)** para o **CubeDesign 2026** (categoria CubeSat).
Missão primária: recepção e processamento **ADS-B em 1090 MHz** (janela de 10 min, até 20 aeronaves).
Missão secundária (opcional): caracterização do noise floor em LEO.

> Fonte da verdade de engenharia: `FCP-01_Design_Package_CubeDesign_2026.md` (este README só traz o essencial para desenvolver).
> Documentação completa e Drive da equipe: [Link do Drive](https://drive.google.com/drive/folders/1zuqoZDisrKwNKC3qj_JZJbrycwYCYJCP).

## ConOps — o que o firmware precisa respeitar

Sequência operacional (detalhe em §2.4 do Design Package):

| Fase | Nome | Resumo p/ dev |
|---|---|---|
| P0 | Integração/bancada | Alimentação externa via umbilical USB-C, RBF inserido |
| P1 | Inicialização | OBC auto-inicia, self-test, entra em SAFE/INIT |
| P2 | Detumbling | ADCS reduz giro até < 5 rpm (telecomando) |
| P3 | Apontamento solar | Controle 1 eixo por sensores de luz (telecomando) |
| P4 | Missão ADS-B | Payload recebe/decodifica; OBC carimba, armazena e prepara TM |
| P5 | Downlink | TM desce / TC sobe durante passes (concorrente com P4) |
| P6 | Low-power/safe | Watchdog de energia/temperatura; só tarefas críticas |

Modos e transições (detalhe em §2.5):

| Modo | Entrada | Saída |
|---|---|---|
| UMBILICAL/BENCH | Configuração de bancada via USB-C | Testes ok → config de voo |
| SAFE/INIT | Boot ou reset por watchdog | TC específico → DETUMBLING / SUN-POINTING / MISSION |
| DETUMBLING | TC + condições de segurança | Taxa < 5 rpm → SAFE/INIT |
| SUN-POINTING | TC + sensores de luz válidos | Estabilidade ok → SAFE/INIT |
| MISSION (ADS-B) | TC + checks ok | Fim da janela, TC stop ou falha → SAFE/LOW-POWER |
| LOW-POWER | Limite energia/temperatura | Recuperação → SAFE/INIT |

Regras: qualquer modo vai a LOW-POWER por watchdog; qualquer falha com restart volta a SAFE/INIT; passe de solo não troca modo.

## Interfaces congeladas (ESP32 — não mudar sem ICD)

Alocação unificada (§3.1 do Design Package, espelhada em `obc-dh/headers/Pinout.h`):

| Interface | Pinos ESP32 | Uso |
|---|---|---|
| UART0 | GPIO 1 / 3 | Umbilical USB-C (TC/TM bancada) |
| UART payload | GPIO 16 / 17, 115,2 kbps | Pi → OBC, pacotes binários ADS-B (SeqID + NACK, §6.4) |
| I2C (único, sensores) | GPIO 21 (SDA) / 22 (SCL) | 3x INA219, 5x LM75A, MPU-9250 (`0x68`), AS5600 (`0x36`), 2x BH1750 (`0x23`/`0x5C`) |
| VSPI | 23 / 19 / 18, CS 5, DIO0 4, RST 14 | LoRa SX1276 (921 MHz, §7.1) |
| HSPI | MOSI 32, SCK 15, MISO 35, CS 13 | microSD (missão + logs) |
| PWM + EN | 25 / 26 / 27 (U/V/W), EN 12 | SimpleFOC Mini (roda de reação; EN desliga fora do detumbling) |
| Saída digital | GPIO 33 | Burn-wire das 2 antenas (2 circuitos, 2,5 A / 12,5 W transitório) |

Avisos: **sem RTC externo** (tempo = timer interno ESP32 + TC fixa absoluto com correção retroativa, §6.4); **sem servo de antena** (burn-wire); **sem MPU6050** (é MPU-9250); LoRa é VSPI e SD é HSPI (o inverso está obsoleto); MISO do SD no GPIO 35 por segurança de boot.

Detalhe completo em `common/interfaces/pinout.md` e `obc-dh/.agents/PINAGEM.md`.

## Contratos de dados (mínimo para integrar)

- **Payload → OBC:** só dado já decodificado (ICAO, posição, altitude, velocidade) em binário compacto via UART; nunca I/Q cru (§6.3–6.4). Perda/gap trata com SeqID + NACK.
- **Tempo:** OBC carimba tudo em ms; payload usa referência do OBC (§6.4, REQ-OBC-006).
- **TM/TC:** LoRa 921 MHz SF7, pacotes JSON ~150 B; OBC enfileira TM e TT&C transmite quando o rádio libera; downlink é agregado por aeronave (log completo fica no SD, §5.3/§7.1).
- **SD:** FAT32 com fechamento controlado + monitoramento de alimentação (§7.3).

## Estrutura do repositório e build

```text
FCP-01/
├── FCP-01_Design_Package_CubeDesign_2026.md
├── obc-dh/              # ÚNICO firmware ESP32 (env `obc` em obc-dh/platformio.ini)
│   ├── headers/Pinout.h # Constantes de GPIO (não inicializa hardware)
│   ├── src/main.cpp     # Boot, WDT, tasks
│   ├── tests/           # Testes de comportamento
│   └── .agents/         # Arquitetura, pinagem, pendências e checklist do OBC-DH
├── adcs/                # Bancada/lib ADCS (SimpleFOC, sensores) — integra via OBC
├── eps-tc/              # Lib EPS + Thermal
├── tt-c/                # Lib enlace LoRa + ground-station/ + examples/link-test/
├── payload/             # Raspberry Pi Zero W (NÃO é PlatformIO): telemetry.py + runbook
├── common/
│   ├── interfaces/pinout.md  # Tabela oficial de pinos/endereços
│   └── protocol/             # Specs de protocolo UART e pacotes TT&C
└── str/                 # exports/ (.STEP), drawings/ (.PDF), manufacturing/
```

Só `obc-dh/` (ESP32) e `payload/` (Raspberry Pi) rodam de forma independente; `adcs/`, `eps-tc/`, `tt-c/` são bibliotecas/consumidas pelo OBC-DH.
Build do OBC: abrir `obc-dh/` no PlatformIO (`board = esp32doit-devkit-v1`, `framework = arduino`) e compilar o env `obc`. Ver `obc-dh/.agents/FUNCIONAMENTO_PIO.md`.
Dependências de terceiros vão em `lib_deps` com versão fixada; nunca copiar lib externa para o repo.

## Onde aprofundar (não duplicado aqui)

| Preciso de… | Ver |
|---|---|
| Requisitos e rastreabilidade | §4 + Apêndice A |
| Orçamentos (massa/potência/enlace/custo) | §5 |
| Payload e protocolo binário | §6 |
| Rádio, FreeRTOS, SD, ground station | §7 |
| V&V, resultados de bancada | §8 |
| Status, open issues, roadmap UNSAM | §9 |
| Dúvidas de interface ainda abertas | `obc-dh/.agents/PENDENCIAS_ARQUITETURAIS.md` |

## Fluxo de desenvolvimento (Git)

```bash
git pull
git status
# edite os arquivos
git add .
git commit -m "nome do subsistema: descrição da alteração"
git push
```

Nomes de subsistema seguem as pastas do repo. A `main` é protegida contra force-push e deleção.

## CAD

Nativos do SolidWorks (`.SLDPRT`, `.SLDASM`, `.SLDDRW`) ficam no Drive. Aqui só `.STEP` (`str/exports/`) e `.PDF` (`str/drawings/`).
