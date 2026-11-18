# TES-002 — Executor I2C: concorrência, fila e erros

**Estado:** Pendente
**Responsável pela execução e comprovação:** Setor de testes
**Responsáveis técnicos de apoio:** OBC-DH
**Responsável pelas ferramentas e regressões de desenvolvimento:** OBC-DH

A execução e o relato dos ensaios cabem ao setor de testes; a análise e as correções de firmware cabem ao responsável OBC-DH, com apoio dos setores envolvidos. Seguir as [responsabilidades e regras de reexecução](../README.md#responsabilidades-correção-e-reexecução).


## Preparação de ferramenta — responsabilidade OBC, 14/11/2026

Mapear cada caso do checklist abaixo ao código e ao roteiro. A bancada de recuperação já cobre retorno de operação iniciada, recursão, erros e recuperação; completar especialmente capacidade das oito posições/nono pedido, reuso, exclusividade/resultados por chamador, argumentos, ISR e ordenação efetiva. A simulação nativa atual retorna `false` em `xPortInIsrContext()`; não declarar o caso ISR aprovado por ela. Preservar vetores/expectativas da bancada existente e acrescentar casos isolados, finitos e sincronizados. Entregar firmware, roteiro e instruções ao setor de testes; preparação ou regressão OBC não preenche os checkboxes de comprovação.

## Objetivo

Comprovar exclusividade e conclusão correta das solicitações concorrentes.

## Entrega

Exercitar múltiplos chamadores, capacidade de oito posições, fila cheia, retorno de falha, argumentos/contextos inválidos e serviço não inicializado. Conferir que pedido vencido não executa, operação iniciada preserva seu contexto e o resultado chega ao chamador correto. Não exigir sensores nem integração do main; não confundir esse ensaio com [TES-001](done/TES-001-escalonamento-i2c.md).

## Pré-requisitos

- **Tarefas:** [COM-001](../01-comunicacao/done/COM-001-servico-i2c.md), [COM-002](../01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md)
- **Condições externas:** Ambiente capaz de exercitar o executor FreeRTOS; callbacks de teste finitos.

## Critérios de conclusão

Registrar ambiente, procedimento, entradas, resultados esperados/observados e versão do código. Todas as condições do contrato do executor exercitadas devem passar; desvios exigem diagnóstico antes de correção.

## Prontidão e preparação para o setor de testes

COM-001 e COM-002 estão concluídas. É possível encaminhar esta tarefa agora: basta ESP32 com FreeRTOS e USB, sem sensores. O ambiente `i2c-recovery-check` já usa o executor real com FreeRTOS e backend de sinais simulado, mas cobre apenas parte deste checklist. OBC-DH deve reutilizar essa base e completar a ferramenta de ensaio; o setor de testes executa o procedimento e registra a comprovação. TES-001 verifica somente a política e não substitui esta bancada.

Executar primeiro o caso sem `Init`, antes que o firmware central inicialize o serviço. Nos demais casos, inicializar uma vez antes dos chamadores. Coordenar as tarefas da bancada com semáforos e contadores fora dos callbacks; não usar callbacks infinitos, logs dentro do callback, alterações de produção ou acesso concorrente a `Wire`. Registrar ordem, identidade do chamador, chamadas ao callback, status e duração. `context == nullptr` é permitido quando o callback não precisa de dados.

## Checklist de execução

- [ ] **Serviço não inicializado:** chamar `Execute` em contexto de tarefa antes de `Init`; esperar `NOT_INITIALIZED` e zero execução do callback. Comprova sinalização antes de usar recursos.
- [ ] **Inicialização e sucesso:** inicializar e executar callback finito que retorna `true`; esperar `OK` e uma execução. Repetir `Init` com serviço pronto; não duplicar executor ou execuções.
- [ ] **Exclusividade e resultado por chamador:** iniciar múltiplos chamadores com contextos identificáveis; esperar no máximo um callback ativo e resultado entregue ao contexto correto, sem mistura ou execução duplicada.
- [ ] **Capacidade e reuso:** ocupar as oito posições e apresentar um nono pedido enquanto todas permanecem ocupadas; esperar `QUEUE_FULL`, sem executar o nono callback. Contar a operação ativa e conclusões ainda não consumidas na capacidade. Após liberar posições, novo pedido deve completar normalmente.
- [ ] **Vencimento na fila:** manter um pedido aguardando até seu prazo vencer; esperar `QUEUE_TIMEOUT` e nenhuma ação desse callback. Registrar a ordem para distinguir espera de execução.
- [ ] **Operação já iniciada:** usar callback finito que conclui após o prazo de espera da fila; o chamador deve continuar aguardando e receber o resultado final, com contexto válido até o retorno. Timeout da fila não pode produzir retorno antecipado deixando o callback usar dados destruídos.
- [ ] **Falha da operação:** callback retorna `false`; esperar `OPERATION_FAILED` somente no chamador correspondente. Um próximo pedido válido deve continuar funcionando.
- [ ] **Argumentos inválidos:** após inicialização, testar callback nulo, prioridade acima de 4 e espera de fila zero ou superior a 2147483647 ms; esperar `INVALID_ARGUMENT`, sem callback. Prioridades 0 e 4 e esperas dentro da faixa são válidas; não exigir aguardar o prazo máximo para verificar sua aceitação.
- [ ] **Contextos de chamada inválidos:** testar chamada em ISR e chamada recursiva a partir do executor; esperar `INVALID_CONTEXT` sem bloqueio ou nova operação. Não confundir com contexto de dados nulo permitido.
- [ ] **Ordenação no executor:** observar prioridade, promoção após callback concluído, desempate por chegada e ausência de preempção de operação ativa conforme TES-001/contrato. Pedidos vencidos não devem executar nem provocar promoção por conclusão inexistente.

Aprovar rejeições esperadas como sucesso do caso. Divergência, bloqueio ou acesso após retorno exigem diagnóstico antes de corrigir código ou alterar o procedimento. Registrar resultados no formato comum de [evidências](../README.md#registro-por-cenário).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- [obc-dh/headers/I2cBus.h](../../headers/I2cBus.h)
- [obc-dh/src/I2cBus.cpp](../../src/I2cBus.cpp)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
