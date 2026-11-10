# OBC-DH — documentação e orientação

Software de bordo em C++ para ESP32, com Arduino/FreeRTOS. O OBC-DH coordena telecomandos, telemetria, tempo e registros. O Payload executa no Raspberry Pi e se comunica por UART; o ADCS será integrado ao ESP32 central.

**Revisão documental: 10/11/2026, sobre o firmware de `839a186`.** Esta revisão organiza o estado comprovado nas tarefas; não acrescenta execução de testes ou aceite de outros setores.

## Mapa dos documentos

| Documento | Fonte principal de informação |
|---|---|
| [AGENTS.md](AGENTS.md) | Instruções de trabalho e limites de execução. |
| [CPP_STANDARDS.md](CPP_STANDARDS.md) | Convenções de C++ e documentação do código. |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Responsabilidades, decisões vigentes, comportamento implementado e integrações previstas. |
| [REQUISITOS.md](REQUISITOS.md) | Requisitos, critérios originais e referências históricas preservadas. |
| [DEFINICOES_OBC_DP.md](DEFINICOES_OBC_DP.md) | Definições para o Design Package, custos e hipóteses do data budget. |
| [TODO.md](TODO.md) | Estado das tarefas, dependências, prioridades com poucos dispositivos e prontidão dos ensaios. |
| [Tarefas](../.tasks/README.md) | Escopo, decisões pendentes, checklists e evidências; concluídas em `done/`. |
| [PINAGEM.md](PINAGEM.md) | GPIOs, endereços e restrições elétricas. |
| [FUNCIONAMENTO_PIO.md](FUNCIONAMENTO_PIO.md) | Ambiente, compilação, upload, escolha dos roteiros e editor. |
| [Ferramentas](../tools/README.md) | Armazenamento, codecs e leitura de registros v1/v2/v3. |
| [Bancadas](../examples/README.md) | Loopback, receptor e fluxo integrado, com procedimentos e limitações. |

Consultar instruções e padrões, arquitetura/requisitos e a task correspondente antes de trabalhar. Para bancada, conferir pinagem e selecionar o ambiente e o roteiro compatíveis. Obter ConOps, ICDs, regulamento e arquitetura CEFAST vigentes antes dos trabalhos que dependem deles; as referências citadas não constituem cópias disponíveis ou aceites registrados.

Decisões ficam na arquitetura; formatos nos contratos compartilhados; procedimentos nos guias; pendências e evidências nas tarefas. Resumos devem apontar para essas fontes. Decisão, implementação, aceite entre setores e validação física são estados distintos.

## Estado atual e limites

`main.cpp` inicia Serial a 115200 baud, reserva o boot na NVS, inicializa o serviço I2C e tenta montar o SD por `ObcStartup`. Depois inicia TT&C e a primeira etapa de `ObcDataFlowEsp32`. O `loop()` atende telecomandos e diagnósticos; não contém mais a espera de 100 ms da entrega inicial de INT-003. Falhas dos serviços independentes são diagnosticadas. Boot inválido ou falha de recursos pode impedir o fluxo; não fabricar identificação ou UTC.

TT&C conserva sua tarefa e suas filas. O firmware atende `ping`, `get_status`, `request_tm` e `set_time`. TM é enviada sob demanda, no modo `bench`; dados indisponíveis são `null`. `set_time` aplica UTC em RAM e tenta registrar a referência separadamente. `request_log`, envio agregado de aeronaves, modos de voo, sensores, atuadores e supervisão completa ainda dependem das respectivas integrações.

UART2 tem transporte próprio; processamento e escritor SD usam tarefas separadas. O receptor aceita snapshots JSON completos no enquadramento FCPU local. ACK confirma aceitação em RAM. A gravação usa registros binários versionados, fila limitada e bloco exclusivo de snapshot. SD ausente, lento ou falho pode causar descartes contabilizados, sem exigir interromper recepção/TM. Isso não promete ausência de perdas ou recuperação automática.

