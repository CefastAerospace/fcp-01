# TES-004 — Recepção UART: validação e limites

**Estado:** Pendente
**Responsáveis:** OBC-DH e Payload

## Objetivo

Verificar delimitação e interpretação do fluxo de bytes recebido.

## Entrega

Injetar mensagens válidas, parciais, consecutivas, inválidas, acima do limite e rajadas. Exercitar sequência/gaps, NACK, repetição, timeout e retomada do parser conforme contrato. Preservar o cenário de excesso UART e a semântica dos campos do Payload.

## Pré-requisitos

- **Tarefas:** [COM-004](../01-comunicacao/COM-004-uart-payload.md)
- **Condições externas:** Contrato UART e seus resultados esperados definidos; não exige Raspberry Pi físico.

## Critérios de conclusão

Registrar casos, entradas, resultados esperados/observados e limites de buffer, sem adaptar critérios às falhas encontradas.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: Preparação de REQ-OBC-005 e REQ-OBC-006. Implementação não equivale a requisito validado.
- [payload/README.md](../../../payload/README.md)
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
