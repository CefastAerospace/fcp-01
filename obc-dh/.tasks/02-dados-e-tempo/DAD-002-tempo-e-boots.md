# DAD-002 — Tempo monotônico, boots e referências absolutas

**Estado:** Pendente
**Responsáveis:** OBC-DH, Payload, TT&C e coordenação

## Objetivo

Representar e reconstruir o horário dos dados e eventos sem inventar horário absoluto.

## Entrega

Implementar lógica temporal verificável com entradas simuladas, sem exigir rádio ou SD. Definir identificação/contagem dos boots e persistência inclusive com SD indisponível. A recepção real de set_time e a gravação das referências serão conectadas em [INT-006](../03-integracao/INT-006-fluxo-de-dados.md).

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Definir precisão, resolução, deriva e identificação persistente dos boots com coordenação, Payload e TT&C.

## Definições e condições

- RTC externo está fora da configuração atual; considerar sua instalação apenas se o método monotônico/LoRa falhar na validação temporal acordada. DS3231 histórico usa `0x68`, que conflita com a IMU; eventual reconsideração exige decisão elétrica sem mudar MPU9250 para `0x69` por suposição. Payload UART não recebe endereço I2C.
- Decisão: ms monotônicos por boot e referências absolutas pelo LoRa via `CMD_SET_TIME`/`set_time`, com reconstrução retroativa sem reescrever registros. RTC externo fica fora da configuração atual e só será reconsiderado se esse método não atender aos critérios temporais acordados; DS3231 permanece histórico.
- `args.unix` e `ts` do contrato TT&C estão em segundos Unix. Definir instante representado/recebido, conversão ms/s, resolução, erro/deriva admissível, periodicidade e validade; considerar quantização do horário transmitido e atraso no enlace. Não alterar silenciosamente o contrato ou inventar precisão.
- Definir identificação dos boots, persistência e contagem sem colisões após reset, inclusive com SD indisponível. Tratar duração, possível estouro do contador monotônico e representação dos tempos nos registros.
- Definir escolha/interpretação de referências sucessivas, ajuste inicial sem internet, análise posterior e indicação de horário desconhecido nos registros e na TM. Não inferir intervalo desligado usando apenas a referência do boot anterior.
- Definir o instante do timestamp OBC e preservar o horário produzido pelo Payload sem remover ou redefinir seu campo. O exemplo TT&C faz ajuste em memória e não comprova recuperação ou precisão.

## Critérios de conclusão

Lógica e contrato temporal implementados, com verificações locais para ausência de referência, referências sucessivas, resets e estouro do contador. Precisão física continua dependente de medição em [TES-008](../04-testes/TES-008-integracao.md).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-006, REQ-OBC-010, REQ-NF-OBC-003. Implementação não equivale a requisito validado.
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- Origem no planejamento anterior: P-ENDERECOS, P-TEMPO.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
