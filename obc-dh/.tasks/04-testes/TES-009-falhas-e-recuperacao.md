# TES-009 — Falhas, recuperação e integridade

**Estado:** Pendente
**Responsáveis:** OBC-DH, EPS e demais setores envolvidos

## Objetivo

Comprovar recuperação e efeitos sobre dados/estados após falhas.

## Entrega

Executar os cenários abaixo separadamente, incluindo limites de detecção/recuperação e tratamento de tentativas esgotadas. Preservar entradas e resultados esperados existentes. Não alterar firmware para acomodar resultados sem diagnóstico e discussão.

## Pré-requisitos

- **Tarefas:** Por cenário, conforme tabela abaixo; não exigem conclusão conjunta para iniciar.
- **Condições externas:** Por cenário: mecanismo de recuperação disponível, montagem e critérios acordados.

## Cenários independentes

| Cenário | Pré-requisitos | Condições e verificação | Estado |
|---|---|---|---|
| SDA/SCL presos e retorno do I2C | [COM-003](../01-comunicacao/COM-003-recuperacao-i2c.md) | Método de injeção aprovado com EPS/ADCS; conferir tentativas, timeouts, pedidos pendentes e retorno do serviço. | Pendente |
| Desconexão e excesso UART | [COM-004](../01-comunicacao/COM-004-uart-payload.md) | Usar taxas e limites acordados; conferir recuperação do receptor e sinalização das perdas. | Pendente |
| Interrupção durante escrita no SD | [COM-005](../01-comunicacao/COM-005-armazenamento-sd.md) | Método acordado com EPS; avaliar arquivos incompletos e FAT32, não apenas fechamento normal ou conteúdo de um registro. | Pendente |
| Travamento, perda de comunicação e reinicialização | [INT-008](../03-integracao/INT-008-supervisao-e-retomada.md) | Medir detecção/recuperação, conferir falhas, horários e contagem de reinicializações; ausência ADS-B não significa enlace indisponível. | Pendente |
| Retomada persistente de missão, tempo e atuações | [INT-006](../03-integracao/INT-006-fluxo-de-dados.md), [INT-007](../03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-008](../03-integracao/INT-008-supervisao-e-retomada.md) | Conferir boots, referências, registros, saídas e Burn Wire após reset/interrupção, sem repetir atuação indevida nem inventar intervalo desligado. | Pendente |

## Critérios de conclusão

Todos os cenários aprovados, com versão, procedimento, entradas, resultados, tempos e análise dos registros/sistema de arquivos. Recuperação de um periférico não comprova recuperação completa do OBC.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-007, REQ-OBC-009, REQ-OBC-010, REQ-OBC-011, REQ-NF-OBC-003, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
