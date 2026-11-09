# OBC-DH — documentação e orientação

Software de bordo em C++ para o ESP32, com Arduino/FreeRTOS. O OBC-DH integra os módulos dos setores, organiza telecomandos (TC), telemetria (TM), referência temporal e registros da missão. O Payload executa no Raspberry Pi e se comunica por UART; o ADCS será integrado ao mesmo ESP32 do OBC.

**Estado em 09/11/2026:** COM-004/COM-005, DAD-001/DAD-003 e
TES-004/TES-007 estão concluídas em seu escopo próprio. Aceites, montagem,
ponta Pi, fluxo integrado e estados duráveis continuam nas INT/TES
destinatárias vinculadas no [painel](TODO.md). As descrições datadas
abaixo preservam o histórico; conclusão local não declara missão validada.

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

Consolidação de interfaces: UART Payload TX 17 / RX 16, 115,2 kbps 8N1; LoRa no VSPI, SD no HSPI; encoder AS5600/I2C `0x36`. Em 05/11/2026, o responsável substituiu o binário compacto decidido em 14/10 por snapshots JSON completos na UART. Enquadramento, integridade, perdas e repetição ficam em COM-004. TT&C usa JSON, `Send()`/`SendTelemetry()`, quadro ≤255 bytes sem fragmentação, CRC do rádio e tarefa própria via `Start()`. EPS já oferece biblioteca INA219; integração do OBC continua pendente. `PENDENCIAS_ARQUITETURAIS.md` foi incorporado ao antigo TODO; suas pendências agora estão distribuídas nas tarefas.

Tempo de voo usa ms monotônicos por boot e referências absolutas pelo LoRa, com reconstrução retroativa. RTC externo só será reconsiderado se o método não atender à validação temporal acordada; DS3231 permanece no histórico. `args.unix` e `ts` do contrato TT&C estão em segundos; a integração deve converter unidades explicitamente e preservar o horário local do Payload como informação de bancada.

## Organização real do módulo

```text
obc-dh/
├── .agents/         # Documentação, instruções e painel TODO.
├── .tasks/          # Quatro categorias de tarefas, cada uma com done/.
├── headers/         # Declarações, tipos e interfaces públicas.
│   ├── Pinout.h     # Constantes de GPIO; não inicializa hardware.
│   ├── I2cBus.h     # Configuração e API do serviço I2C.
│   ├── I2cScheduling.h # Política de prioridade e expiração.
│   ├── ObcTime.h     # Contrato temporal portátil e reserva de boots.
│   ├── ObcTimeEsp32.h # Timer/NVS chamados pelo boot de bancada.
│   ├── ObcStartup.h # Coordenação e relatório do boot de bancada.
│   ├── ObcStorage.h # Núcleo POSIX de arquivos e repetição limitada.
│   ├── ObcStorageEsp32.h # SD/HSPI e mutex estático.
│   ├── ObcRecordCodec.h # Codec portátil v1 e snapshots v2, com CRC.
│   └── models/ObcData.h # Modelos portáteis de registros v1 e snapshots.
├── src/
│   ├── main.cpp     # Boot de bancada e espera de 100 ms no loop.
│   ├── ObcStartup.cpp # Tempo/NVS, I2C e SD com falhas independentes.
│   ├── I2cBus.cpp   # Inicialização e executor exclusivo do I2C.
│   ├── ObcTime.cpp # Lógica temporal sem hardware.
│   ├── ObcTimeEsp32.cpp # Timer de 64 bits e NVS.
│   ├── ObcStorage.cpp # Escrita/sincronização e leitura por offset.
│   ├── ObcStorageEsp32.cpp # Montagem e coordenação do SD.
│   ├── ObcData.cpp # Validação dos modelos, sem drivers.
│   └── ObcRecordCodec.cpp # Layout binário explícito e CRC.
├── tests/
│   ├── I2cSchedulingCheck.cpp # Teste nativo da política I2C.
│   ├── ObcTimeCheck.cpp # Referências e persistência simulada.
│   ├── ObcDataCheck.cpp # Modelos, disponibilidade e limites.
│   ├── PayloadSnapshotCheck.cpp # Snapshots, extras e limites em memória.
│   ├── ObcStorageCheck.cpp # Arquivos reais e falhas POSIX injetadas.
│   ├── ObcStartupCheck.cpp # Ordem e oito combinações do boot.
│   ├── ObcRecordCodecCheck.cpp # Vetores independentes e limites do codec.
│   ├── SnapshotCodecCheck.cpp # Vetores e saídas preservadas do codec v2.
│   ├── SnapshotRecordsCheck.py # Gera vetores e verifica leitor v1/v2/misto.
│   └── ReadRecordsCheck.py # Executa/verifica o leitor C++ real.
├── tools/          # Leitor nativo e instruções para arquivos extraídos.
└── platformio.ini   # Ambiente obc: ESP32/Arduino e diretórios.
```

Revisão de 27/10/2026: serviço I2C implementado e teste nativo aprovado; `main.cpp` ainda não inicializa o serviço e os consumidores não estão integrados. Marcações estão no [TODO](TODO.md); parâmetros e limites estão em [COM-001](../.tasks/01-comunicacao/done/COM-001-servico-i2c.md), [COM-002](../.tasks/01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md), [TES-001](../.tasks/04-testes/done/TES-001-escalonamento-i2c.md).

