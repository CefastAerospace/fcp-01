# INT-008 — Supervisão, watchdog e retomada após falhas

**Estado:** Pendente
**Responsáveis:** OBC-DH e setores

## Objetivo

Detectar falhas e perda de comunicação, recuperar recursos e registrar o ocorrido.

## Entrega

Supervisionar tarefas críticas e recursos, distinguindo watchdog de tarefas de proteção térmica. Reutilizar GetLinkStats()/last_tc_ms e a saúde/reset do SX1276. Associar horários, persistir falhas e contagem de reinicializações, recuperar periférico/tarefa/OBC e retornar ao estado do ConOps. Não assumir watchdog externo ou partição de recuperação do DP como implementação confirmada.

## Pré-requisitos

- **Tarefas:** [COM-003](../01-comunicacao/COM-003-recuperacao-i2c.md), [INT-007](INT-007-comandos-modos-e-protecoes.md)
- **Condições externas:** Definir saúde, tempos, tentativas e retomada com coordenação e setores.

## Definições e condições

- Preservar limites TT&C de entrega TC ao OBC ≤200 ms e resposta ≤2 s; definir detecção/recuperação e critérios de saúde/tentativas por periférico/tarefa/OBC. Reutilizar estatísticas e reset do rádio existentes; ausência ADS-B não equivale à perda de comunicação.
- Considerar limites físicos: watchdog não recupera falha física do ESP32; não existe redundância física confirmada de OBC/armazenamento.

## Critérios de conclusão

Supervisão, limites e recuperação implementados, eventos registrados e verificações locais disponíveis. Atendimento aos tempos e integridade após falhas depende de [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md).

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Integrar a política de retomada do escritor após falha/reset: preservar arquivos, End/Init e nome novo, sem apagar ou reutilizar conteúdo anterior. | Pendente |
| [DAD-003](../02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) | Integrar retomada persistente por boot e falhas com os estados definidos em INT-007, sem deduzir autorização de atuação de eventos ou flags voláteis. | Pendente |

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-007, REQ-OBC-009, REQ-OBC-010, REQ-OBC-011, REQ-NF-OBC-003, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [tt-c/include/Ttc.h](../../../tt-c/include/Ttc.h)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-MISSAO.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
