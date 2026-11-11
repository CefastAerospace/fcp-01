# COM-003 — Recuperação do barramento I2C após falhas

**Estado:** Concluída — implementação lógica e verificações locais concluídas em 10/11/2026
**Responsáveis:** OBC-DH, ADCS, EPS e Térmico

## Encerramento por responsabilidade — 10/11/2026

O responsável confirmou o encerramento da entrega de implementação lógica.
As evidências abaixo comprovam o núcleo portátil, o executor e a bancada
ESP32 com falhas simuladas. A adequação aos consumidores e a avaliação nas
condições físicas seguem nas tarefas destinatárias.

| Destinatária | Obrigação encaminhada | Situação |
|---|---|---|
| [INT-005](../../03-integracao/INT-005-sensores-e-adcs.md) | Consolidar a política com ADCS/EPS/Térmico, ajustar limites aos consumidores, mapear falhas dos drivers e integrar reconfiguração de sensores e acesso exclusivo ao I2C. | Pendente na integração |
| [INT-008](../../03-integracao/INT-008-supervisao-e-retomada.md) | Integrar diagnóstico e recuperação explícita ao supervisor, incluindo detecção e ação para callbacks ou chamadas do driver que permanecem executando. | Pendente na integração |
| [TES-009](../../04-testes/TES-009-falhas-e-recuperacao.md) | Executar os ensaios de SDA/SCL presos, liberação e esgotamento, medir os limites na montagem e conferir efeitos sobre pedidos e retorno do serviço. | Pendente nos testes |

## Consolidação na integração e nos testes

COM-001 está concluída e a recuperação local foi implementada com os limites provisórios autorizados pelo responsável. Para o aceite com ADCS/EPS/Térmico, consolidar os pontos abaixo.

- [ ] Sinais de falha e como distinguir ausência de dispositivo, transferência expirada, callback que demora e SDA/SCL presos.
- [ ] Limites de detecção, tentativas, duração total e estado final após esgotamento, com referência e unidades.
- [ ] Tratamento de pedidos ativos e pendentes e resultado entregue a cada chamador, preservando contextos em uso.
- [ ] Responsável exclusivo por interromper/reinicializar o barramento e condições para voltar a aceitar pedidos; não reinicializar `Wire` concorrentemente ao executor.
- [ ] Método de verificação local e de injeção física aprovado para TES-009, com critérios de retorno e falha permanente.

Os 20 ms configurados limitam uma transferência. O contexto permanece válido até o callback retornar. A supervisão de callbacks que permanecem executando será tratada na INT-008. Os limites provisórios abaixo aguardam acordo com os consumidores do barramento.

## Implementação local — 10/11/2026

O executor existente concentra callbacks e recuperação. `ExecuteChecked`
acrescenta causas explícitas, com sucesso, NACK, timeout, erro de barramento,
leitura incompleta e falha da operação. `Execute` participa da mesma fila
com seu callback booleano. NACK e leitura incompleta com linhas liberadas
são informados ao consumidor. `requestFrom` curto conserva causa
indeterminada, pois sua API fornece somente a quantidade recebida.

Timeout explícito, erro de barramento ou linha baixa após o retorno do
callback iniciam recuperação. O pedido ativo recebe o resultado original
uma vez. A fila aguarda com seus prazos e prioridades. Novos pedidos
recebem `SERVICE_RECOVERING` durante o ciclo.

A política provisória usa até duas tentativas, orçamento cooperativo de
100 ms, intervalo nominal de 10 ms, até nove pulsos SCL por tentativa,
fases de 5 µs e espera máxima de 1 ms pela liberação de cada linha. A
sequência desativa o controlador, libera GPIOs em dreno aberto, verifica
SCL, aplica clocks quando SDA está preso, gera STOP e reinicializa Wire
nos GPIOs 21/22, a 400 kHz e timeout de transferência de 20 ms.

O orçamento é verificado entre etapas e após as chamadas do driver.
Uma chamada que permanece bloqueada exige a supervisão da INT-008. Em
esgotamento, o serviço publica `UNAVAILABLE`, conclui os pedidos vencidos
com `QUEUE_TIMEOUT` e os demais com `SERVICE_UNAVAILABLE`. `RequestRecovery`
solicita novo ciclo ao worker. `GetDiagnostics` fornece estado, causas,
contadores saturados, fila, início/duração do callback e pilha do executor.

Cada callback deve concluir todas as transações e liberar os locks internos
de Wire antes de retornar, incluindo as sequências de repeated START.
Reconfiguração de sensores após retorno do barramento será integrada com
os consumidores na INT-005. A montagem precisa prover os pull-ups I2C.

## Objetivo

Restabelecer o serviço após falhas recuperáveis sem acesso concorrente ao barramento.

## Entrega

