# INT-002 — Conciliação de EN do motor no GPIO 12

**Estado:** Concluída
**Responsáveis:** ADCS e OBC-DH

## Objetivo

Manter a mesma alocação de EN no código ADCS e na pinagem OBC.

## Entrega

PIN_EN e pinout::motor_en coincidem no GPIO 12. Esta conclusão é restrita ao EN; fases, montagem e estados seguros pertencem a [INT-003](../INT-003-inicializacao-central.md)/[INT-005](../INT-005-sensores-e-adcs.md).

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Nenhuma para a coincidência das constantes no código.

## Definições e condições

- Conciliar EN no GPIO 12 entre `adcs/include/config.h` (`PIN_EN`) e `obc-dh/headers/Pinout.h` (`motor_en`). Evidência: inspeção do código em 27/10/2026; não comprova montagem ou estados em boot/reset.

## Critérios de conclusão

Constantes conferidas nos dois módulos. Não representa ensaio elétrico ou autorização para alinhamento do motor no boot.

## Evidências

Inspeção de 27/10/2026 confirma PIN_EN = 12 e motor_en = 12. Fases ainda diferem: ADCS 27/26/25 e OBC 25/26/27. Não houve alteração de firmware nesta reorganização.

## Referências

- [adcs/include/config.h](../../../../adcs/include/config.h)
- [obc-dh/headers/Pinout.h](../../../headers/Pinout.h)
- Origem no planejamento anterior: P-ADCS.
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
