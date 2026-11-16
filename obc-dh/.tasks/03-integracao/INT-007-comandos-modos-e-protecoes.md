# INT-007 — Comandos, modos e coordenação da missão

**Estado:** Pendente
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos contratos:** coordenação, ADCS, EPS, Térmico e TT&C

## Objetivo

Coordenar comandos e modos conforme o ConOps, validando argumentos e condições antes de executar ações.

## Redistribuição de 14/11/2026

O escopo anterior reunia modos, proteção energética/térmica, Burn Wire e retorno após reset. As responsabilidades passam a ser:

| Entrega | Destino |
|---|---|
| Comandos, transições e coordenação da missão | Esta INT-007 |
| Proteções energéticas/térmicas, prioridades e retorno | [INT-010](INT-010-protecoes-energia-termico.md) |
| Burn Wire, autorização, interlock, duplicatas e estados persistentes recebidos de DAD-003 | [INT-011](INT-011-burn-wire-persistencia.md) |
| `CMD_RESET_OBC` | [INT-012](done/INT-012-watchdog-reset-obc.md) |
| Retomada operacional de modos/missão após reset | [INT-013](INT-013-retomada-operacional.md) |

Todas as obrigações permanecem pendentes nas destinatárias. A revisão não implementa modos nem altera critérios dos ensaios.

## Entrega

Validar argumentos, modo e condições antes de executar cada TC; responder com ACK/rejeição pelo consumidor existente. Integrar transições, início/fim da missão e coordenação do detumbling. Definir no núcleo de modos as condições e a coordenação que as INT-010/011 consomem. Essas destinatárias integram suas proteções/atuações ao núcleo e verificam os efeitos, sem criar dependência circular de encerramento. Desabilitar motor fora do detumbling conforme a interface ADCS; consumo residual é medido pelo setor de testes, sem presumir zero.

## Pré-requisitos

- **Tarefas/capacidades:** consumidor/ACK da INT-004 e estado/fluxo da INT-006, já disponíveis nos recortes registrados. Definir a lógica de modos a partir do contrato sem exigir conclusão de escopos independentes dessas INT.
- **Integração por ação:** comandos ADCS consomem a interface da INT-005; decisões de energia/temperatura consomem INT-009/010; atuação Burn Wire consome INT-011. Ausência de recurso não autoriza indicar que a ação foi executada.
- **Condições externas:** ConOps, estado inicial, transições, comandos permitidos, duração/perfil de missão e condições de atuação aprovados pelos responsáveis.

## Definições e condições

- Reutilizar `common/protocol/ttc-json.md` e `tt-c/include/TtcProtocol.h`: envelope TC no TT&C, argumentos/condições no OBC e `SendAck()` existente. Não presumir PUS; satélite e estação precisam da mesma versão.
- A biblioteca deduplica oito TCs por 30 s; isso não substitui persistência de atuações críticas, transferida à INT-011.
- Integrar modos/transições e estado inicial do README geral/contrato TT&C, conferindo ConOps vigente. Preservar LOW-POWER com rádio em RX e beacon periódico somente em safe-mode.
- Definir se alinhamento do motor no boot é permitido e em quais condições com INT-003/005. Confirmar “tumbling” nos requisitos versus “detumbling” nas demais referências, sem alterar o requisito silenciosamente.
- Detalhar comandos permitidos e prioridades por modo. Proteção térmica e watchdog de tarefas são mecanismos distintos, com INT-010 e INT-012 proprietárias.

## Verificação de desenvolvimento

Cobrir transições válidas e recusadas, argumentos inválidos sem efeitos, recurso indisponível, falha da ação e coordenação de início/fim de missão. Casos novos acompanham a implementação; não alterar as expectativas dos comandos ainda não implementados em roteiros históricos.

## Critérios de conclusão

Comandos e transições do próprio escopo implementados conforme contratos, efeitos/recusas integrados, build e regressões pertinentes aprovados. A interface ADCS deve estar disponível para concluir seus comandos. O núcleo não exige encerrar INT-010/011: elas implementam e verificam as próprias extensões ao núcleo já entregue. Comando ainda não implementado não pode receber sucesso fictício; não criar módulos vazios. Comprovação física de modos, energia e atuações continua nas TES-008/009/010; sua aprovação não é condição de encerramento desta implementação.

## Encaminhamento DAD-003

A obrigação de estados duráveis de atuações/Burn Wire, autorização, confirmação e eventual extensão versionada de registros foi transferida integralmente à INT-011. A retomada geral que consome esses estados está na INT-013. Eventos existentes não substituem estado de retomada.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- REQ-OBC-001/002/004/012 e REQ-NF-OBC-001 conservam implementações complementares nas novas destinatárias; [matriz](../README.md#cobertura-dos-requisitos).
- [Contrato TT&C](../../../common/protocol/ttc-json.md), [pinagem](../../.agents/PINAGEM.md), [TES-008](../04-testes/TES-008-integracao.md), [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md), [TES-010](../04-testes/TES-010-missao-e-cobertura.md).
- Origens anteriores preservadas na correspondência: P-ADCS, P-BURN-WIRE, P-DADOS, P-EPS-ELETRICA, P-MISSAO, P-TERMICO.
- [Painel](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
