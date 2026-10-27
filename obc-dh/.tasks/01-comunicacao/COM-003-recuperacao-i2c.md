# COM-003 — Recuperação do barramento I2C após falhas

**Estado:** Pendente
**Responsáveis:** OBC-DH, ADCS, EPS e Térmico

## Objetivo

Restabelecer o serviço após falhas recuperáveis sem acesso concorrente ao barramento.

## Entrega

Definir detecção, limite de tentativas, efeito sobre pedidos pendentes e retorno ao chamador. Tratar SDA/SCL presos conforme a montagem e diferenciar timeout de transferência, operação longa e falha física. Timeout de 20 ms não limita um callback completo; uma operação que não retorna impede o progresso atual.

## Pré-requisitos

- **Tarefas:** [COM-001](done/COM-001-servico-i2c.md)
- **Condições externas:** Acordar política e limites de recuperação com ADCS, EPS e Térmico.

## Critérios de conclusão

Política documentada, recuperação implementada no responsável pelo barramento e falhas sinalizadas. Conferir o comportamento de software; o cenário físico será registrado em [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-007, REQ-OBC-011. Implementação não equivale a requisito validado.
- Origem no planejamento anterior: P-BARRAMENTO; recuperação separada da integração dos consumidores em INT-005.
- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
