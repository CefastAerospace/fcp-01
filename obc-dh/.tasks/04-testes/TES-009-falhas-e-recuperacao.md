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
| Desconexão e excesso UART | [COM-004](../01-comunicacao/done/COM-004-uart-payload.md) | Usar taxas e limites acordados; conferir recuperação do receptor e sinalização das perdas. | Pendente |
| Interrupção durante escrita no SD | [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Método acordado com EPS; avaliar arquivos incompletos e FAT32, não apenas fechamento normal ou conteúdo de um registro. | Pendente |
| Travamento, perda de comunicação e reinicialização | [INT-008](../03-integracao/INT-008-supervisao-e-retomada.md) | Medir detecção/recuperação, conferir falhas, horários e contagem de reinicializações; ausência ADS-B não significa enlace indisponível. | Pendente |
| Retomada persistente de missão, tempo e atuações | [INT-006](../03-integracao/INT-006-fluxo-de-dados.md), [INT-007](../03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-008](../03-integracao/INT-008-supervisao-e-retomada.md) | Conferir boots, referências, registros, saídas e Burn Wire após reset/interrupção, sem repetir atuação indevida nem inventar intervalo desligado. | Pendente |

## Critérios de conclusão

Todos os cenários aprovados, com versão, procedimento, entradas, resultados, tempos e análise dos registros/sistema de arquivos. Recuperação de um periférico não comprova recuperação completa do OBC.

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [COM-004](../01-comunicacao/done/COM-004-uart-payload.md) | Executar os cenários físicos de desconexão/excesso UART com taxas acordadas e verificar recuperação e perdas. | Pendente |
| [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Exercitar interrupção durante escrita e avaliar arquivos incompletos e FAT32; fsync lógico não comprova integridade física. | Pendente |
| [DAD-003](../02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) | Comprovar retomada e integridade sob reset/interrupção real, incluindo estados operacionais; recuperação lógica do leitor não é reparo de FAT32. | Pendente |
| [TES-004](done/TES-004-uart-software.md) | Executar o cenário físico de excesso/desconexão UART; a saturação simulada não o substitui. | Pendente |
| [TES-007](done/TES-007-dados-e-tempo.md) | Comprovar boots, persistência temporal e integridade de registros/estados em resets e interrupções reais, preservando os cenários existentes. | Pendente |

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-007, REQ-OBC-009, REQ-OBC-010, REQ-OBC-011, REQ-NF-OBC-003, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
