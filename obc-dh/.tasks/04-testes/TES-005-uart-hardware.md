# TES-005 — UART entre ESP32 e Raspberry Pi

**Estado:** Em andamento
**Responsáveis:** OBC-DH e Payload

## Objetivo

Comprovar troca física bidirecional e comportamento de falhas do enlace UART.

## Entrega

Validar binário compacto com SeqID/NACK em 115,2 kbps 8N1. Conferir TX/RX cruzados, GND, 3,3 V, GPIOs e console/Bluetooth do Pi. Exercitar desconexão, reset de uma ponta, mensagens parciais e rajadas dentro dos parâmetros aprovados, incluindo comportamento com uma ponta desligada.

## Pré-requisitos

- **Tarefas:** [COM-004](../01-comunicacao/done/COM-004-uart-payload.md)
- **Condições externas:** Ponta UART do Payload implementada; placas, montagem e porta serial disponíveis.

## Critérios de conclusão

Registrar montagem, firmware/script, tráfego e perdas/recuperação contra o contrato. Teste com dados simulados não comprova aquisição ADS-B completa.

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [COM-004](../01-comunicacao/done/COM-004-uart-payload.md) | Comprovar troca ESP32–Pi, GPIOs/console, desconexões, resets e alimentação unilateral; conciliar explicitamente o cenário binário histórico com FCPU sem reescrever entradas/resultados esperados. | Pendente |
| [TES-004](done/TES-004-uart-software.md) | Executar a interoperabilidade e falhas nas duas pontas reais; os casos nativos aprovados não encerram o ensaio físico. | Pendente |

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

### Bancada local — 09/11/2026

O responsável identificou ESP32-WROOM-32 e os pinos G16/G17. Foi executado o
ambiente isolado `uart-loopback`, com jumper G17–G16 confirmado pelo responsável,
115200 8N1 e USB em `/dev/ttyUSB0` via CH340/CH341. A porta não aparece no
sandbox, mas foi identificada no kernel e acessada no host. Upload e reset
foram executados, sem alterar os vetores ou o parser do beta.

[Log completo](../../examples/uart-loopback-2026-11-09.txt): 60 envios e 60
recepções com ICAO E4953A, altitude 39000 pés, velocidade 489 nós e sequência
`1, 2, 4, ... 61`. Somente o gap proposital esperado=3/recebido=4 e seu NACK
simulado. Sem erros de JSON, excesso de buffer ou reinício inesperado. Novo
reset voltou a `1, 2, 4`. O NACK desse beta é apenas uma mensagem de log.

Separadamente, `uart-receiver-check` injetou a amostra real em RAM no ESP32 e
aprovou o novo receptor, repetição e CRC, com heap/pilha medidos. Seu
[log](../../examples/uart-receiver-check-split-heap-2026-11-09.txt) não é ensaio
físico do protocolo novo: não há transmissor Pi ou quadros enviados pelo fio.
Procedimentos e retorno ao firmware normal no [guia](../../examples/README.md).

Permanecem sem execução a troca bidirecional ESP32–Pi, desconexões, reset de
uma ponta, rajadas e alimentação unilateral. O cenário histórico de binário
compacto da entrega acima diverge do JSON vigente; foi registrado para
conciliação sem modificar suas entradas ou resultados esperados. A tarefa
continua em andamento e não foi movida para `done/`.

## Referências

- Requisitos relacionados: REQ-OBC-011. Implementação não equivale a requisito validado.
- [payload/README.md](../../../payload/README.md)
- [payload/raspberry-pi-runbook.md](../../../payload/raspberry-pi-runbook.md)
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