Os [contratos](../../common/protocol/obc-data.md) distinguem atualizações individuais v1 preservadas, snapshots v2 e TM v3. O leitor aceita arquivos mistos e exporta JSON Lines sem alterar a origem. Extração por `request_log` continua pendente; leitura de uma cópia obtida do cartão já está disponível.

O [FCPU](../../common/protocol/payload-uart.md) transporta JSON com cabeçalho, CRC e controles binários pelo cabo OBC–Pi. Não é somente uma representação interna. O produtor Pi e o aceite do Payload continuam em INT-006; interoperabilidade física fica em TES-005/TES-009. O enlace com o solo usa o [contrato JSON TT&C](../../common/protocol/ttc-json.md), com corpo ≤245 bytes e quadro ≤255 bytes, sem fragmentação. Esses limites não se aplicam aos snapshots UART ou aos arquivos.

Tempo usa ms monotônicos por boot e referências absolutas, com reconstrução sem reescrever o histórico. `args.unix` e `ts` do TT&C estão em segundos. Antes da referência válida, UTC é desconhecido; não inferir o intervalo desligado a partir de outro boot. Resolução lógica e compensação estimada não comprovam precisão física. RTC externo permanece fora da configuração atual.

## Organização real do módulo

O mapa abaixo agrupa os módulos atuais; não é um inventário exaustivo de arquivos.

| Camada | Arquivos e responsabilidade |
|---|---|
| Entrada e boot | `src/main.cpp`, `ObcStartup`: ordem, resultados independentes e diagnóstico inicial. |
| I2C | `I2cBus`, `I2cScheduling`: executor exclusivo, fila e prioridade; consumidores dos setores ainda não integrados. |
| Tempo | `ObcTime`, `ObcTimeEsp32`: reconstrução portátil, timer e reserva NVS. |
| Dados | `headers/models/ObcData.h`, `ObcData`, `ObcRecordCodec`: modelos, validadores e codecs v1/v2/v3. |
| Arquivos | `ObcStorage`, `ObcStorageEsp32`: I/O e SD/HSPI; não definem o conteúdo dos registros. |
| Payload | `PayloadReceiver`, `PayloadReceiverEsp32`: validação, publicação e transporte UART2. |
| Integração | `ObcTtc`, `ObcDataFlow`, `ObcDataFlowEsp32`: comandos, TM, filas e diário por boot/segmento. |
| Bancadas | `src/bench/`: entradas isoladas; `examples/` conserva os procedimentos e logs UART. |
| Verificação | `tests/`: verificações portáteis e roteiros USB, cada qual com seu escopo. |
| Análise | `tools/`: leitor nativo, geradores de vetores e hook controlado de ArduinoJson. |
| Configuração | `platformio.ini`: ambiente padrão `obc` e três ambientes de bancada. |

Os históricos de entrega, hashes e medições permanecem nas tarefas. Os registros de 08/10 em `.codex/` são históricos e não substituem este mapa ou o painel vigente. Não usar descrições antigas de `main` vazio, ausência de parser ou falta de integração como diagnóstico do firmware atual.

## Evidências e próximos trabalhos

COM-001/002/004/005, DAD-001/002/003, INT-001/002 e TES-001/004/007 estão concluídas nos escopos registrados. INT-003/004/006 conservam entregas parciais e pendências próprias. As verificações da primeira etapa INT-006 incluem bancada com entrada em flash/SD simulado e firmware normal sem Pi/cartão; não comprovam aquisição ADS-B, SD físico, RF ou estados seguros das saídas.

Os checklists e a prontidão por cenário estão nas TES e no [painel](TODO.md). Novos resultados devem registrar versão, procedimento, entradas, esperado, observado e limitações. Relatórios em `/tmp` são saídas locais regeneráveis, não anexos permanentes disponíveis a toda a equipe.
