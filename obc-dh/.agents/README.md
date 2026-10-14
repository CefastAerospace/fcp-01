# OBC-DH — documentação e orientação

Software de bordo em C++ para o ESP32, com Arduino/FreeRTOS. O OBC-DH integra os módulos dos setores, organiza telecomandos (TC), telemetria (TM), referência temporal e registros da missão. O Payload executa no Raspberry Pi e se comunica por UART; o ADCS será integrado ao mesmo ESP32 do OBC.

## Mapa dos documentos

| Documento | Fonte principal de informação |
|---|---|
| [AGENTS.md](AGENTS.md) | Instruções de trabalho e limites de execução. |
| [CPP_STANDARDS.md](CPP_STANDARDS.md) | Convenções de C++ e documentação do código. |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Decisões vigentes, responsabilidades e comportamento planejado. |
| [REQUISITOS.md](REQUISITOS.md) | Requisitos, critérios de validação e referências históricas preservadas. |
| [DEFINICOES_OBC_DP.md](DEFINICOES_OBC_DP.md) | Síntese para o Design Package: definições, justificativas, custos/sobressalentes e data budget estimado. |
| [TODO.md](TODO.md) | Estado real, inventário, pendências, cinco entregas e evidências. |
| [PINAGEM.md](PINAGEM.md) | GPIOs, endereços e restrições elétricas. |
| [FUNCIONAMENTO_PIO.md](FUNCIONAMENTO_PIO.md) | Instalação, compilação, upload, testes e editor. |

Para trabalhar, consultar instruções e padrões, entender arquitetura/requisitos e localizar no checklist a entrega e suas dependências. Para bancada, conferir pinagem e guia PlatformIO. ICDs definem interfaces entre áreas; ConOps define modos e operação. Obter suas versões vigentes, regulamento e arquitetura lógica CEFAST antes do trabalho que depende dessas referências.

Decisões ficam na arquitetura; definições abertas e evidências ficam no checklist. Não replicar o conteúdo nos guias. Alteração de interface deve atualizar as referências dependentes. Decisão, implementação e validação são estados distintos.

Consolidação de 14/10/2026: UART Payload TX 17 / RX 16, 115,2 kbps 8N1 e binário compacto com SeqID + NACK; LoRa no VSPI, SD no HSPI; encoder AS5600/I2C `0x36`. TT&C usa JSON, `Send()`/`SendTelemetry()`, quadro ≤255 bytes sem fragmentação, CRC do rádio e tarefa própria via `Start()`. EPS já oferece biblioteca INA219; integração do OBC continua pendente. `PENDENCIAS_ARQUITETURAIS.md` foi incorporado a `TODO.md`.

Tempo de voo usa ms monotônicos por boot e referências absolutas pelo LoRa, com reconstrução retroativa. RTC externo só será reconsiderado se o método não atender à validação temporal acordada; DS3231 permanece no histórico. `args.unix` e `ts` do contrato TT&C estão em segundos; a integração deve converter unidades explicitamente e preservar o horário local do Payload como informação de bancada.

## Organização real do módulo

```text
obc-dh/
├── .agents/         # Documentação e instruções.
├── headers/         # Declarações, tipos e interfaces públicas.
│   └── Pinout.h     # Constantes de GPIO; não inicializa hardware.
├── src/
│   └── main.cpp     # setup() e loop() ainda sem comportamento.
├── tests/           # Ainda sem testes implementados.
└── platformio.ini   # Ambiente obc: ESP32/Arduino e diretórios.
```

Criar modelos, implementações e testes conforme os comportamentos forem desenvolvidos, sem arquivos vazios antecipados. Reutilização de ADCS e TT&C, tratamento de estado compartilhado e organização da execução estão na arquitetura. O guia de ferramentas não comprova compilação ou funcionamento do hardware; evidências devem constar do checklist.
