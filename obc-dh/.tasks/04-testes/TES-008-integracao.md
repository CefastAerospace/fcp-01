# TES-008 — Boot, comandos, telemetria e convivência

**Estado:** Pendente
**Responsáveis:** OBC-DH e setores

A execução dos novos ensaios será encaminhada ao setor de testes; os responsáveis técnicos acima apoiam interfaces, condições e diagnóstico.

## Objetivo

Demonstrar o funcionamento conjunto por cenários independentes.

## Entrega

Executar cada cenário assim que suas dependências estiverem prontas, preservando os critérios de REQUISITOS.md. Registrar resultados individualmente. Não aguardar todas as integrações para iniciar boot ou TC/ACK.

## Pré-requisitos

- **Tarefas:** Por cenário, conforme tabela abaixo; não exigem conclusão conjunta para iniciar.
- **Condições externas:** Por cenário: montagem conferida, limites definidos e integrações correspondentes disponíveis.

## Definições e condições

- Preservar associação do teste TC a REQ-OBC-001, registrando relação com REQ-OBC-002 e evidência complementar de autonomia sem substituir o teste original.

## Cenários independentes

| Cenário | Pré-requisitos | Condições e verificação | Estado |
|---|---|---|---|
| Boot e reset sem atuação indevida de motor/Burn Wire | [INT-003](../03-integracao/INT-003-inicializacao-central.md) | Montagem e estados elétricos aprovados; conferir antes e depois da execução do firmware. | Pendente |
| TC/ACK e umbilical | [INT-004](../03-integracao/INT-004-ttc-e-umbilical.md) | Injetar TC e medir execução; preservar associação a REQ-OBC-001 e relação com REQ-OBC-002. Entrega ≤200 ms e resposta ≤2 s conforme TT&C. | Pendente |
| Convivência FOC, I2C, rádio e armazenamento | [INT-004](../03-integracao/INT-004-ttc-e-umbilical.md), [INT-005](../03-integracao/INT-005-sensores-e-adcs.md), [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Limites ADCS acordados; medir atendimento do controle sob operações de rádio e SD, sem presumir que SPI separado elimina concorrência. | Pendente |
| Fluxo ADS-B, TM e extração dos registros | [INT-006](../03-integracao/INT-006-fluxo-de-dados.md) | Injetar ADS-B simulado e demonstrar timestamp correto e legível na TM; inspecionar estrutura, integridade e ordenação de status/energia/atitude. Conferir registros e estatísticas extraídos. | Pendente |
| Referência temporal real via LoRa | [INT-006](../03-integracao/INT-006-fluxo-de-dados.md) | Medir erro/resolução e efeito da latência/quantização contra os limites temporais, incluindo período sem sincronização e resets. | Pendente |
| Modos, energia, térmico e Burn Wire | [INT-007](../03-integracao/INT-007-comandos-modos-e-protecoes.md) | Condições físicas e operacionais aprovadas. Conferir transições, recusa/duplicatas de comandos, retomada e consumo contra a cota EPS; preservar o mínimo de cinco ciclos bem-sucedidos do Burn Wire e confirmação física. | Pendente |

## Critérios de conclusão

Todos os cenários desta tarefa aprovados contra os critérios originais e limites acordados. Resultados parciais não encerram a tarefa; não substituem a demonstração de autonomia de [TES-010](TES-010-missao-e-cobertura.md).

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [TES-007](done/TES-007-dados-e-tempo.md) | Medir precisão/resolução temporal real pelo LoRa e demonstrar timestamp no fluxo ADS-B–TM, além da coexistência dos recursos. | Pendente |

## Prontidão por cenário — 10/11/2026

Os pré-requisitos formais da tabela original continuam válidos para concluir cada cenário. As entregas parciais permitem preparar e registrar verificações de bancada antes do encerramento global das INT, sem declarar aprovação física ou substituir suas condições externas.

| Recorte | O que já pode ser preparado/exercitado | O que impede o aceite completo |
|---|---|---|
| Boot | Diagnóstico, NVS e atendimento USB no ESP32 | INT-003 aberta: ConOps, montagem e níveis de motor/Burn Wire antes/durante/depois de reset |
| TC/ACK/umbilical | INT-004 tem evidência USB; firmware atual usa roteiro INT-006 | Aceite TT&C; RF e medição integrada conforme condições do cenário |
| ADS-B, UTC e TM | `int006-check` injeta bytes em flash com SD lento simulado | INT-006 aberta: produtor Pi, cartão, downlink agregado e extração por umbilical |
| Referência LoRa | Lógica temporal e `set_time` estão implementados | Rádio/estação, referência de medição e limites temporais aprovados |
| Convivência FOC/I2C/rádio/SD | Preparar instrumentação e limites com ADCS | INT-005 e montagem integrada ainda pendentes |
| Modos, energia, térmico e Burn Wire | Levantar contratos e procedimento com os responsáveis | INT-007, ConOps, limites e condições físicas ainda pendentes |

## Checklist de execução por cenário

- [ ] **Boot e reset:** conferir inicialização automática e falhas independentes. Antes de autorizar o caso físico completo, obter estados elétricos e medir motor/Burn Wire antes de o firmware executar, durante reset e após inicialização. Ausência de comandos de atuação no código não comprova saída segura.
- [ ] **TC/ACK e umbilical:** executar comandos suportados, argumentos inválidos e duplicatas; medir entrega e resposta contra ≤200 ms/≤2 s. Registrar origem e ACK correspondente. `q` mede o intervalo informado pelo transporte, não todo o tempo físico de RF/execução. Preservar o teste associado a REQ-OBC-001 e sua relação com REQ-OBC-002.
- [ ] **Compatibilidade de roteiro:** no firmware atual, usar `ObcDataFlowCheck.py` conforme o guia; `ObcTtcCheck.py` exige a versão INT-004, anterior ao suporte a `set_time`. Não alterar o teste antigo para aprovar a evolução.
- [ ] **Tempo e indisponibilidade:** após boot novo, verificar UTC desconhecido (`ts:null`); aplicar referência válida e demonstrar timestamp legível na TM com ADS-B simulado. Conferir época zero, segundos no TT&C e ms internos; novo boot não pode herdar UTC fictício. ACK de `set_time` não comprova persistência no SD.
- [ ] **Fluxo e extração:** com as pontas disponíveis, comparar snapshots, registros v1/v2/v3, referências, TM e estatísticas extraídas. Esperar conteúdo reproduzível e perdas explícitas; ACK UART, enfileiramento de TM e recepção no solo são evidências diferentes. Snapshots não encerram a pendência de cada mensagem ADS-B intermediária.
- [ ] **Inspeção de TM:** verificar estrutura, integridade e ordenação de status/energia/atitude conforme REQ-OBC-003; fontes indisponíveis devem permanecer identificadas. TM de bancada com `null` não comprova os produtores ainda não integrados.
- [ ] **Referência real LoRa:** medir erro, resolução, latência, quantização e deriva contra os limites acordados, antes/depois da sincronização e após reset. Resolução de 1 ms e compensação estimada de 162 ms não são garantias de precisão.
- [ ] **Convivência:** executar FOC, aquisições I2C, RX/TX e escrita/leitura SD no perfil acordado. Medir atendimento do controle, filas, memória e perdas; barramentos SPI separados não substituem a medição dos recursos compartilhados.
- [ ] **Modos e proteções:** aplicar transições válidas, comandos fora das condições permitidas, duplicatas e reset, comparando saídas e retomada com ConOps. Medir consumo contra a cota EPS e resposta térmica contra limiares aprovados; sem limites, o caso não pode ser aprovado.
- [ ] **Burn Wire:** executar somente sob autorização e montagem aprovadas, com interlock de TX, limite de duração e critério físico de liberação. Preservar o mínimo de cinco ciclos bem-sucedidos e verificar proteção contra repetição indevida, inclusive após reset. Evento gravado não confirma liberação física.

O [guia de bancada](../../examples/README.md#bancada-integrada-int-006) descreve os recortes ESP32/USB. Registrar [evidências](../README.md#registro-por-cenário) separadas para simulação e montagem real. Todos os cenários originais precisam de aceite para concluir a task; autonomia completa permanece na TES-010.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-001, REQ-OBC-002, REQ-OBC-003, REQ-OBC-004, REQ-OBC-006, REQ-OBC-008, REQ-OBC-012, REQ-NF-OBC-001. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [tt-c/ENSAIOS.md](../../../tt-c/ENSAIOS.md)
- Origem no planejamento anterior: P-VALIDACAO.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
