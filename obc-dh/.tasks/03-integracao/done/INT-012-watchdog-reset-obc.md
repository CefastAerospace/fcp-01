# INT-012 — Watchdog e reinicialização do OBC

**Estado:** Concluída — 15/11/2026, mecanismo no escopo de bancada ESP32/USB
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos limites operacionais:** OBC-DH, coordenação e setores afetados

## Objetivo

Reagir à falta de progresso real das tarefas e atender reinicialização comandada de forma controlada.

## Origem e limites

Watchdog extraído da [INT-008](INT-008-supervisao-e-retomada.md) e `CMD_RESET_OBC` da [INT-007](../INT-007-comandos-modos-e-protecoes.md), em 14/11/2026. A [INT-013](../INT-013-retomada-operacional.md) define retomada de voo; este recorte pode ser desenvolvido com o boot de bancada existente. Não assume watchdog externo, redundância ou partição de recuperação.

## Entrega

- Reutilizar diagnósticos de progresso UART/processamento/escritor/I2C da INT-008 e o watchdog disponível no ESP32. Não alimentar o watchdog apenas porque o supervisor continua executando.
- Distinguir tarefas inativas, espera legítima, recuperação limitada e operação sem progresso, incluindo callback/chamada de driver que não retorna.
- Definir prazos, tolerâncias, ação final e comportamento em falha do supervisor, com integração por proprietário do recurso. Não destruir tarefa com callback ativo que ainda usa contexto do chamador.
- Implementar `CMD_RESET_OBC` no consumidor existente: validar argumentos/condições, encaminhar ACK e agendar reset com espera limitada. ACK enfileirado não garante recepção no solo.
- Reutilizar reserva de boot/NVS e causa de reset já presentes no BOOT. Tentar registrar falhas pelos recursos disponíveis, sem exigir sucesso do SD para reiniciar nem declarar persistência inexistente.

## Pré-requisitos

