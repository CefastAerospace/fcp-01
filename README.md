# FCP-01: Falcão-Peregrino

CubeSat 2U desenvolvido pela CEFAST Aerospace (equipe aeroespacial do CEFET-MG) para o CubeDesign 2026, competição que acontece em novembro em Buenos Aires, Argentina.

O FCP-01 é o primeiro satélite da linha Falcão-Peregrino, a linha de CubeSats 2U da equipe. As demais linhas são Magela (CubeSats 1U) e Harpia (CanSats).

A missão do FCP-01 é a recepção e o processamento de sinais **ADS-B**, usados para identificação e monitoramento de aeronaves.

Este repositório é **privado** e contém o código de desenvolvimento do satélite. Documentação de engenharia completa (requisitos, arquitetura, resultados de testes) fica no Google Drive da equipe: [Link do Drive](https://drive.google.com/drive/folders/1zuqoZDisrKwNKC3qj_JZJbrycwYCYJCP).

## Arquitetura

O FCP-01 usa uma arquitetura **centralizada**: um único ESP32 (OBC) roda **FreeRTOS** e gerencia todos os subsistemas via periféricos ligados diretamente a ele. O **Payload** é a exceção, roda em um Raspberry Pi Zero W separado, e se comunica com o OBC por UART.

| Subsistema | Hardware | Interface com o OBC |
|---|---|---|
| **OBC & Data Handling** | ESP32, cartão SD, RTC | - (é o próprio OBC) |
| **TT&C** | LoRa SX1276 (921 MHz), servo motor de antena | SPI (HSPI) + PWM |
| **EPS** | 2x INA219 | I2C |
| **Thermal Control** | LM75A | I2C |
| **ADCS** | SimpleFOC Mini (roda de reação) + MPU6050 | PWM/analógico + I2C |
| **Payload** | RTL-SDR v3 + Raspberry Pi Zero W | UART |
| **STR** | CAD (SolidWorks) | - |

O ESP32 usa os dois barramentos SPI disponíveis: **VSPI** para o cartão SD e **HSPI** para o LoRa.

## Estrutura do repositório

```text
FCP-01/
├── README.md
├── .gitignore
├── obc-dh/              # ÚNICO firmware real - platformio.ini mora aqui
│   ├── include/              # Interfaces globais e headers
|   ├── src/
│       ├── main.cpp          # Boot flow, WDT, e instanciação das tasks
|       ├── sd_logger.h       # Header do Logger do SD Card
│       ├── sd_logger.cpp     # Gerenciamento assíncrono do SD Card
│       ├── base_comms.cpp    # Parser e envio via UART da Payload
|       ├── base_comms.h      # Header da interface UART da Payload
│   ├── eps-tc/           # Driver do subsistema de potência
│   ├── rtc/              # Driver e sincronização do RTC
│   └── tt-c/             # Protocolo PUS, rádio SX1276 e parser de TC
└── platformio.ini        # Configurações de compilação e dependências
├── adcs/                 # biblioteca - consumida pelo obc-dh via lib_extra_dirs
│   ├── include/
│   └── src/
├── eps-tc/               # biblioteca - EPS e Thermal Control
│   ├── include/
│   └── src/
├── tt-c/                 # biblioteca - enlace LoRa (TC/TM/dados) + controle do servo de antena
│   ├── include/
│   ├── src/
│   ├── ground-station/   # firmware da estação terrena de bancada (2º ESP32 + SX1276)
│   └── examples/
│       └── link-test/    # firmware de teste do enlace no lado do satélite
├── payload/               # Raspberry Pi Zero W - projeto separado, NÃO é PlatformIO
│   └── telemetry.py       # Script de telemetria que captura as informações dos aviões próximos
├── str/
│   ├── exports/            # arquivos .STEP
│   ├── drawings/           # desenhos técnicos em .PDF
│   └── manufacturing/
└── common/
    ├── protocol/            # spec do protocolo UART (OBC ↔ Payload) e pacotes TT&C
    └── interfaces/
        └── pinout.md        # endereços I2C, alocação de pinos e barramentos SPI
```

## Estrutura Dual-Core

```text
OBC (ESP32 - Dual Core)
├── Core 0 (I/O, Management & System Supervision)
│   ├── System Manager (TaskBootManager) [Prioridade 3]
│   ├── Health Monitor (esp_task_wdt + Loop Supervision)
│   ├── EPS Manager (TaskEPS) [Prioridade 4]
│   ├── SD Manager / Data Logger (SD_Task) [Prioridade 2]
│   ├── RTC Manager (TaskCommunication / I2C) [Prioridade 2]
│   └── Payload Interface (Base_Task / UART) [Prioridade 2]
│
└── Core 1 (Control, Comms & Mission Logic)
    ├── Mode Manager (State Machine / Mission Control)
    ├── TT&C Manager (TaskTTC / LoRa SX1276) [Prioridade 3]
    ├── TC Manager (Command Parser & CRC16 Validation)
    ├── TM Manager (Telemetry Builder & Queue)
    └── ADCS / Motor Control (SimpleFOC PWM Task) [Prioridade 2]
```

**Importante:** `adcs/`, `eps-tc/` e `tt-c/` não são firmwares que rodam sozinhos: são bibliotecas que só ganham vida quando compiladas junto com `obc-dh/`. Só `obc-dh/` (no ESP32) e `payload/` (no Raspberry Pi) são projetos que rodam de forma independente.

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

Dependências de terceiros (ESP32/Arduino) são declaradas em `obc-dh/platformio.ini` via `lib_deps`, com versão fixada. Código interno de cada subsistema é integrado via `lib_extra_dirs`, apontando para as pastas irmãs (`adcs/`, `eps-tc/`, `tt-c/`, `common/`). Nunca copie bibliotecas de terceiros para dentro do repositório.

## CAD

Arquivos nativos do SolidWorks (`.SLDPRT`, `.SLDASM`, `.SLDDRW`) **não** entram no repositório, ficam no Google Drive. Apenas exports em `.STEP` (`str/exports/`) e desenhos em `.PDF` (`str/drawings/`) são versionados aqui.
