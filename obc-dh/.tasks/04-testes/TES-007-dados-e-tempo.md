# TES-007 — Formatos e tempo: interpretação e limites

**Estado:** Pendente
**Responsáveis:** OBC-DH

## Objetivo

Comprovar codificação, interpretação e validade temporal sem depender de periféricos.

## Entrega

Exercitar registros válidos/incompletos/inválidos, unidades, tamanhos e interpretação reproduzível. Conferir ausência de sincronização, referências sucessivas, conversão ms/s, reset, identificação de boots, contagens e estouro do contador. Preservar timestamp do Payload e não inferir tempo desligado pela referência de outro boot.

## Pré-requisitos

- **Tarefas:** [DAD-003](../02-dados-e-tempo/DAD-003-registros-e-interpretacao.md)
- **Condições externas:** Contratos e critérios temporais definidos; permite entradas simuladas.

## Critérios de conclusão

Casos e resultados reproduzíveis respeitam os contratos. Tempo simulado verifica lógica; precisão real do horário recebido pelo rádio exige [TES-008](TES-008-integracao.md).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-OBC-006, REQ-OBC-010, REQ-NF-OBC-002, REQ-NF-OBC-003. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