Definir detecção, limite de tentativas, efeito sobre pedidos pendentes e retorno ao chamador. Tratar SDA/SCL presos conforme a montagem e diferenciar timeout de transferência, operação longa e falha física. Timeout de 20 ms não limita um callback completo; uma operação que não retorna impede o progresso atual.

## Pré-requisitos

- **Tarefas:** [COM-001](COM-001-servico-i2c.md)
- **Condições externas:** Acordar política e limites de recuperação com ADCS, EPS e Térmico, encaminhado à INT-005 no encerramento por responsabilidade.

## Critérios de conclusão

Política documentada, recuperação implementada no responsável pelo barramento e falhas sinalizadas. Conferir o comportamento de software; o cenário físico será registrado em [TES-009](../../04-testes/TES-009-falhas-e-recuperacao.md).

## Evidências

- Núcleo portátil e executor real compilados nativamente com C++11,
  `-Wall -Wextra -Werror`, ASan e UBSan. A primeira execução passou com
  3 ciclos, 5 tentativas, 2 recuperações e 1 esgotamento. O runtime nativo
  usa threads e semáforos simulados, com pilha informada como zero.
- Uma execução nativa posterior encontrou intermitência no cenário que
  concedia 1 ms para início do callback. O depurador reproduziu a falha
  aplicando atraso de 5 ms antes do despacho. O resultado foi `QUEUE_TIMEOUT`
  e zero invocações, enquanto o roteiro aguardava a entrada do callback.
  O responsável autorizou usar fila de 1.000 ms e reter o callback iniciado
  por 1.005 ms, conforme as instruções 0 e 7. O cenário ajustado passou em
  cinco execuções consecutivas com ASan/UBSan. Cada execução confirmou
  3 ciclos, 5 tentativas, 2 recuperações e 1 esgotamento. O roteiro verifica
  callback ativo após o prazo e conclusão única com contexto válido.
- Os tempos do cenário ficam nas constantes `context_queue_wait_ms` e
  `context_hold_ms` no início da bancada. A retenção é calculada como prazo
  de fila mais 5 ms, e a espera de diagnóstico como retenção mais 1.000 ms.
  A alteração exige recompilar a bancada. O prazo operacional de fila pode
  ser passado por chamada em `Execute` ou `ExecuteChecked`.
- A regressão existente `I2cSchedulingCheck` passou com UBSan.
- Os builds `obc`, `int006-check` e `int006-delivery-check` passaram. A
  bancada `i2c-recovery-check` passou após os ajustes aprovados de include
  Arduino e qualificação de `i2c::STATUS`. O SDK também define `STATUS`.
- Bancada executada em ESP32-D0WD-V3 revisão 3.1, pela porta `/dev/ttyUSB0`.
  `I2cRecoveryBenchCheck.py` aprovou 3 ciclos, 5 tentativas, 2 recuperações,
  1 esgotamento, pilha mínima livre de 3.276 bytes e callback máximo de
  1.005.392 µs no cenário ajustado. O backend simula GPIOs, controlador e
  relógio da recuperação.
  Executor, tarefas, prioridades e semáforos são FreeRTOS reais.
- A porta apareceu na execução fora do sandbox. O firmware `obc` foi
  restaurado e seu roteiro USB existente aprovou os 75 cenários. O build
  normal ocupou 31.536 bytes de RAM estática e 411.037 bytes de flash.

O build final dos quatro ambientes passou. A bancada final ocupou
24.072 bytes de RAM estática e 288.529 bytes de flash. Depois do último
ensaio com retenção de 1.005 ms, a placa foi novamente devolvida ao firmware
`obc`, com verificação de hash da gravação pelo esptool.

SHA-256 dos binários verificados.

- `obc` — `b82b1f83cf668232bdeb152db802fc6f3117c7a08a79421d9491b675b4f8a7fa`.
- `i2c-recovery-check` — `c7cd1f38ed991e97257891d0cd76df900bfb48a40bc495315afcdcc5fbe7428a`.

Relatórios regeneráveis em `/tmp/fcp01-i2c-recovery-bench.json` e
`/tmp/fcp01-com003-obc.json`. Procedimentos em
[tools/README.md](../../../tools/README.md#recuperação-i2c-com-003).

O acordo ADCS/EPS/Térmico e as medições físicas permanecem pendentes.
A TES-009 recebe o ensaio de SDA/SCL presos e o
comportamento da montagem. As obrigações estão registradas nas destinatárias
indicadas no encerramento por responsabilidade.

## Referências

- Requisitos relacionados: REQ-OBC-007, REQ-OBC-011. Implementação não equivale a requisito validado.
- Origem no planejamento anterior: P-BARRAMENTO; recuperação separada da integração dos consumidores em INT-005.
- [obc-dh/.agents/PINAGEM.md](../../../.agents/PINAGEM.md)
- [obc-dh/.agents/ARCHITECTURE.md](../../../.agents/ARCHITECTURE.md)
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
