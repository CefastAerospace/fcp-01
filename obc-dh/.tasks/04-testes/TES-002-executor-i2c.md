# TES-002 — Executor I2C: concorrência, fila e erros

**Estado:** Pendente
**Responsáveis:** OBC-DH

## Objetivo

Comprovar exclusividade e conclusão correta das solicitações concorrentes.

## Entrega

Exercitar múltiplos chamadores, capacidade de oito posições, fila cheia, retorno de falha, argumentos/contextos inválidos e serviço não inicializado. Conferir que pedido vencido não executa, operação iniciada preserva seu contexto e o resultado chega ao chamador correto. Não exigir sensores nem integração do main; não confundir esse ensaio com [TES-001](done/TES-001-escalonamento-i2c.md).

## Pré-requisitos

- **Tarefas:** [COM-001](../01-comunicacao/done/COM-001-servico-i2c.md), [COM-002](../01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md)
- **Condições externas:** Ambiente capaz de exercitar o executor FreeRTOS; callbacks de teste finitos.

## Critérios de conclusão

Registrar ambiente, procedimento, entradas, resultados esperados/observados e versão do código. Todas as condições do contrato do executor exercitadas devem passar; desvios exigem diagnóstico antes de correção.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- [obc-dh/headers/I2cBus.h](../../headers/I2cBus.h)
- [obc-dh/src/I2cBus.cpp](../../src/I2cBus.cpp)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
