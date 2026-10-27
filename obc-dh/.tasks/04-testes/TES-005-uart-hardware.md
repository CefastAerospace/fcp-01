# TES-005 — UART entre ESP32 e Raspberry Pi

**Estado:** Pendente
**Responsáveis:** OBC-DH e Payload

## Objetivo

Comprovar troca física bidirecional e comportamento de falhas do enlace UART.

## Entrega

Validar binário compacto com SeqID/NACK em 115,2 kbps 8N1. Conferir TX/RX cruzados, GND, 3,3 V, GPIOs e console/Bluetooth do Pi. Exercitar desconexão, reset de uma ponta, mensagens parciais e rajadas dentro dos parâmetros aprovados, incluindo comportamento com uma ponta desligada.

## Pré-requisitos

- **Tarefas:** [COM-004](../01-comunicacao/COM-004-uart-payload.md)
- **Condições externas:** Ponta UART do Payload implementada; placas, montagem e porta serial disponíveis.

## Critérios de conclusão

Registrar montagem, firmware/script, tráfego e perdas/recuperação contra o contrato. Teste com dados simulados não comprova aquisição ADS-B completa.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-011. Implementação não equivale a requisito validado.
- [payload/README.md](../../../payload/README.md)
- [payload/raspberry-pi-runbook.md](../../../payload/raspberry-pi-runbook.md)
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
