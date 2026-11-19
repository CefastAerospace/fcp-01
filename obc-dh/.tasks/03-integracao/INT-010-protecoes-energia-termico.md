# INT-010 — Proteções energéticas e térmicas

**Estado:** Em andamento
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos contratos:** EPS, Térmico e coordenação

## Objetivo

Aplicar proteções e prioridades operacionais conforme limites de energia e temperatura aprovados.

## Origem e limites

Escopo extraído da [INT-007](INT-007-comandos-modos-e-protecoes.md) em 14/11/2026. A [INT-009](INT-009-aquisicao-eps-termico.md) fornece aquisição; a INT-007 coordena modos. Watchdog de tarefas pertence à [INT-012](done/INT-012-watchdog-reset-obc.md), não à proteção térmica.

## Entrega

- Integrar limiares, prioridades, bloqueios, entrada em baixo consumo e condições de retorno com o coordenador de modos.
- Tratar dados ausentes/inválidos e sua ação conforme contrato; armazenar temperatura não comprova proteção.
- Integrar corte de alimentação de periféricos somente quando a interface e as condições forem fornecidas pelos responsáveis. Não presumir sinal dedicado.
- Obter cota energética real e regras para recuperação. Os 5x LM75A previstos monitoram bateria/pontos críticos conforme localização e responsabilidade confirmadas pelos setores.
- Preservar LOW-POWER com rádio em RX e beacon periódico somente em safe-mode, conforme decisão TT&C vigente.

## Pré-requisitos

- **Tarefas/capacidades:** dados válidos/disponibilidade da INT-009, efeitos de controle ADCS da INT-005 e transições/comandos da INT-007. Preparar a lógica com entradas simuladas não requer encerrar os ensaios físicos.
- **Condições externas:** ConOps, cota EPS, limiares térmicos, periodicidade, prioridades, atuação, retorno e comportamento sem amostra aprovados. Níveis e condições elétricas não podem ser arbitrados pelo OBC.

## Verificação de desenvolvimento

Cobrir fronteiras dos limites acordados, amostra inválida/ausente, bloqueio de comandos, prioridades e retorno. Caso o contrato estabeleça limiares diferentes de entrada/saída, verificar ambos. Valores provisórios só verificam a lógica e seguem o [registro comum](../README.md#parâmetros-provisórios).

## Critérios de conclusão

Proteções implementadas segundo contratos, falhas/eventos encaminhados, build e regressões pertinentes aprovados. Não exigir consumo medido nesta entrega de software nem apresentar estimativas como medições.

## Encaminhamento para comprovação

[TES-008](../04-testes/TES-008-integracao.md) comprova transições, resposta térmica e consumo contra cota EPS, incluindo consumo residual do motor desabilitado. [TES-010](../04-testes/TES-010-missao-e-cobertura.md) consolida prioridades e orçamento durante a missão. Preservar todos os critérios originais.

## Evidências

### Recorte de desenvolvimento

O avaliador portátil consome tensão da bateria e cinco temperaturas com
disponibilidade e instante monotônico. Bloqueia novos inícios durante a
carência. Dispara ao atingir um limite, ao perder a validade de uma leitura
obrigatória após a primeira rodada válida ou ao vencer a carência sem essa
rodada. O retorno exige todos os canais na faixa e amostras que confirmem
o fim do período de estabilização. A autorização habilita `exit_safe`.

O TT&C pausa novas tentativas ADS-B e novos pedidos de arquivo durante a
proteção, a parada ADCS pendente por proteção e o modo `LOW-POWER`. Eventos
TX, `cf`, respostas de arquivo já admitidas, ACK, status e TM continuam
em serviço. O código OBC 268 registra causas e elegibilidade no diário.
Rejeições do escritor aparecem no diagnóstico serial e nos contadores.

A política de bancada usa carência e idade máxima de 3 s, amostragem
simulada de 1 s e estabilização de 2 s. A bateria dispara em até 6 V e
retorna a partir de 7 V. T0–T4 disparam até 10 °C ou a partir de 40 °C e
retornam entre 15 e 35 °C, inclusive. O cenário injeta bateria baixa e
depois sobretemperatura em T1, com sobreposição.
[Procedimento](../../tools/README.md#proteções-epstérmico-da-int-010).

Para ativação em voo, EPS/Térmico e coordenação precisam aprovar o ponto
de medição da bateria, cota e critério de potência, mapa dos canais,
limites e tempos, tratamento de invalidade e prioridades. A configuração
operacional usa os produtores da INT-009, a parada ADCS da INT-005 e as
interfaces de corte acordadas com os setores.

### Verificações de 19/11/2026

Base `9731b65` com as alterações locais das INT-007 e INT-010. Builds de
`int010-protection-check`, `int007-modes-check` e `obc` aprovados.
`ObcProtectionCheck` e `ObcModesCheck` passaram com UBSan.

A [execução USB](../../examples/int010-2026-11-19/01-protection-usb.json)
aprovou o disparo, a parada ADCS simulada, o registro das duas causas e a
recuperação por `exit_safe`. Em `LOW-POWER`, o OBC confirmou a entrega
`[27,1]` e recusou `request_log`. A entrega `[27,2]` e a listagem de
arquivos foram atendidas após `exit_safe`. Foram 15 ACKs e 4 TMs, com
`q` máximo de 1 ms e RTT máximo de 132,4 ms.

A [regressão USB da INT-007](../../examples/int010-2026-11-19/02-int007-regression-usb.json)
passou com o roteiro original, 28 ACKs e `q` máximo de 85 ms. O ESP32 voltou
a `obc`. `ObcStationCheck.py` passou com 64 ACKs, 37 TMs, `q` máximo de
105 ms e RTT máximo de 156,6 ms.

| Perfil | SHA-256 do binário |
|---|---|
| `int010-protection-check` | `65d9a47f0ae2bf02e9478171dc8f468f2b6dc2e2563be4a1459aa6e7b3c483f3` |
| `int007-modes-check` | `3495a073270341538eaa2e52c50e9b942b9909243c830e06e1887e129ce4ef92` |
| `obc` restaurado | `97fa43670f81e03081cf5237497d084c05f2b3c17864296c1047d84677427f37` |
