# INT-013 — Retomada operacional após reset

**Estado:** Pendente
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos contratos:** coordenação, ADCS, EPS, Térmico, Estruturas e TT&C

## Objetivo

Retomar serviços, modos e estados duráveis após reset sem repetir atuações nem inventar dados temporais.

## Origem e limites

Escopo extraído da [INT-008](INT-008-supervisao-e-retomada.md), incluindo o encaminhamento DAD-003, em 14/11/2026. A recuperação local do escritor permanece na INT-008; a [INT-012](INT-012-watchdog-reset-obc.md) produz o reset. Estados duráveis do Burn Wire pertencem à [INT-011](INT-011-burn-wire-persistencia.md).

## Entrega

- Integrar estado inicial e retorno do ConOps, indicando quais serviços retomam, quais comandos são admitidos e quais ações permanecem bloqueadas.
- Recuperar registros/entregas persistidos da INT-006, conservando IDs, conteúdo, arquivos anteriores e nomes exclusivos por boot/segmento. Não fabricar confirmação cuja gravação falhou.
- Tratar estado ausente, incompleto, inválido e atuação interrompida segundo contratos aprovados. Não derivar autorização de eventos ou flags voláteis.
- Preservar reserva de boot/NVS, tempo monotônico e referências por boot. Não inferir UTC ou intervalo desligado a partir do boot anterior.
- Integrar falhas/limites de retomada e seu estado final à supervisão, sem ciclos ilimitados de recuperação/reset. Receber da INT-012 os parâmetros provisórios e consolidar política/valores definitivos de voo com os responsáveis, ajustando a configuração OBC e suas regressões antes do aceite de missão.

## Pré-requisitos

- **Tarefas/capacidades:** boot da INT-003, diário/entregas/tempo da INT-006, modos da INT-007, recuperação local da INT-008, estados de atuação da INT-011 e ação de reset da INT-012. Proteções da INT-010 entram nas decisões que dependem de energia/temperatura.
- **Condições externas:** ConOps, política de retenção, estados seguros/duráveis e tratamento de interrupções acordados. Simulação permite desenvolver a lógica, mas não substitui essas decisões.

## Verificação de desenvolvimento

Cobrir reinício em cada estado persistente, registro incompleto/inválido, falha de NVS/SD, entregas não confirmadas e confirmações não persistidas. Conferir preservação de arquivos, novo boot, UTC desconhecido e ausência de atuação repetida ou autorização fabricada.

## Critérios de conclusão

Retomada implementada segundo contratos, estados e falhas documentados, build e regressões pertinentes aprovados. Registro de evento isolado não basta para concluir persistência operacional. Ensaios físicos independentes continuam pendentes nas TES destinatárias.

## Encaminhamento para comprovação

[TES-009](../04-testes/TES-009-falhas-e-recuperacao.md) comprova reset/corte reais, arquivos/FAT32, tempo e saídas; [TES-008](../04-testes/TES-008-integracao.md) comprova estados elétricos e transições; [TES-010](../04-testes/TES-010-missao-e-cobertura.md) consolida autonomia e cobertura de missão.

## Evidências

Nenhuma retomada operacional implementada ou aprovada por esta revisão.