- **Tarefas/capacidades:** boot de bancada da INT-003, consumidor/ACK da INT-004 e diagnósticos existentes da INT-008. Não exigir encerramento global da INT-007 para o recorte de bancada.
- **Condições externas:** prazos e política operacional acordados antes do uso em voo; efeitos do reset sobre saídas dependem da INT-003/INT-011/INT-013. Limites provisórios de desenvolvimento devem seguir o [registro comum](../../README.md#parâmetros-provisórios), sem alterar limites de aceitação existentes.

## Verificação de desenvolvimento

Cobrir progresso normal, fronteiras temporais, espera legítima, travamento de cada tarefa supervisionada, supervisor sem progresso e reset comandado. Conferir ausência de falso reset, acionamento da ação prevista, novo boot e causa de reset, com armazenamento indisponível. Usar injeção finita/isolada e preservar os testes anteriores.

## Critérios de conclusão

Mecanismo e comando implementados, limites/ações documentados, build e regressões pertinentes aprovados no escopo declarado. O recorte de bancada não aprova retorno de voo. A comprovação independente dos tempos/retorno cabe à TES-009. A política e os parâmetros definitivos de retomada de voo são obrigação da INT-013, com origem nesta INT; pendência nessa destinatária não impede concluir o mecanismo no escopo de bancada declarado.

## Encaminhamento para comprovação

[TES-009](../../04-testes/TES-009-falhas-e-recuperacao.md) comprova travamento, watchdog, contagem, causa e tempos de reinicialização. O cenário de retomada de missão depende também da INT-013; [TES-008](../../04-testes/TES-008-integracao.md) mantém o ensaio elétrico de boot/reset.

## Evidências

Implementado e verificado em ESP32-D0WD-V3 rev. 3.1/CH340, somente USB,
sem SD/Pi/rádio físicos. Espressif32 6.6.0, Arduino ESP32 2.0.14 e APIs
IDF 4.x instaladas. Fluxo e TT&C reais, SD/Payload simulados nas bancadas.

### Comportamento entregue

- `ObcWatchdog` decide por cópias de progresso; adaptador ESP32 inscreve
  manualmente loop no watchdog nativo de 5 s, conservando IDLE0 e watchdog
  de interrupção. Arma após Serial.begin, alimenta entre inicializadores
  concluídos e, no runtime, somente após avaliação dos recursos a cada
  100 ms como alvo. Supervisor/coleta bloqueados deixam de alimentar.
- Limites provisórios aprovados pelo responsável neste desenvolvimento:
  UART >500 ms, processamento >5.000 ms, escritor/operação >10.000 ms,
  executor I2C >2.000 ms e TT&C >5.000 ms. Ociosidade I2C publica progresso
  a cada 1.000 ms. Se o progresso retorna antes do disparo, retoma alimentação.
  Tarefas não criadas, indisponibilidade, espera/recuperação com progresso
  e retornos de erro/NO_MEMORY não acionam reset sozinhos. Limites diagnósticos
  da INT-008 permanecem intactos.
- `reset_obc` aceita argumentos ausentes/null/{} e rejeita outros. Primeiro
  pedido válido agenda esp_timer de 1.500 ms antes do registro/ACK; repetições
  não adiam prazo. Callback nativo executa esp_restart, mesmo com ACK cheio
  ou consumidor bloqueado. Timer indisponível/agendamento falho rejeita
  comando sem declarar pendência; falhas de armamento/timer são explícitas.
- EVENT/FAULT 265 registra suspensão/retomada; EVENT 266 tenta registrar
  pedido aceito pela fila existente. Sem dependência de SD ou flush para
  reset. Boot/NVS e causa de reset reutilizados; causa também aparece por
  serial e UTC recomeça desconhecido. Não apaga NVS ou fabrica boot.

Valores definitivos e política de voo continuam com OBC-DH, coordenação e
setores afetados na INT-013; TES-009 comprova tempos/retorno independentemente.
Não há watchdog externo, partição alternativa, destruição de tarefa com
callback ativo ou aprovação elétrica/operacional de missão nesta entrega.

### Verificação e resultados

- Núcleo novo passou fronteiras de cinco recursos, igualdade/excesso,
  tarefas inativas, retomada e aritmética temporal com UBSan e depois
  ASan/UBSan/LeakSanitizer. Executor/recuperação I2C original passou com os
  mesmos sanitizadores, sem desativá-los. Coordenador original passou com
  UBSan: ordem, oito combinações de falhas e inicializadores ausentes.
- Quatorze casos novos ESP32 passaram: comando/repetição, travamentos
  UART/processamento/escritor/I2C/TT&C, supervisor, boot, retomada antes
  do disparo, ACK cheio, consumidor bloqueado e falhas de armamento/criação/
  agendamento. Injeções finitas cedem CPU e cookie RTC exclusivo evita
  repetição após reset. Confirmados novo boot reservado, causas TASK_WDT
  (6) e SW (3), UTC desconhecido e retorno estável sem cartão.
- Timer real medido em 1.500 ms nos quatro casos de bancada por comando;
  o roteiro usa tolerância provisória ±100 ms e 1.400–3.500 ms do agendamento
  até boot visível. Pausa até boot visível: UART 5.911 ms; processamento
  10.470 ms; escritor 15.535 ms; I2C 7.554 ms; TT&C 10.542 ms; supervisor
  5.482 ms; setup 5.441 ms. Observações incluem panic e boot; não são máximos
  certificados. Watchdog de interrupção preservado pode agir antes em outro
  tipo de travamento.
- Pressão de memória/NO_MEMORY e esperas/varredura/recuperação SD passaram
  com watchdog armado e roteiros originais. Regressões originais I2C,
  armazenamento, downlink e recursos passaram nos ambientes compatíveis.
  Total: 25 relatórios USB aprovados, 21 ambientes compilados. As 48
  validações preexistentes foram conferidas por SHA256 e ficaram intactas.
- `obc` restaurado, reset comandado no firmware normal e quatro roteiros
  normais aprovados (downlink, I2C, tarefas e recursos). Firmware SHA256:
  `b67d628bf92fb0ca7a0515b9f4b15b5b140a90628b05f2bd819f5c285c571502`.

### Reprovação inicial e ajuste autorizado

A primeira bancada nova falhou antes de enviar TC: o marcador de boot foi
truncado quando TT&C reabriu a UART e a inicialização antecipada de I2C
fragmentou o heap. Após primeiro buffer de 32.768 bytes, maior bloco de
28.660 bytes não comportou o segundo de 31.729 bytes; admissão deixou
escritor inativo corretamente. A implementação parou para diagnóstico.

Após autorização explícita, somente a bancada passou a concluir o marcador
por Serial.flush antes de reabrir a serial e inicializar I2C no caso dedicado.
Demais casos conservam buffers originais; produção conserva sua sequência.
Nenhum vetor, assert, resultado esperado histórico, buffer ou limite foi
alterado. Reprovação e manifesto parcial permanecem preservados, separados
da reexecução aprovada.

### Entrega ao setor de testes

O [guia](../../../tools/README.md#watchdog-e-reset-obc--15112026) contém a tabela
de ambientes/casos e comandos. O
[manifesto final](../../../examples/obc-watchdog-2026-11-15-manifest.json)
identifica relatórios, hashes de firmware/fontes e escopo. O
[relatório reprovado](../../../examples/obc-watchdog-2026-11-15-case-00-failed.json)
e o [manifesto parcial](../../../examples/obc-watchdog-2026-11-15-partial-manifest.json)
documentam a tentativa anterior. Nenhum commit foi criado; hashes de fontes
identificam o estado com INT-008 e INT-012 locais.

TES-009 recebe mecanismo, versão, ferramentas e instruções para seus casos
de watchdog/reset, mantendo sua execução/aceite independentes. TES-008
conserva ensaio elétrico. INT-013 conserva retomada de modos/missão, estados
duráveis e parâmetros finais de voo. Nenhum cenário/checkbox/estado de TES
foi aprovado ou alterado por esta conclusão.
