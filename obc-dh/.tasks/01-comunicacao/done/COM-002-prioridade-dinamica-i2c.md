# COM-002 — Prioridade dinâmica e ordenação I2C

**Estado:** Concluída
**Responsáveis:** OBC-DH

## Objetivo

Ordenar solicitações por prioridade efetiva, evitando que novas chegadas prioritárias ultrapassem indefinidamente uma solicitação antiga.

## Entrega

Aplicar promoção por callbacks concluídos, saturação no teto, desempate por chegada e descarte de solicitações vencidas. Falhas de callbacks contam como conclusão; a passagem do tempo não promove pedidos.

## Pré-requisitos

- **Tarefas:** [COM-001](COM-001-servico-i2c.md)
- **Condições externas:** Nenhuma para o escopo implementado.

## Definições e condições

- Implementar cinco prioridades de solicitação (0–4), promoção por callback concluído e desempate pela ordem de chegada. Envelhecimento não altera prioridades FreeRTOS nem comprova prazos do FOC.

## Critérios de conclusão

Política implementada e utilizada pelo executor; sua verificação nativa está em [TES-001](../../04-testes/done/TES-001-escalonamento-i2c.md). Isso não demonstra latência do FOC.

## Evidências

Política implementada no commit bdfa48f e inspecionada em 27/10/2026. [TES-001](../../04-testes/done/TES-001-escalonamento-i2c.md) registra o teste aprovado da lógica, sem comprovar concorrência FreeRTOS ou comunicação física.

## Referências

- [obc-dh/headers/I2cScheduling.h](../../../headers/I2cScheduling.h)
- [obc-dh/src/I2cBus.cpp](../../../src/I2cBus.cpp)
- Origem no planejamento anterior: P-BARRAMENTO.
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
