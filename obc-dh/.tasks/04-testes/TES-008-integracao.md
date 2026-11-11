# TES-008 — Boot, comandos, telemetria e convivência

**Estado:** Pendente
**Responsáveis:** OBC-DH e setores

Complemento de limites em 11/11/2026: Espressif32 fixado em 6.6.0, dez
builds aprovados e regressões originais nativas/USB preservadas. As duas
bancadas novas passaram: 40 alterações saturaram o cache com 32 admissões
e oito perdas; depois houve FIFO confirmado, esvaziamento e nova admissão.
JSON de 65.536 bytes e snapshot denso foram aceitos na bancada de memória;
esgotamento retornou NO_MEMORY, preservou estado e permitiu retomada.
Maior q de 82/92 ms e resposta de 171,185/795,705 ms em capacidade/memória.
obc restaurado e verificado. Evidências e limites na
[INT-006](../03-integracao/INT-006-fluxo-de-dados.md#limites-de-capacidade-e-memória--11112026).
Não aprova os cenários completos ou modifica critérios anteriores.

Complemento local de 11/11/2026: `int006-stress-check`/`ObcStressCheck.py`
passou com ESP32/USB, SD/Payload simulados, 59 ACKs, q máximo de 32 ms e
resposta máxima de 1.060,223 ms no timeout de leitura. Exercitou leitura
lenta, busy, erro parcial, sete entregas, confirmação desconhecida/
duplicada/atrasada e continuidade sob NO_SPACE. Os 17 testes TT&C
existentes também passaram na placa com FakeTransport, incluindo fila
comum cheia e prioridade de ACK. Essa saturação não foi demonstrada pela
USB integrada. Firmware obc restaurado e verificado sem periféricos.
Evidências e limites na [INT-006](../03-integracao/INT-006-fluxo-de-dados.md#estresse-adicional-sem-periféricos--11112026).
Critérios e casos anteriores preservados; a tarefa continua pendente.

A execução e o relato dos ensaios cabem ao setor de testes; a análise e as correções de firmware cabem ao responsável OBC-DH, com apoio dos setores envolvidos. Seguir as [responsabilidades e regras de reexecução](../README.md#responsabilidades-correção-e-reexecução).

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

## Atualização dos recortes disponíveis — 11/11/2026

A segunda etapa local da INT-006 acrescentou controle de entregas, diário
FCD1 e ferramentas para arquivos fechados. Os contratos e as instruções
atuais estão em [obc-delivery.md](../../../common/protocol/obc-delivery.md) e
[tools/README.md](../../tools/README.md).

| Recorte | Firmware ou ferramenta disponível | Encaminhamento |
|---|---|---|
| USB e comandos do firmware atual | `obc` e `int006-ttc-check`, com `ObcDownlinkCheck.py` | Executar a evolução de entregas/extração; os roteiros anteriores permanecem históricos e os demais critérios originais continuam exigidos |
| Entregas com ID e retentativas | `int006-delivery-check`, com `ObcDeliveryBenchCheck.py` | Executar o roteiro preparado; transporte, ACK e arquivos usam simulação nessa bancada |
| Extração local e recuperação de pendências | `DeliveryLogs`, `ReadRecords` e `DeliveryLogsCheck.py` | Exercitar arquivos fechados e segmentos mistos FCP1/FCP2/FCP3/FCD1 |
| Entregas e extração por USB | Adaptador OBC ligado a TT&C, `request_log` implementado e nova bancada compilada | Executar `ObcDownlinkCheck.py PORTA --bench` com `int006-ttc-check`; entrada, SD e solo são simulados |
| Enlace real e extração pela estação | APIs TT&C consumidas no OBC e cliente `logs list|get` disponível | Exercitar estação/radio/cartão reais, após correções da estação e montagem disponível |

O roteiro da bancada de entregas está pronto e sua execução permanece
pendente. A extração local usa os arquivos fechados obtidos pelo serviço ou
copiados do cartão. A integração OBC de `request_log`, eventos TX e `cf`
foi implementada em 11/11; os testes nativos e builds passaram, sem execução
da nova versão em placa. A revisão da estação encontrou falhas de
durabilidade, cauda truncada e continuidade da recepção, encaminhadas ao
TT&C. Os testes da estação não são substituídos pelo solo simulado da bancada.

O roteiro atual `ObcDownlinkCheck.py` confere perda do primeiro `cf`,
retentativa com ID/conteúdo preservados, atendimento de ping durante
extração, paginação, EOF, erros e CRC FCD1. Os roteiros anteriores conservam
entradas e expectativas e devem ser associados às versões históricas.
O novo roteiro não substitui todos os cenários originais desta tarefa.

Os cenários de convivência dependem da integração dos consumidores na
INT-005. Modos, proteções e atuações dependem da INT-007. O cenário de boot
seguro depende das condições da INT-003. A montagem completa permite
preparar esses ensaios e disponibilizar os dispositivos aos responsáveis
pelas integrações.

## Checklist de execução por cenário

### Cliente real e TM/tempo por USB — 11/11/2026

`ObcStationCheck.py` passou na bancada e no firmware normal sem
periféricos, sem mudar os roteiros históricos. Usou as APIs reais da
estação para confirmar sete entregas e baixar três arquivos, conferidos
por tamanho, CRC e pelo leitor OBC existente. TM começou com UTC
desconhecido em ambos os boots; rejeições de set_time, época zero,
referência atual e ac=7/depois 0 foram aprovados no recorte simulado.

Foram 75 TCs diretos e 48 TMs na bancada; 64 TCs diretos e 37 TMs no
firmware normal. q máximo de 17/10 ms, RTT máximo de 83,4/49,9 ms e
transporte UART máximo de 2 ms, com pilhas positivas nos serviços ativos.
ACK/lg correlacionados nos logs do PC, incluindo extração, tiveram maior
intervalo de 421 ms na bancada e 50 ms no normal, abaixo dos 2 s existentes.
Procedimentos, hashes, relatórios e downloads estão na
[INT-006](../03-integracao/INT-006-fluxo-de-dados.md#cliente-real-da-estação-e-tmtempo--11112026).
A placa ficou com `obc`.

Esse recorte complementa os casos originais de UTC/TM/extração. Não aprova
produtores de energia/atitude, precisão LoRa, cartão, UART entre placas,
boot seguro, persistência da estação, convivência FOC ou missão completa.
Os critérios e checkboxes completos permanecem preservados.

### Reexecução USB após correção da admissão — 11/11/2026

O mesmo roteiro `ObcDownlinkCheck.py --bench` passou em `int006-ttc-check`,
sem alteração de entradas/expectativas: escritor ativo, sete IDs,
retentativa com ID/conteúdo preservados e três arquivos extraídos com CRC
válido. Foram 46 ACKs, q máximo 32 ms, resposta máxima 486,945 ms e intervalo
máximo do transporte de 2 ms. Todas as pilhas observadas permaneceram
positivas. O firmware normal sem periféricos também passou e ficou na placa.

Relatórios novos e métricas de memória estão na
[INT-006](../03-integracao/INT-006-fluxo-de-dados.md#correção-da-admissão-e-reexecução-usb--11112026).
A reprovação anterior permanece preservada. Este recorte aprovado não
encerra os cenários completos de RF, cartão, Pi, estação, tempo, convivência
ou missão, nem altera os critérios e checkboxes originais da TES-008.

### Evidência parcial USB — 11/11/2026

Histórico da execução anterior à correção da admissão.

Com somente ESP32/USB, o roteiro atual passou no firmware normal sem
periféricos: seis ACKs, q máximo 1 ms, resposta máxima 30,406 ms e `no_sd`
esperado. A bancada `int006-ttc-check` reprovou: a reserva de memória
impediu iniciar o escritor simulado e a listagem retornou `no_sd`. Antes
da falha, foram observados 26 ACKs e retransmissão com ID/conteúdo
preservados. Não houve extração FCD1 ou aprovação do cenário completo.

Diagnóstico, binários e relatórios estão na
[INT-006](../03-integracao/INT-006-fluxo-de-dados.md#execução-usb-em-esp32-isolado--11112026).
O responsável OBC deve analisar a memória antes de nova versão e reexecução.
Nenhum checkbox original foi aprovado por esse recorte. A placa voltou a
`obc`; firmware, roteiro e critérios não foram alterados durante o ensaio.

- [ ] **Boot e reset:** conferir inicialização automática e falhas independentes. Antes de autorizar o caso físico completo, obter estados elétricos e medir motor/Burn Wire antes de o firmware executar, durante reset e após inicialização. Ausência de comandos de atuação no código não comprova saída segura.
- [ ] **TC/ACK e umbilical:** executar comandos suportados, argumentos inválidos e duplicatas; medir entrega e resposta contra ≤200 ms/≤2 s. Registrar origem e ACK correspondente. `q` mede o intervalo informado pelo transporte, não todo o tempo físico de RF/execução. Preservar o teste associado a REQ-OBC-001 e sua relação com REQ-OBC-002.
- [ ] **Compatibilidade de roteiro:** no firmware atual, usar `ObcDownlinkCheck.py` conforme o guia. `ObcDataFlowCheck.py` exige a versão anterior a `request_log` e aos quadros `ac`; `ObcTtcCheck.py` exige a versão INT-004, anterior a `set_time`. Preservar entradas e expectativas desses testes históricos e todos os critérios originais da TES.
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
