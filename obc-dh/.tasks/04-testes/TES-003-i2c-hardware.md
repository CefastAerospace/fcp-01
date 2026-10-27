# TES-003 — Comunicação I2C em hardware

**Estado:** Pendente
**Responsáveis:** OBC-DH, ADCS, EPS e Térmico

## Objetivo

Comprovar transações físicas I2C usando o serviço existente.

## Entrega

Usar uma bancada mínima que chama Init()/Execute(), sem depender do main integrado. Conferir endereços e operações dos dispositivos disponíveis, frequência real, resposta/ausência de dispositivo e timeout. Registrar configuração e limitações de cada montagem; recuperação de SDA/SCL presos pertence a [TES-009](TES-009-falhas-e-recuperacao.md).

## Pré-requisitos

- **Tarefas:** [COM-001](../01-comunicacao/done/COM-001-servico-i2c.md), [COM-002](../01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md)
- **Condições externas:** Placa e dispositivos disponíveis; inventário e montagem elétrica conferidos com os setores.

## Definições e condições

- Conferir controlador/dispositivos, SDA/SCL, compatibilidade dos 400 kHz, níveis, pull-ups, capacitância, comprimento e comportamento com periférico desligado. O Payload não participa desse barramento.

## Critérios de conclusão

Registrar versão, montagem, operações e resultados por dispositivo contra os parâmetros acordados. Dispositivo ainda não disponível mantém seu cenário pendente; a conclusão não comprova prazos FOC integrados.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- [obc-dh/headers/I2cBus.h](../../headers/I2cBus.h)
- Origem no planejamento anterior: P-BARRAMENTO.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
