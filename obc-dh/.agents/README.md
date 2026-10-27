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
| [TODO.md](TODO.md) | Painel unificado: entregas, pré-requisitos, condições externas e estado em quatro grupos. |
| [Tarefas](../.tasks/README.md) | Escopo, decisões pendentes, evidências e histórico por tarefa; concluídas em `done/`. |
| [PINAGEM.md](PINAGEM.md) | GPIOs, endereços e restrições elétricas. |
| [FUNCIONAMENTO_PIO.md](FUNCIONAMENTO_PIO.md) | Instalação, compilação, upload, testes e editor. |

Para trabalhar, consultar instruções e padrões, entender arquitetura/requisitos e localizar no checklist a entrega e suas dependências. Para bancada, conferir pinagem e guia PlatformIO. ICDs definem interfaces entre áreas; ConOps define modos e operação. Obter suas versões vigentes, regulamento e arquitetura lógica CEFAST antes do trabalho que depende dessas referências.

Decisões ficam na arquitetura; definições abertas e evidências ficam nas tarefas vinculadas pelo checklist. Não replicar o conteúdo nos guias. Alteração de interface deve atualizar as referências dependentes. Decisão, implementação e validação são estados distintos.

Consolidação de 14/10/2026: UART Payload TX 17 / RX 16, 115,2 kbps 8N1 e binário compacto com SeqID + NACK; LoRa no VSPI, SD no HSPI; encoder AS5600/I2C `0x36`. TT&C usa JSON, `Send()`/`SendTelemetry()`, quadro ≤255 bytes sem fragmentação, CRC do rádio e tarefa própria via `Start()`. EPS já oferece biblioteca INA219; integração do OBC continua pendente. `PENDENCIAS_ARQUITETURAIS.md` foi incorporado ao antigo TODO; suas pendências agora estão distribuídas nas tarefas.

Tempo de voo usa ms monotônicos por boot e referências absolutas pelo LoRa, com reconstrução retroativa. RTC externo só será reconsiderado se o método não atender à validação temporal acordada; DS3231 permanece no histórico. `args.unix` e `ts` do contrato TT&C estão em segundos; a integração deve converter unidades explicitamente e preservar o horário local do Payload como informação de bancada.

## Organização real do módulo

```text
obc-dh/
├── .agents/         # Documentação, instruções e painel TODO.
├── .tasks/          # Quatro categorias de tarefas, cada uma com done/.
├── headers/         # Declarações, tipos e interfaces públicas.
│   ├── Pinout.h     # Constantes de GPIO; não inicializa hardware.
│   ├── I2cBus.h     # Configuração e API do serviço I2C.
│   └── I2cScheduling.h # Política de prioridade e expiração.
├── src/
│   ├── main.cpp     # setup() e loop() ainda sem comportamento.
│   └── I2cBus.cpp   # Inicialização e executor exclusivo do I2C.
├── tests/
│   └── I2cSchedulingCheck.cpp # Teste nativo da política I2C.
└── platformio.ini   # Ambiente obc: ESP32/Arduino e diretórios.
```

Revisão de 27/10/2026: serviço I2C implementado e teste nativo aprovado; `main.cpp` ainda não inicializa o serviço e os consumidores não estão integrados. Marcações estão no [TODO](TODO.md); parâmetros e limites estão em [COM-001](../.tasks/01-comunicacao/done/COM-001-servico-i2c.md), [COM-002](../.tasks/01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md), [TES-001](../.tasks/04-testes/done/TES-001-escalonamento-i2c.md).

Criar modelos, implementações e testes conforme os comportamentos forem desenvolvidos, sem arquivos vazios antecipados. Reutilização de ADCS e TT&C, tratamento de estado compartilhado e organização da execução estão na arquitetura. O guia de ferramentas não comprova compilação ou funcionamento do hardware; evidências devem constar das tarefas vinculadas pelo checklist.
