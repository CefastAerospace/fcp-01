# DAD-001 — Modelos de dados e formatos persistidos

**Estado:** Pendente
**Responsáveis:** OBC-DH e produtores dos dados

## Objetivo

Definir representações documentadas para dados recebidos e registros armazenados.

## Entrega

Definir tipos concretos, unidades, validade, versão, tamanho máximo, enquadramento e leitura posterior. Cobrir missão, TM, eventos, falhas, estatísticas, reinicializações e referências de tempo. Usar headers/models/ apenas quando houver tipos efetivamente implementados. Não persistir memória bruta de structs nem alterar o contrato TT&C.

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Acordar campos, unidades e limites com Payload, TT&C, ADCS, EPS e Térmico.

## Definições e condições

- Definir status, temperatura, solicitações/respostas, significado de `val_flags`, validade de dados acumulados em instantes diferentes e comportamento de `seq_id` após reset. Manter distinção entre timestamp do Payload e recepção no OBC.

## Critérios de conclusão

Contrato e tipos necessários disponíveis, com exemplos e limites explícitos. Formato de transporte UART, representação interna e formato do arquivo têm responsabilidades distintas; nenhuma decisão sobre JSON/binário persistido será presumida.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-OBC-009, REQ-OBC-010, REQ-NF-OBC-002, REQ-NF-OBC-003. Implementação não equivale a requisito validado.
- [payload/README.md](../../../payload/README.md)
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-DADOS.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
