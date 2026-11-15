# INT-012 — Watchdog e reinicialização do OBC

**Estado:** Pendente
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos limites operacionais:** OBC-DH, coordenação e setores afetados

## Objetivo

Reagir à falta de progresso real das tarefas e atender reinicialização comandada de forma controlada.

## Origem e limites

Watchdog extraído da [INT-008](INT-008-supervisao-e-retomada.md) e `CMD_RESET_OBC` da [INT-007](INT-007-comandos-modos-e-protecoes.md), em 14/11/2026. A [INT-013](INT-013-retomada-operacional.md) define retomada de voo; este recorte pode ser desenvolvido com o boot de bancada existente. Não assume watchdog externo, redundância ou partição de recuperação.

## Entrega

- Reutilizar diagnósticos de progresso UART/processamento/escritor/I2C da INT-008 e o watchdog disponível no ESP32. Não alimentar o watchdog apenas porque o supervisor continua executando.
- Distinguir tarefas inativas, espera legítima, recuperação limitada e operação sem progresso, incluindo callback/chamada de driver que não retorna.
- Definir prazos, tolerâncias, ação final e comportamento em falha do supervisor, com integração por proprietário do recurso. Não destruir tarefa com callback ativo que ainda usa contexto do chamador.
- Implementar `CMD_RESET_OBC` no consumidor existente: validar argumentos/condições, encaminhar ACK e agendar reset com espera limitada. ACK enfileirado não garante recepção no solo.
- Reutilizar reserva de boot/NVS e causa de reset já presentes no BOOT. Tentar registrar falhas pelos recursos disponíveis, sem exigir sucesso do SD para reiniciar nem declarar persistência inexistente.

## Pré-requisitos

- **Tarefas/capacidades:** boot de bancada da INT-003, consumidor/ACK da INT-004 e diagnósticos existentes da INT-008. Não exigir encerramento global da INT-007 para o recorte de bancada.
- **Condições externas:** prazos e política operacional acordados antes do uso em voo; efeitos do reset sobre saídas dependem da INT-003/INT-011/INT-013. Limites provisórios de desenvolvimento devem seguir o [registro comum](../README.md#parâmetros-provisórios), sem alterar limites de aceitação existentes.

## Verificação de desenvolvimento

Cobrir progresso normal, fronteiras temporais, espera legítima, travamento de cada tarefa supervisionada, supervisor sem progresso e reset comandado. Conferir ausência de falso reset, acionamento da ação prevista, novo boot e causa de reset, com armazenamento indisponível. Usar injeção finita/isolada e preservar os testes anteriores.

## Critérios de conclusão

Mecanismo e comando implementados, limites/ações documentados, build e regressões pertinentes aprovados no escopo declarado. O recorte de bancada não aprova retorno de voo. A comprovação independente dos tempos/retorno cabe à TES-009. A política e os parâmetros definitivos de retomada de voo são obrigação da INT-013, com origem nesta INT; pendência nessa destinatária não impede concluir o mecanismo no escopo de bancada declarado.

## Encaminhamento para comprovação

[TES-009](../04-testes/TES-009-falhas-e-recuperacao.md) comprova travamento, watchdog, contagem, causa e tempos de reinicialização. O cenário de retomada de missão depende também da INT-013; [TES-008](../04-testes/TES-008-integracao.md) mantém o ensaio elétrico de boot/reset.

## Evidências

Nenhum watchdog ou reset por TC implementado por esta revisão documental.