Criar modelos, implementações e testes conforme os comportamentos forem desenvolvidos, sem arquivos vazios antecipados. Reutilização de ADCS e TT&C, tratamento de estado compartilhado e organização da execução estão na arquitetura. O guia de ferramentas não comprova compilação ou funcionamento do hardware; evidências devem constar das tarefas vinculadas pelo checklist.

Revisão de 01/11/2026: `ObcTime` implementa reconstrução temporal portátil e reserva de boots; `ObcTimeEsp32` fornece timer de 64 bits e NVS. Teste nativo temporal e build ESP32 aprovados, conforme [DAD-002](../.tasks/02-dados-e-tempo/done/DAD-002-tempo-e-boots.md). Esses módulos ainda não estão conectados ao boot, rádio ou registros; precisão e persistência física continuam sem ensaio nesta entrega.

DAD-001 tem modelos/validadores portáteis, teste nativo e build ESP32 aprovados. O [contrato OBC](../../common/protocol/obc-data.md) define representação lógica e layout persistente; permanece em andamento até o aceite dos produtores. A entrega inicial de 01/11/2026 preservou os formatos então vigentes. Codec e acesso ao SD ficam nas tarefas correspondentes.

DAD-003 acrescenta `ObcRecordCodec` e o [leitor nativo](../tools/README.md), sobre os mesmos modelos e layouts. A implementação local não encerra a tarefa: o aceite de DAD-001 e a definição dos estados duráveis de retomada continuam pendentes. Não há integração de armazenamento físico ou atuação do Burn Wire.

Revisão de 05/11/2026: DAD-001 acrescenta modelos em memória e validadores de snapshots completos, com extras textuais, reutilizando tempo/status/bits de posição existentes. A nova verificação nativa é `tests/PayloadSnapshotCheck.cpp`. Não há parser ou persistência de snapshots; codec v1, testes anteriores e script do Payload permanecem preservados. Limites e lacunas de contrato estão em [obc-data.md](../../common/protocol/obc-data.md#0-snapshot-do-payload--modelo-em-memória).

COM-005 acrescenta `ObcStorage` e `ObcStorageEsp32`, com arquivos exclusivos, sincronização por bloco, até três tentativas no mesmo offset, leitura por blocos e erros explícitos. Teste nativo e build ESP32 aprovados; módulo/alimentação com EPS e ensaios físicos permanecem pendentes. INT-003 chama a montagem na bancada, sem geração de registros ou transporte de extração. Contrato operacional e evidências estão na [tarefa](../.tasks/01-comunicacao/done/COM-005-armazenamento-sd.md); uso e verificações no [guia de ferramentas](../tools/README.md#serviço-de-arquivos-sdhspi).

Entrega posterior de 05/11/2026: INT-003 liga os inicializadores existentes ao `setup()` por meio de `ObcStartup`. USB/Serial começa em 115200 baud; tempo/NVS, I2C e SD são tentados nessa ordem, com diagnóstico das falhas e sem repetição/reset. `loop()` cede execução a cada 100 ms. A montagem SD não abre arquivos ou grava registros. Teste nativo do coordenador e build ESP32 aprovados; INT-003 permanece em andamento pelas definições do boot integrado. As revisões anteriores acima descrevem o estado de suas próprias entregas; a integração atual de bancada está documentada na [INT-003](../.tasks/03-integracao/INT-003-inicializacao-central.md).

Extensão posterior de 05/11/2026: DAD-003 acrescenta formato persistido v2 dos snapshots e extras, de 37–64497 bytes, com APIs no codec existente e leitura nativa v1/v2/mista. O v1, modelos, drivers e testes anteriores foram preservados. Novos testes independentes e build ESP32 aprovados; consolidação de DAD-001 e estados duráveis de retomada continuam pendentes. Contrato e limites de memória estão em [obc-data.md](../../common/protocol/obc-data.md#persistência-de-snapshots--v2); comandos e uso no [guia](../tools/README.md#snapshots-v2). Esta extensão resolve a ausência de codec de snapshots descrita nas entregas anteriores, sem implementar UART ou gravação automática.

Entrega de 09/11/2026: COM-004 acrescenta `PayloadReceiver`,
`PayloadReceiverEsp32`, testes nativos independentes e autoverificação ESP32
para [FCPU v1](../../common/protocol/payload-uart.md). O JSON existente é o
corpo; controle de transporte é externo. O contexto reserva uma vez dois
blocos no heap e publica somente após validação integral. Amostra, limites,
erros, repetição, CRC e timeouts passaram nos testes; a amostra foi executada
na placa com medição de memória/pilha. O beta de loopback permanece intacto,
isolado no ambiente `uart-loopback`. Os ambientes, comandos, logs e retorno
ao firmware normal estão em [examples/README.md](../examples/README.md).
O boot normal não inicia o receptor e o produtor Pi não foi alterado.
COM-004 e TES-004 tiveram seu escopo próprio encerrado na auditoria posterior
de 09/11. Aceite/parâmetros/ponta Pi ficam em INT-006 e ensaios físicos em
TES-005/TES-009. TES-005 permanece em andamento; critérios foram preservados.
