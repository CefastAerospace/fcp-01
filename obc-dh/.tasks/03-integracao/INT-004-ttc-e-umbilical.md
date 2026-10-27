# INT-004 — Integração de TT&C e umbilical

**Estado:** Pendente
**Responsáveis:** OBC-DH e TT&C

## Objetivo

Conectar o OBC ao transporte existente de telecomandos e telemetria.

## Entrega

Reutilizar a biblioteca TT&C: Init()/Start(), ReceiveTelecommand(), SendAck(), Send()/SendTelemetry() e UmbilicalLink. Não executar Update() concorrentemente com Start(). Preservar tarefa do enlace no Core 0, prioridade padrão 10, ISR de DIO0 apenas notificando e acesso exclusivo ao rádio. Declarar dependências e fixar versões validadas no ambiente consumidor. A aplicação de set_time pertence a [INT-006](INT-006-fluxo-de-dados.md); ações de missão a [INT-007](INT-007-comandos-modos-e-protecoes.md).

## Pré-requisitos

- **Tarefas:** [INT-003](INT-003-inicializacao-central.md)
- **Condições externas:** Biblioteca TT&C disponível; conferir montagem, versões e contrato com o setor.

## Definições e condições

- Rádio usa `SPI` global; SD exige instância própria HSPI. Parâmetros atuais TT&C: 921 MHz, SF7, BW 125 kHz, CR 4/6, preâmbulo 32, CRC do rádio e TX sob demanda. Preservar decisão TT&C; validar velocidades/coordenação com estação e não duplicar filas.

## Critérios de conclusão

Transporte ligado ao firmware central, TC/ACK e TC/TM de bancada conectados e build aprovado. Preservar ACK pelo meio de origem e os limites de entrega TC ≤200 ms e resposta ≤2 s; comprovação integrada em [TES-008](../04-testes/TES-008-integracao.md).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-002, REQ-OBC-003. Implementação não equivale a requisito validado.
- [tt-c/include/Ttc.h](../../../tt-c/include/Ttc.h)
- [tt-c/include/TtcProtocol.h](../../../tt-c/include/TtcProtocol.h)
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [tt-c/ENSAIOS.md](../../../tt-c/ENSAIOS.md)
- Origem no planejamento anterior: P-SPI.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
