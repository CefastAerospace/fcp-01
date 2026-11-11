# TODO — OBC-DH

**Atualização: 11/11/2026, alterações locais sobre `f7af605`.** O firmware central integra entregas identificadas, eventos TX, confirmações do solo e extração `request_log` pelo TT&C. Após corrigir a admissão de memória, a bancada USB de entregas/extração e o firmware normal passaram com o roteiro original. Testes nativos, quatro builds, relatórios e histórico da reprovação estão na INT-006. Aceites, operação, hardware restante, sensores, modos e supervisão continuam abertos.

**Limites e versão:** Espressif32 fixado em 6.6.0; dez ambientes compilados
e regressões nativas/USB preservadas e aprovadas. Saturação na placa:
32 admissões, oito perdas contabilizadas, FIFO confirmado e nova admissão
após esvaziamento. Memória: JSON de 65.536 bytes e snapshot denso aceitos;
NO_MEMORY preservou o snapshot e permitiu retomada. Heap interno mínimo
de 1.040 bytes no esgotamento. Evidências na INT-006; obc restaurado.

**Estresse sem periféricos:** os 17 testes TT&C existentes passaram no
ESP32 com transportes falsos. A bancada USB adicional `int006-stress-check`
passou com timeout/busy/erro parcial, confirmação desconhecida/duplicada/
atrasada e NO_SPACE simulado. Sete entregas confirmadas, ACK máximo de
1.060,223 ms e q máximo de 32 ms. obc restaurado e verificado. Procedimento
e evidências na INT-006; não encerra ensaios físicos nem durabilidade.

**Admissão de memória:** a correção aprovada distingue o teto do parser
(98.304 bytes, com NO_MEMORY previsto) das pilhas e margem de inicialização
(23.552 bytes). A admissão usa heap interno acessível por byte e maior bloco.
O parser, os buffers e os testes não mudaram. A placa ficou com `obc` restaurado.

**Cliente real e TM/tempo:** `ObcStationCheck.py` passou com um ESP32/USB
na bancada e no firmware normal. APIs reais da estação confirmaram sete
entregas e baixaram três arquivos interpretados pelo leitor OBC. UTC
desconhecido, época zero, referência atual, rejeições e ac=7/depois 0 foram
verificados. Evidências na INT-006; durabilidade da estação e periféricos
reais continuam pendentes. A placa permaneceu com `obc`.

Cada entrega aparece uma vez no cadastro das seções 1–4. **Pendente** = não iniciada; **Em andamento** = execução iniciada; **Bloqueada** = impedimento registrado durante a execução; **Concluída** = escopo comprovado e arquivo em `done/`. As matrizes de trabalho/prontidão das seções 5–6 orientam encaminhamentos, sem criar estados novos. Pendente não significa automaticamente disponível.

O encerramento por responsabilidade de 09/11 manteve COM-004/005, DAD-001/003 e TES-004/007 em `done/`, com obrigações externas encaminhadas às INT/TES. As evidências históricas de cada entrega ficam nas próprias tarefas. INT-003 permanece aberta pelo boot de voo; a bancada já foi usada nas integrações posteriores.

Em 10/11, a COM-003 foi concluída no escopo da implementação lógica, com
verificações nativas e bancada ESP32 simulada. A política com os consumidores
segue na INT-005, a supervisão na INT-008 e os ensaios físicos na TES-009.

**Para escolher trabalho:** conferir tarefas predecessoras e condições externas separadamente. Com ambas atendidas, a tarefa pode avançar independentemente das demais. Resolver definições externas pode começar antes da implementação. Em TES-008/TES-009, conferir cada cenário. Números identificam tarefas, não fases obrigatoriamente sequenciais.

Detalhes e histórico: [regras e rastreabilidade](../.tasks/README.md). Requisitos e validação: [REQUISITOS.md](REQUISITOS.md).

## 1. Comunicação e armazenamento

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| COM-001 | [Serviço I2C: inicialização, executor e fila](../.tasks/01-comunicacao/done/COM-001-servico-i2c.md) | — | — | Concluída |
| COM-002 | [Prioridade dinâmica e ordenação I2C](../.tasks/01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md) | COM-001 | — | Concluída |
| COM-003 | [Recuperação do barramento I2C após falhas](../.tasks/01-comunicacao/done/COM-003-recuperacao-i2c.md) | COM-001 | Política dos consumidores em INT-005; supervisão em INT-008; ensaios físicos em TES-009 | Concluída |
| COM-004 | [Transporte UART e recepção validada do Payload](../.tasks/01-comunicacao/done/COM-004-uart-payload.md) | — | Aceite, tráfego e ponta Pi em INT-006; ensaios em TES-005/TES-009 | Concluída |
| COM-005 | [SD: gravação, leitura, extração e recuperação](../.tasks/01-comunicacao/done/COM-005-armazenamento-sd.md) | — | Montagem/EPS em INT-003/TES-006; fluxo e falhas em INT-006/INT-008/TES-009 | Concluída |

