# INT-007 — Comandos, modos e proteções operacionais

**Estado:** Pendente
**Responsáveis:** OBC-DH, coordenação, ADCS, EPS, Térmico, Estruturas e TT&C

## Objetivo

Executar comandos e modos apenas nas condições acordadas, com prioridades energéticas e proteções.

## Entrega

Validar argumentos, modo e condições antes de executar cada TC, responder com ACK/rejeição e preservar proteção contra atuação duplicada. Integrar modos/transições, detumbling, Thermal Watchdog e Burn Wire. Definir estados persistentes e retorno após reset. Desabilitar o motor fora do detumbling e medir consumo residual; não presumir consumo zero.

## Pré-requisitos

- **Tarefas:** [INT-004](INT-004-ttc-e-umbilical.md), [INT-005](INT-005-sensores-e-adcs.md), [INT-006](INT-006-fluxo-de-dados.md)
- **Condições externas:** ConOps, cota energética, limiares térmicos e condições de atuação aprovados pelos responsáveis.

## Definições e condições

- Reutilizar `common/protocol/ttc-json.md` e `tt-c/include/TtcProtocol.h`: envelope TC no TT&C, `args`/condições no OBC e resposta `SendAck()`. A biblioteca deduplica oito TCs por 30 s; definir persistência/condições das atuações críticas após reset e fora da janela. Não presumir PUS; satélite e estação precisam da mesma versão.
- O mecanismo confirmado é MOSFET no GPIO 33 para queimar a retenção física das antenas. Remoto registra dois circuitos nichrome, 5 V / 4 Ω cada e orçamento de 2,5 A / 12,5 W transitório; conferir montagem, nível ativo, estados em boot/reset, duração máxima, autorização, energia e novas tentativas com Estruturas/EPS.
- Acionar fora de TX, usando `Ttc::HoldTransmit(true, wait_ms)` e confirmando seu retorno antes da atuação; definir retomada e tratamento de falhas. Esse interlock não autoriza a queima. Preservar ensaio mínimo de cinco ciclos bem-sucedidos registrado pelo TT&C, sem mudar critérios dos setores.
- A flag `IS_WIRE_BURNT` foi apenas sugerida; não é implementação aprovada nem convenção de nomes. Definir persistência, estado inicial, interrupção durante acionamento e duplicatas de comando; flag volátil perde valor após reset.
- Distinguir comando emitido de liberação física confirmada; definir sensor ou critério de sucesso. Se a antena for necessária para receber TC, acordar autorização da primeira liberação sem depender de um comando impossível de receber.
- Definir se o alinhamento do motor no boot é permitido e em quais condições; confirmar a ação denominada “tumbling” nos requisitos versus “detumbling” nas demais referências.
- Obter cota energética real, limiares, prioridades, baixo consumo, retorno e corte de alimentação de periféricos. Medir consumo da montagem e consumo residual do motor desabilitado; valores históricos não comprovam orçamento.
- Projeto EPS/Térmico prevê 5x LM75A na bateria/pontos críticos, com alerta ao OBC; confirmar endereços, localização, aquisição, periodicidade, limites/retorno e responsável pela proteção. Não presumir implementação térmica a partir da biblioteca INA219 existente.
- Distinguir monitoramento de atuação térmica e watchdog de tarefas. Armazenar temperatura não comprova proteção; não presumir sinal dedicado.
- Integrar modos/transições e estado inicial registrados no README geral e no contrato TT&C, conferindo ConOps vigente. Preservar decisão TT&C de LOW-POWER com rádio em RX e beacon periódico só em safe-mode. Detalhar retomada, comandos permitidos e prioridades sem substituir decisões dos setores.

## Critérios de conclusão

Ações e recusas implementadas conforme contratos, estados persistentes tratados e verificações locais disponíveis. A atuação física e o atendimento energético/térmico exigem [TES-008](../04-testes/TES-008-integracao.md)/[TES-009](../04-testes/TES-009-falhas-e-recuperacao.md)/[TES-010](../04-testes/TES-010-missao-e-cobertura.md).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-001, REQ-OBC-002, REQ-OBC-004, REQ-OBC-012, REQ-NF-OBC-001. Implementação não equivale a requisito validado.
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- Origem no planejamento anterior: P-ADCS, P-BURN-WIRE, P-DADOS, P-EPS-ELETRICA, P-MISSAO, P-TERMICO.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
