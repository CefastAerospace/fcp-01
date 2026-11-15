# INT-011 — Atuação e persistência do Burn Wire

**Estado:** Pendente
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos contratos:** Estruturas, EPS, TT&C e coordenação

## Objetivo

Executar liberação de antena somente nas condições autorizadas, preservando proteção contra repetição após reset.

## Origem e limites

Escopo extraído da [INT-007](INT-007-comandos-modos-e-protecoes.md) em 14/11/2026, incluindo o encaminhamento DAD-003 sobre estados duráveis. A revisão não autoriza atuação. Retomada operacional geral pertence à [INT-013](INT-013-retomada-operacional.md).

## Entrega

- Validar argumentos, autorização, modo, energia e condições de atuação antes do comando; integrar as recusas ao consumidor TT&C existente.
- Usar `Ttc::HoldTransmit(true, wait_ms)` e confirmar seu retorno antes de atuar fora de TX. Definir liberação do interlock e tratamento de falha; o interlock não autoriza a queima.
- Implementar limite de duração e estados duráveis para autorização, início, interrupção e confirmação conforme contrato. Distinguir comando emitido de liberação física confirmada.
- Impedir repetição indevida após reset e fora da janela TT&C de oito TCs/30 s; deduplicação volátil e eventos históricos não substituem estado persistente.
- Se houver extensão dos registros, versionar e verificar leitura retrocompatível com DAD-003 antes do uso. `IS_WIRE_BURNT` foi sugestão, não nome ou implementação aprovada.

## Pré-requisitos

- **Tarefas/capacidades:** contrato/ACK e interlock TT&C da INT-004, estados iniciais da INT-003, validação de modos da INT-007 e proteção energética aplicável da INT-010. Reutilizar modelos/codec e persistência existentes.
- **Condições externas:** montagem MOSFET/GPIO 33, nível ativo, estados em boot/reset, duração máxima, energia, autorização, critério de sucesso e novas tentativas acordados com Estruturas/EPS/TT&C. O remoto registra dois circuitos nichrome, 5 V/4 Ω cada e orçamento de 2,5 A/12,5 W transitório; isso não aprova a montagem.
- Definir sensor ou critério de liberação e primeira autorização se a antena for necessária para receber TC. Não depender de comando impossível de receber.

## Verificação de desenvolvimento

Cobrir recusas, interlock recusado, duplicatas dentro/fora da janela, falha de persistência e reinício em cada estado da atuação. Nenhum estado incerto deve fabricar confirmação ou autorização. Preservar argumentos e expectativas dos testes anteriores; casos novos são adicionais.

## Critérios de conclusão

Atuação e recusas implementadas conforme contratos, persistência e compatibilidade tratadas, build e regressões pertinentes aprovados. Definições de segurança ausentes mantêm a tarefa aberta; não substituir por valores arbitrários.

## Encaminhamento para comprovação

[TES-008](../04-testes/TES-008-integracao.md): montagem autorizada, interlock, limite de duração, confirmação física e mínimo original de cinco ciclos bem-sucedidos. [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md): interrupção, duplicatas e saídas após reset. [TES-010](../04-testes/TES-010-missao-e-cobertura.md): integração de missão.

## Evidências

Nenhuma implementação, liberação física ou aprovação registrada por esta revisão.