## 2. Dados e tempo

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| DAD-001 | [Modelos de dados e formatos persistidos](../.tasks/02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md) | — | Aceites/mapeamentos em INT-004/INT-005/INT-006 | Concluída |
| DAD-002 | [Tempo monotônico, boots e referências absolutas](../.tasks/02-dados-e-tempo/done/DAD-002-tempo-e-boots.md) | — | Contrato local aprovado; precisão física em TES-008 | Concluída |
| DAD-003 | [Codificação e interpretação dos registros](../.tasks/02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) | DAD-001, DAD-002 | Fluxo em INT-006; estados duráveis em INT-007/INT-008; ensaios em TES-009 | Concluída |

## 3. Integração e operação

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| INT-001 | [Configuração base do ambiente OBC](../.tasks/03-integracao/done/INT-001-ambiente-obc.md) | — | — | Concluída |
| INT-002 | [Conciliação de EN do motor no GPIO 12](../.tasks/03-integracao/done/INT-002-pinagem-en-motor.md) | — | — | Concluída |
| INT-003 | [Inicialização central e organização da execução](../.tasks/03-integracao/INT-003-inicializacao-central.md) | INT-001 | Bancada implementada; confirmar placa, ConOps e estados seguros de voo | Em andamento |
| INT-004 | [Integração de TT&C e umbilical](../.tasks/03-integracao/INT-004-ttc-e-umbilical.md) | INT-003 | Aceite OBC dos formatos registrado; aceite conjunto TT&C pendente | Em andamento |
| INT-005 | [Integração de sensores, encoder e ADCS](../.tasks/03-integracao/INT-005-sensores-e-adcs.md) | COM-001, COM-002, INT-002, INT-003 | Inventário e limites dos setores | Pendente |
| INT-006 | [Fluxo Payload–tempo–SD–telemetria](../.tasks/03-integracao/INT-006-fluxo-de-dados.md) | COM-004, COM-005, DAD-003, INT-004 | Cliente real, TM/tempo e extração USB aprovados nos recortes locais; aceites, operação e hardware restante pendentes | Em andamento |
| INT-007 | [Comandos, modos e proteções operacionais](../.tasks/03-integracao/INT-007-comandos-modos-e-protecoes.md) | INT-004, INT-005, INT-006 | ConOps e condições de atuação | Pendente |
| INT-008 | [Supervisão, watchdog e retomada após falhas](../.tasks/03-integracao/INT-008-supervisao-e-retomada.md) | COM-003, INT-007 | Limites de recuperação | Pendente |

## 4. Testes

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| TES-001 | [Teste nativo do escalonamento I2C](../.tasks/04-testes/done/TES-001-escalonamento-i2c.md) | COM-002 | — | Concluída |
| TES-002 | [Executor I2C: concorrência, fila e erros](../.tasks/04-testes/TES-002-executor-i2c.md) | COM-001, COM-002 | Preparar roteiro FreeRTOS; dispensa sensores | Pendente |
| TES-003 | [Comunicação I2C em hardware](../.tasks/04-testes/TES-003-i2c-hardware.md) | COM-001, COM-002 | Dispositivos e montagem | Pendente |
| TES-004 | [Recepção UART: validação e limites](../.tasks/04-testes/done/TES-004-uart-software.md) | COM-004 | Contrato local verificado; interoperabilidade em TES-005 e excesso físico em TES-009 | Concluída |
| TES-005 | [UART entre ESP32 e Raspberry Pi](../.tasks/04-testes/TES-005-uart-hardware.md) | COM-004 | JSON confirmado; aceite do enquadramento, ponta Pi, tráfego e montagem pendentes | Em andamento |
| TES-006 | [SD: operações e falhas em hardware](../.tasks/04-testes/TES-006-sd-hardware.md) | COM-005 | Cartão e montagem | Pendente |
| TES-007 | [Formatos e tempo: interpretação e limites](../.tasks/04-testes/done/TES-007-dados-e-tempo.md) | DAD-003 | Lógica e formatos verificados; precisão/retomada física em TES-008/TES-009 | Concluída |
| TES-008 | [Boot, comandos, telemetria e convivência](../.tasks/04-testes/TES-008-integracao.md) | Por cenário no arquivo | Por cenário | Pendente |
| TES-009 | [Falhas, recuperação e integridade](../.tasks/04-testes/TES-009-falhas-e-recuperacao.md) | Por cenário no arquivo | Por cenário | Pendente |
| TES-010 | [Missão completa e cobertura dos requisitos](../.tasks/04-testes/TES-010-missao-e-cobertura.md) | INT-008; TES-001 a TES-009 | Referências e limites vigentes | Pendente |

## 5. Trabalho OBC-DH com ESP32 e USB

Ordem sugerida para avançar engenharia com poucos dispositivos. “Pode avançar” não significa que a tarefa inteira está liberada: o levantamento de definições pode anteceder predecessoras, mas implementação/aceite mantêm suas dependências.

