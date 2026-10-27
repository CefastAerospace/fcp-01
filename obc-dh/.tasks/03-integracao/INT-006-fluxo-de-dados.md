# INT-006 — Fluxo Payload–tempo–SD–telemetria

**Estado:** Pendente
**Responsáveis:** OBC-DH, Payload e TT&C

## Objetivo

Conectar recepção, referência temporal, registro e envio dos dados da missão.

## Entrega

Associar tempo aos dados/eventos, gravar missão/TM/estatísticas/falhas e persistir boots/referências. Tratar set_time validando args.unix em segundos Unix e correlacionando ao contador local em ms. Usar SendTelemetry() para estado e BuildAircraft()/Send() para aeronaves agregadas. Tratar falhas de serialização, filas e SD sem perdas silenciosas; implementar a extração pelo umbilical conforme contrato.

## Pré-requisitos

- **Tarefas:** [COM-004](../01-comunicacao/COM-004-uart-payload.md), [COM-005](../01-comunicacao/COM-005-armazenamento-sd.md), [DAD-003](../02-dados-e-tempo/DAD-003-registros-e-interpretacao.md), [INT-004](INT-004-ttc-e-umbilical.md)
- **Condições externas:** Acordar seleção/frequência de downlink e validade temporal com Payload e TT&C.

## Definições e condições

- TM via `SendTelemetry()` e aeronaves agregadas via `BuildAircraft()`/`Send()`. Corpo JSON ≤245 bytes sem `n`, quadro final ≤255 bytes, sem fragmentação e CRC de hardware LoRa. ACK prioritário e TX sob demanda já estão definidos; acordar frequência/seleção e tratar falhas de geração/enfileiramento. Não substituir demonstração de timestamp na TM por outro teste; preservar o critério e discutir evidência correspondente.

## Critérios de conclusão

Fluxo central e tratamento de erros implementados, contrato temporal preservado e verificações locais registradas. Demonstrar ADS-B simulado com timestamp na TM e extração reproduzível em [TES-008](../04-testes/TES-008-integracao.md).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-003, REQ-OBC-005, REQ-OBC-006, REQ-OBC-009, REQ-OBC-010, REQ-NF-OBC-002, REQ-NF-OBC-003. Implementação não equivale a requisito validado.
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-DADOS.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
