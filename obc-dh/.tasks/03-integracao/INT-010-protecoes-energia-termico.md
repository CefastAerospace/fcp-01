# INT-010 — Proteções energéticas e térmicas

**Estado:** Pendente
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos contratos:** EPS, Térmico e coordenação

## Objetivo

Aplicar proteções e prioridades operacionais conforme limites de energia e temperatura aprovados.

## Origem e limites

Escopo extraído da [INT-007](INT-007-comandos-modos-e-protecoes.md) em 14/11/2026. A [INT-009](INT-009-aquisicao-eps-termico.md) fornece aquisição; a INT-007 coordena modos. Watchdog de tarefas pertence à [INT-012](done/INT-012-watchdog-reset-obc.md), não à proteção térmica.

## Entrega

- Integrar limiares, prioridades, bloqueios, entrada em baixo consumo e condições de retorno com o coordenador de modos.
- Tratar dados ausentes/inválidos e sua ação conforme contrato; armazenar temperatura não comprova proteção.
- Integrar corte de alimentação de periféricos somente quando a interface e as condições forem fornecidas pelos responsáveis. Não presumir sinal dedicado.
- Obter cota energética real e regras para recuperação. Os 5x LM75A previstos monitoram bateria/pontos críticos conforme localização e responsabilidade confirmadas pelos setores.
- Preservar LOW-POWER com rádio em RX e beacon periódico somente em safe-mode, conforme decisão TT&C vigente.

## Pré-requisitos

- **Tarefas/capacidades:** dados válidos/disponibilidade da INT-009, efeitos de controle ADCS da INT-005 e transições/comandos da INT-007. Preparar a lógica com entradas simuladas não requer encerrar os ensaios físicos.
- **Condições externas:** ConOps, cota EPS, limiares térmicos, periodicidade, prioridades, atuação, retorno e comportamento sem amostra aprovados. Níveis e condições elétricas não podem ser arbitrados pelo OBC.

## Verificação de desenvolvimento

Cobrir fronteiras dos limites acordados, amostra inválida/ausente, bloqueio de comandos, prioridades e retorno. Caso o contrato estabeleça limiares diferentes de entrada/saída, verificar ambos. Valores provisórios só verificam a lógica e seguem o [registro comum](../README.md#parâmetros-provisórios).

## Critérios de conclusão

Proteções implementadas segundo contratos, falhas/eventos encaminhados, build e regressões pertinentes aprovados. Não exigir consumo medido nesta entrega de software nem apresentar estimativas como medições.

## Encaminhamento para comprovação

[TES-008](../04-testes/TES-008-integracao.md) comprova transições, resposta térmica e consumo contra cota EPS, incluindo consumo residual do motor desabilitado. [TES-010](../04-testes/TES-010-missao-e-cobertura.md) consolida prioridades e orçamento durante a missão. Preservar todos os critérios originais.

## Evidências

Nenhuma implementação ou aprovação registrada por esta revisão.