| Prioridade | Task | Trabalho possível agora | Condição para concluir |
|---|---|---|---|
| 1 | INT-006 | Etapas locais de memória, cliente real, extração USB e TM/tempo concluídas. Fechar com Payload/TT&C origem, tráfego, retenção e operação; 10 s são provisórios. Preparar cartão, RF/Pi e resets reais. | Aceites, produtor Pi, operação e ensaios restantes do escopo; preservar as TES correspondentes. |
| 2 | INT-004 | Preparar o aceite TT&C usando contratos, mapeamentos e evidências USB existentes. | Aceite conjunto pendente; não encerrar automaticamente INT-003 ou ensaios RF. |
| 3 | INT-005 | Consolidar com ADCS/EPS/Térmico a política I2C recebida da COM-003 e integrar os consumidores ao executor. | Inventário, montagem e limites de aquisição/FOC acordados; ensaios físicos seguem nas TES correspondentes. |
| 4 | INT-003/005/007/008 | Levantar esquema da placa, ConOps, inventário, limites FOC/energia/térmico e estados seguros/duráveis com os setores. | Não são concluíveis somente por ESP32/USB; preservam predecessoras, decisões e integrações próprias. |

O engenheiro OBC-DH pode preparar interfaces e evidências; a execução dos novos ensaios é encaminhada ao setor de testes. Roteiros atuais e retorno ao firmware normal estão em [examples/README.md](../examples/README.md#bancada-integrada-int-006). Abertura de contrato não autoriza escolher parâmetros dos outros setores unilateralmente.

Mensagens preparadas para TT&C, Payload e testes, sem envio externo:
[encaminhamentos de 11/11/2026](../encaminhamentos-2026-11-11.md).

## 6. Encaminhamento ao setor de testes

Predecessoras concluídas e condições externas são verificações separadas. Os checklists completos ficam nas tarefas da seção 4; usar o [registro por cenário](../.tasks/README.md#registro-por-cenário).

O setor de testes executa e encaminha acertos, falhas, impedimentos e evidências ao responsável OBC-DH; não corrige firmware nem altera entradas, critérios ou resultados esperados. Análise e correção ficam com OBC-DH, seguidas de reexecução pelo setor de testes. Falhas do próprio escopo mantêm a tarefa aberta, conforme o [fluxo de correção e encerramento](../.tasks/README.md#responsabilidades-correção-e-reexecução).

| Task/cenário | Predecessoras | Encaminhamento atual | Recurso/definição que falta |
|---|---|---|---|
| TES-001/004/007 | Concluídas no escopo próprio | Reutilizar evidências; não reabrir por esta revisão. | As verificações portáteis não substituem seus ensaios físicos destinatários. |
| TES-002 | COM-001/002 concluídas | Encaminhar agora para preparar e executar bancada FreeRTOS real; sensores dispensados. | Roteiro/entrada isolada ainda precisa ser preparado pelo setor de testes. |
| TES-003 | COM-001/002 concluídas | Encaminhar preparação; executar por dispositivo disponível e montagem aprovada. | Dispositivos, pull-ups, instrumentação e critérios dos setores. |
| TES-005 | COM-004 concluída | Encaminhar preparação do ensaio ESP32–Pi. | Produtor/porta Pi, aceite FCPU ou conciliação de interface, taxas/rajadas/retenção e montagem. JSON é o conteúdo vigente. |
| TES-006 | COM-005 concluída | Encaminhar preparação das operações/extração por cartão ou API de arquivo fechado. | Cartão/módulo, alimentação/montagem e critérios com EPS. |
| TES-008: USB e simulação de fluxo/tempo | INT-003/004/006 têm entregas parciais verificadas | Recortes de bancada disponíveis com ESP32/USB e roteiro compatível; registrar evidência parcial. | Aceites e dependências formais continuam para os cenários completos; não aprovar reset seguro, RF ou SD físico por esses recortes. |
| TES-008: demais cenários | Dependências por cenário ainda abertas | Preparar procedimento e limites. | Boot de voo, FOC/sensores, RF, cartão, modos/energia/térmico/atuações e respectivas aprovações. |
| TES-009: UART e interrupção SD | COM-004/005 concluídas | Preparar separadamente os ensaios físicos. | Ponta/gerador compatível, taxas e montagem UART; cartão, corte aprovado e critérios EPS. |
| TES-009: recuperação I2C, watchdog e retomada | COM-003 concluída; INT-007/INT-008 pendentes; INT-006 parcial | Preparar o cenário físico I2C e levantar procedimentos dos demais cenários. | Acordo e montagem I2C; supervisão e estados duráveis das integrações. |
| TES-010 | INT-008 e parte de TES-001 a TES-009 ainda abertas | Preparar rastreabilidade e procedimento; missão completa ainda não liberada. | Predecessoras, referências/limites vigentes e montagem integrada. |

Nenhuma linha desta matriz é evidência de novo ensaio. A auditoria de links e consistência documental é distinta da validação funcional ou física.
