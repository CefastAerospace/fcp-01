# TES-001 — Teste nativo do escalonamento I2C

**Estado:** Concluída
**Responsáveis:** OBC-DH

## Objetivo

Verificar a política I2C independentemente de FreeRTOS e dispositivos.

## Entrega

Executar o teste existente sem modificar suas entradas ou asserts. Cobrir prioridade, promoção por conclusões, saturação, desempate por chegada, fila vazia, vencimento exato e estouro do contador em ms.

## Pré-requisitos

- **Tarefas:** [COM-002](../../01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md)
- **Condições externas:** Compilador C++ nativo; sem hardware.

## Definições e condições

- Implementar e executar o teste nativo da política em `tests/I2cSchedulingCheck.cpp`. Evidência e limites registrados abaixo.

## Critérios de conclusão

Compilação e execução do teste existente encerram com código zero. O teste não cobre executor FreeRTOS, fila cheia real, concorrência ou transações físicas.

## Evidências

Execução aprovada em 27/10/2026, já registrada no TODO anterior ao commit 423dacb. Compilação e execução encerraram com código zero. A reorganização reutiliza essa evidência, sem declarar novo ensaio.

Executado dentro de obc-dh/:

```bash
g++ -std=c++11 -Wall -Wextra -Werror tests/I2cSchedulingCheck.cpp -o /tmp/fcp01-i2c-check
/tmp/fcp01-i2c-check
```

## Referências

- [obc-dh/tests/I2cSchedulingCheck.cpp](../../../tests/I2cSchedulingCheck.cpp)
- [obc-dh/headers/I2cScheduling.h](../../../headers/I2cScheduling.h)
- Origem no planejamento anterior: P-BARRAMENTO.
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
