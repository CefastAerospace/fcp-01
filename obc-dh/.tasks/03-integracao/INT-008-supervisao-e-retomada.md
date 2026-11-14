# INT-008 — Supervisão, watchdog e retomada após falhas

**Estado:** Em andamento — retomada local e diagnósticos de tarefas/I2C concluídos; demais recursos, watchdog e operação pendentes
**Responsáveis:** OBC-DH e setores

## Diagnóstico I2C — recorte local concluído, 14/11/2026

Recorte autorizado: callback acima de 100 ms e recuperação acima de 200 ms,
avaliados pelo loop a cada 100 ms como alvo. `ObcI2cHealth` implementa
avaliação portátil de progresso e disponibilidade. `I2cBus::DIAGNOSTICS`
publica início da recuperação e instante da amostra sob o mutex existente.
`ReportDiagnostics` publica `[I2C-HEALTH]` mesmo sem boot válido; registros
EVENT/FAULT OBC 259 exigem boot válido e usam fila/layout existentes.
Ociosidade não vence; não há cancelamento, reset ou pedido automático de
recuperação. `ObcTaskHealth`, protocolos e validações anteriores permanecem.

O teste portátil novo passou com ASan/UBSan/LeakSanitizer ativo. As regressões
nativas originais de recuperação I2C e supervisão das três tarefas também
passaram, sem alterar suas entradas ou expectativas. Builds aprovados: obc,
i2c-recovery-check, int006-ttc-check, int008-health-check e
int008-i2c-health-check. A bancada nova inicialmente falhou por ambiguidade
entre `STATUS` do SDK e `i2c::STATUS`; após diagnóstico e autorização do
responsável, as oito referências foram qualificadas. A recompilação passou.

### Reprovação preservada — duração da pausa

A execução USB da bancada nova parou em
`GetDiagnostics().maximum_callback_us >= 350000`. As verificações anteriores
confirmaram retorno OK, uma execução e duração da chamada de pelo menos
350 ms, incluindo fila/escalonamento. A duração interna do callback não
atingiu 350.000 us; o valor exato não foi impresso. O diagnóstico USB
detectou suspeita do callback com idade de 133 ms, acima do limite de 100 ms.

O helper de pausa usa `vTaskDelay` em ticks, cuja resolução não garante
uma duração mínima em microssegundos. A duração da chamada completa e a
duração interna do callback também cobrem intervalos diferentes. Proposta
para discussão: garantir o prazo monotônico mínimo na espera da bancada,
cedendo CPU, sem reduzir os critérios de 350/450 ms ou os limites de
diagnóstico. Na interrupção, nenhum ajuste da espera foi aplicado; o
diagnóstico foi apresentado ao responsável antes da correção descrita abaixo.

O firmware `obc` foi restaurado com hash de gravação verificado e passou
no roteiro novo sem sensores: I2C ready/idle, sem suspeita ou recuperação.
Relatórios novos, sem sobrescrever os resultados anteriores:

- [Bancada reprovada](../../examples/obc-i2c-health-2026-11-14-bench.json).
- [Firmware normal restaurado](../../examples/obc-i2c-health-2026-11-14-obc.json).

### Ajuste autorizado e reexecução aprovada

Após autorização explícita, somente o helper `Wait` da bancada foi alterado
para consultar `micros()` até atingir a duração mínima, cedendo CPU com
`vTaskDelay` pelo intervalo restante arredondado para cima em ticks.
Entradas de 350/450 ms, assertivas e limites de 100/200 ms permanecem.
Nenhum teste anterior foi alterado e não houve remoção de funções.

ESP32-D0WD-V3 rev 3.1/CH340 em `/dev/ttyUSB0`, Espressif32 6.6.0,
Arduino ESP32 2.0.14 e alterações locais sobre `777b5f8`. Somente ESP32/USB:
controlador, GPIOs, Payload e SD simulados nas bancadas. Não requer sensores,
cartão, Pi, rádio ou jumpers. Uploads concluídos com hash de gravação verificado.

| Verificação | Resultado observado |
|---|---|
| Bancada I2C nova | Callback suspeito aos 170 ms e recuperação aos 279 ms; uma transição de suspeita em cada atividade, sem repetição ou reboot |
| Contexto do callback | Chamada de 351 ms, uma execução, prazo de fila de 100 ms e callback interno confirmado ≥350.000 us |
| Recuperação e disponibilidade | Quatro ciclos, dois recuperados e dois esgotados; serviço indisponível sem repetição automática; retomada somente por pedidos explícitos da bancada |
| Ociosidade, NACK, leitura incompleta e recuperação normal | Nenhum falso alerta de atraso; ausência de sensores não é indisponibilidade inferida |
| USB durante bancada I2C | 25 ACKs, q máximo de 12 ms, maior resposta de 91,924 ms; EVENT e FAULT 259 observados no backend simulado |
| Regressão original de recuperação I2C | Três ciclos, cinco tentativas, dois recuperados e um esgotado; contexto retido por 1.005 ms para prazo de fila de 1.000 ms; duração interna observada de 1.005.345 us |
| Regressão original de supervisão | 113 ACKs, q máximo de 94 ms, maior resposta de 150,234 ms; sete entregas; varredura de 6.465 ms e espera SD de 5.879 ms sem falso alerta |
| Regressão original de downlink | 44 ACKs, q máximo de 22 ms, maior resposta de 464,491 ms; retentativa identificada e três arquivos de 68 bytes com CRC válido |
| Firmware normal restaurado | Roteiro I2C: 16 ACKs, maior resposta de 62,426 ms, serviço ready/idle sem falso alerta; supervisão anterior: 16 ACKs, maior resposta de 60,875 ms; downlink: seis ACKs, maior resposta de 30,048 ms e `no_sd` esperado |

Menor pilha livre I2C observada: 3.280 bytes na bancada nova e 3.276 na
regressão original. `obc` usa 32.504 bytes de RAM estática e 427.461 de
flash: acréscimo de 56 bytes estáticos e 1.340 de flash sobre o último
firmware de supervisão registrado. A bancada nova usa 36.704/559.497 bytes,
respectivamente. Essas medidas não representam pico completo de heap/pilhas.

Relatórios da conclusão, preservando a reprovação e a primeira restauração:

- [Bancada I2C aprovada](../../examples/obc-i2c-health-2026-11-14-bench-02.json).
- [Regressão de recuperação I2C](../../examples/obc-i2c-health-2026-11-14-recovery-regression.json).
- [Regressão de supervisão](../../examples/obc-i2c-health-2026-11-14-task-regression.json).
- [Regressão de downlink](../../examples/obc-i2c-health-2026-11-14-downlink-regression.json).
- [I2C no firmware normal](../../examples/obc-i2c-health-2026-11-14-obc-02.json).
- [Supervisão no firmware normal](../../examples/obc-i2c-health-2026-11-14-task-obc.json).
- [Downlink no firmware normal](../../examples/obc-i2c-health-2026-11-14-downlink-obc.json).

SHA-256 dos firmwares utilizados na conclusão:

```text
obc: 7af49d0773751ea6f24c9b6d2a9588f03e1bd4db8b44c9a66c44820bd8a5c257
int008-i2c-health-check: e01e4129d60f90d132d02e0e46cdccd7ac04307d72121870cffd9b67fe296ea5
i2c-recovery-check: f1cf3cd2051dfe926de08fec694204155af8e7f33efbf6a271a1fce914af60dc
int008-health-check: 92df56569f90240d6f681eaa82660a109fb49c4d0913b271ef89d9c9aad901bc
int006-ttc-check: 1eeb82870168596978fbbfbdcf08609f684da06168db63ad2b416ae381c192d4
```

Recorte local I2C concluído; `obc` restaurado e verificado. A avaliação é
amostrada, não garante observar recuperações inteiras ocorridas entre duas
amostras. `ok` durante `recovering` significa apenas ausência de atraso,
não disponibilidade. Idades usam aritmética modular u32; a detecção inicial
exige menos de um wrap, e uma suspeita detectada permanece até o fim da
atividade. O diagnóstico depende do loop/escalonamento/mutex funcionando;
não cancela contexto nem recupera driver bloqueado. Fila/SD podem perder
eventos, com perdas contabilizadas. Não comprova funcionamento elétrico,
sensores, persistência física ou watchdog global. Não mover a INT-008 para
`done/`: permanecem TT&C, demais recursos e retomada operacional.

## Supervisão diagnóstica das tarefas — 14/11/2026

Plano autorizado: diagnóstico sem reset/recuperação de tarefas, avaliando
UART, processamento e escritor no loop existente a cada 100 ms como alvo.
O responsável escolheu suspeita após exceder 100/2.000/5.000 ms, respectivamente.
Os limites são diagnósticos iniciais; UART de 10 ms e critérios anteriores
permanecem. Não inclui I2C, perda de comunicação física, rádio ou atuação.

Implementado `ObcTaskHealth` portátil, sem alocação dinâmica, com progresso
publicado pelos próprios responsáveis. Tarefa não criada fica inactive;
atividade sem conclusão pode ficar suspect e volta a ok após progresso.
Retornos de erro/sem dados continuam sendo avanço. Cada operação SD é
instrumentada no adaptador, tanto físico como simulado, preservando contextos
e I/O fora dos locks. Varreduras renovam progresso por leitura/listagem;
a espera entre tentativas SD é distinguida de montagem/escrita pendente.

`[HEALTH]` acrescenta amostras periódicas e transições. EVENT/FAULT existentes
recebem origem OBC/códigos 256–258 e o instante da avaliação; fila/SD
indisponíveis conservam perdas explícitas. Não há promessa de persistência
de todos os eventos. Protocolos, prioridades, tarefas e filas permanecem;
nenhuma função foi removida. O ambiente UART isolado passou a compilar o
novo núcleo porque seu adaptador publica progresso, mantendo dependências
e entradas anteriores. O loop precisa continuar executando para diagnosticar.

### Verificações já concluídas

- `ObcTaskHealthCheck.cpp` passou com C++11, -Wall/-Wextra/-Werror,
  ASan/UBSan e LeakSanitizer ativo, fora da sandbox e sem ptrace. Cobriu os
  três limites e fronteiras, início em zero, tarefa ausente/nunca executada,
  varredura/espera com progresso, suspeita/retorno sem eventos repetidos,
  saturação, amostra temporal anterior à publicação e EVENT/FAULT reais.
- Cinco builds aprovados: obc, uart-receiver-check, int006-ttc-check,
  int008-storage-check e int008-health-check. Mantido apenas o aviso
  preexistente uartSetPins do framework.
- obc: RAM estática 32.448 bytes e flash 426.121 bytes. Acréscimo de
  272 bytes estáticos sobre o último obc verificado; não é medição de pico
  de heap/pilha. int008-health-check: RAM 35.400 bytes, flash 545.745 bytes.
- Roteiros e resultados anteriores preservados. Novo roteiro USB tem
  entradas/expectativas próprias; nenhuma validação anterior foi modificada.

### Bancada USB concluída — 14/11/2026

`int008-health-check` acrescenta pausas de 300 ms/2,5 s/5,5 s na UART,
processamento e escrita, fora dos locks. Depois executa varredura maior
que 6 s com leituras/listagens progredindo e uma falha simulada seguida da
espera real de recuperação SD. O roteiro exige suspeita/retorno uma vez
por tarefa, ausência de falso alerta na varredura/recuperação, sete entregas
e ping/TM dentro dos limites existentes. O modo normal exige escritor
inativo sem cartão, UART/processamento ok e nenhum falso alerta.

Após a conexão informada pelo responsável, a porta foi identificada fora
da sandbox: ESP32-D0WD-V3 rev 3.1/CH340, `/dev/ttyUSB0`. A enumeração na
sandbox não expunha o dispositivo. Execução somente com ESP32/USB, sem
cartão, Pi, rádio ou jumpers, usando Espressif32 6.6.0 e as alterações locais
sobre `777b5f8`. Os três uploads tiveram hashes de gravação verificados.

| Verificação | Resultado observado |
|---|---|
| Pausas UART/processamento/escritor | Um par suspect/ok por tarefa; suspeitas com idades de 158/2.059/5.062 ms, respectivamente |
| Varredura com progresso | 32 operações em 6.465 ms, sem falso alerta |
| Recuperação SD simulada | 5.581 ms incluindo espera real entre tentativas, sem falso alerta ou reboot |
| Comandos na bancada de supervisão | 111 ACKs, q máximo de 76 ms, maior resposta de 166,315 ms; sete entregas no mesmo boot |
| Regressão original `ObcDownlinkCheck.py --bench` | 44 ACKs, q máximo de 41 ms, maior resposta de 452,499 ms; retentativa identificada e três arquivos de 68 bytes com CRC válido |
| Supervisão no firmware normal | 16 ACKs; UART/processamento ok, escritor inactive e nenhuma suspeita; maior resposta de 132,265 ms |
| Regressão original no firmware normal | 6 ACKs, q máximo de 1 ms, maior resposta de 85,966 ms; `no_sd` esperado |

Na bancada de supervisão, menores pilhas livres observadas de 3.104 bytes
no processamento, 4.868 no escritor e 1.332 na UART. A falha SD injetada e
as perdas foram contabilizadas: ao final, 107 registros escritos, uma falha
e 77 descartes. Os intervalos de UART/processamento de 324/2.501 ms são
consequência deliberada das pausas; não comprovam a cadência normal.
Na regressão original de bancada, esses intervalos foram 2/21 ms; no
firmware normal, 2/2 ms. Os critérios anteriores permanecem intactos.

O backend simulado observou FAULT/EVENT 256 e 257, e EVENT 258; as seis
transições foram observadas por USB, sem afirmar persistência de todas.
Fila cheia e indisponibilidade do SD conservam perdas explícitas.

Relatórios novos, preservando as evidências de 13/11:

- [Supervisão com injeções](../../examples/obc-task-health-2026-11-14-bench.json).
- [Regressão original de bancada](../../examples/obc-task-health-2026-11-14-regression.json).
- [Supervisão no firmware normal](../../examples/obc-task-health-2026-11-14-obc.json).
- [Downlink original no firmware normal](../../examples/obc-task-health-2026-11-14-downlink-obc.json).

SHA-256 dos firmwares utilizados:

```text
obc: 1beb2b873cc3cc08768f4b71af007edba5c5b2d6144e188ed5c0fd431bedefb8
int008-health-check: 94f6181d9731a8000e2a247700d6a1b91ed8b92fa01050ad2c012612640261fb
int006-ttc-check: 20ab87cc6bbd18649528749ae2e4fcc639ae5d8635993037a997d1640f5fbac3
```

`obc` restaurado e verificado ao final. Recorte local de diagnóstico concluído;
a INT-008 permanece aberta e fora de `done/`. Não comprova SD físico,
interoperabilidade Pi/RF, supervisão I2C/TT&C, watchdog global ou retomada
operacional. O diagnóstico ainda depende do loop, escalonamento e locks.
Procedimentos no
[guia](../../tools/README.md#supervisão-diagnóstica-das-tarefas--14112026).

## Recorte autorizado — retomada do escritor, 13/11/2026

O responsável OBC autorizou executar este recorte independente das definições
de modos da INT-007, com um ESP32/USB e sem cartão. Escolheu até três tentativas
por episódio, intervalo de 5 s e nenhum reboot automático. A primeira revisão
identificou um bloqueio com cache cheio, preservado no histórico abaixo. Após
autorização explícita, a reserva foi corrigida e o recorte local concluído.
A INT-008 permanece aberta e fora de `done/` por seus demais escopos próprios.

- `Journal::Resume` preserva boot/contadores e avança o segmento sem sobrescrever
  nomes existentes. Não reaproveita `Init` nem repete o registro incerto.
- `StorageRecovery` coordena End/Init/Resume no escritor existente. A espera
  entre tentativas não bloqueia a tarefa; montagem/driver continuam síncronos.
  Erros de fechamento após liberação são diagnosticados; BUSY não autoriza Init.
- BOOT/referências/eventos de recuperação não renovam o orçamento. Um registro
  normal confirmado conclui o episódio; três tentativas sem retomada deixam a
  gravação indisponível até reinicialização manual, conservando UART/TT&C.
- Referência temporal recebida durante recuperação é sincronizada antes de
  liberar admissão. A reconsulta do SD preserva fronteira, IDs e reservas do
  cache. Buffers, filas e perdas explícitas continuam limitados.
- Acrescentados teste portátil, ambiente `int008-storage-check` e roteiro USB
  próprios. Nenhuma entrada, expectativa ou roteiro anterior foi alterado.
  As injeções novas são exclusivas dessa bancada; não houve remoção de funções.

### Evidências locais

Base Git `777b5f8`, com as alterações locais desta entrega. ESP32-D0WD-V3
rev 3.1/CH340 em `/dev/ttyUSB0`, somente USB, sem Pi, rádio ou SD físicos.
Plataforma Espressif32 6.6.0, Arduino ESP32 2.0.14, GCC Xtensa 8.4.0,
ArduinoJson 7.0.4 e LoRa 0.8.0. Comandos reproduzíveis no
[guia](../../tools/README.md#retomada-do-escritor-sd--13112026).

| Verificação | Resultado |
|---|---|
| Teste portátil novo, C++11/-Wall/-Wextra/-Werror/ASan/UBSan | Aprovado: prazos e wrap u64, renovação por escrita normal, esgotamento sem quarta tentativa, BUSY, colisão, BOOT/referência falhos, cauda de snapshot preservada, referência atual e IDs em trânsito |
| Regressões originais, sem modificar vetores | ObcDataFlowCheck, ObcDeliveryCheck, ObcStorageCheck e SnapshotPartsCheck.py aprovados com ASan/UBSan; leitor com UBSan, incluindo registro parcial não publicado |
| Builds | obc, int006-check, int006-ttc-check, int006-stress-check e int008-storage-check aprovados |
| Bancada ESP32 nova | Três episódios, seis tentativas, duas retomadas, um esgotamento, sete entregas; sem reboot entre episódios ou quarta tentativa após esgotamento |
| Atendimento durante recuperação | 341 ACKs, q máximo de 43 ms, maior resposta de 371,492 ms; UTC atualizada durante montagem, nomes exclusivos, transporte UART máximo de 2 ms |
| Pilhas mínimas observadas | Processador 3.104 bytes, escritor 5.160 bytes, transporte 1.332 bytes |
| Regressão USB original de entregas/extração | ObcDownlinkCheck.py --bench aprovado: 46 ACKs, q máximo de 24 ms, maior resposta de 453,077 ms e três arquivos extraídos com CRC válido |
| Firmware normal | obc restaurado com hash de upload verificado; ObcDownlinkCheck.py sem --bench aprovado, com no_sd esperado sem cartão |

Intervalos observados, em ms monotônicos do mesmo boot: falhas em
12.986/24.296/30.345; tentativas em
18.000/23.304/29.334/35.353/40.684/46.012. Todas aguardaram pelo menos
5 s após a falha ou conclusão da tentativa anterior. O roteiro continuou
por mais de 6 s após observar esgotamento. Ao final: 84 registros confirmados,
sete falhas de armazenamento e 245 descartes explícitos sob o tráfego do roteiro.
Esses valores não representam taxa nominal ou conservação de todos os dados.

Evidências permanentes, sem substituir relatórios anteriores:

- [Bancada de recuperação](../../examples/obc-storage-recovery-2026-11-13-bench.json).
- [Regressão USB](../../examples/obc-storage-recovery-2026-11-13-regression.json).
- [Firmware normal restaurado](../../examples/obc-storage-recovery-2026-11-13-obc.json).

SHA-256 dos firmwares verificados:

```text
1390fc0c3e3af670e123e25fb3474e540dd19e5bc16bee2bb3146c21916f842e  obc
ba75e22fb00a893df8afa02a2564e9d3c33d36e3df1f0fcaba4517164907d064  int008-storage-check
605e3221c07ee4e866bc62ed68ba609e6f1f0dea0c41da50911311b1351dc1a4  int006-ttc-check
```

O primeiro lançamento ASan dentro da sandbox encontrou incompatibilidade
do LeakSanitizer com ptrace. A execução fora da sandbox passou sem desativar
sanitizadores ou alterar código/critérios. O aviso uartSetPins do framework
permanece preexistente. Não houve commit.

### Histórico do impedimento — discussão anterior à correção autorizada

A nova chamada `Queue::RecheckStorage` marca `disk_checked_ = false` após
remontagem. Se a falha anterior de leitura liberou admissões e o cache encheu
com 32 entregas não persistidas, não há posição livre ou cópia durável que
`Reserve(true)` possa substituir. Encontrar uma entrega pendente antiga no
SD produz `Restore = false`. A consulta continua pendente e `BeginTransmit`
recusa envios, inclusive retentativas que poderiam liberar capacidade.

Reprodução nativa em 13/11 com o código real e UBSan, sem alterar testes
existentes. O programa foi compilado em `/tmp/fcp01-recovery-full-cache` com
os mesmos seis fontes portáteis do teste novo. Corpo reproduzível:

```cpp
static obc_data::PAYLOAD_SNAPSHOT snapshot = {};
snapshot.stamp = {7, 1};
snapshot.aircraft_count = 1;
obc_delivery::Queue queue;
queue.Init(7, true);
queue.ReaderFailed();
for (unsigned i = 1; i <= 32; ++i) {
    snapshot.aircraft[0].icao = i;
    queue.ObserveValidatedSnapshot(snapshot);
}
obc_delivery::RECORD record = {};
while (queue.BeginStorage(record)) queue.FinishStorage(record, false);
const obc_delivery::DELIVERY older = {{6, 1}, 1, {}};
queue.RecheckStorage();
const bool restored = queue.Restore(queue.GetFrontier(), &older);
obc_delivery::DELIVERY next = {};
const bool transmit = queue.BeginTransmit(100000, 10000, next);
```

Observado: `occupied=32 restored=0 needs_recovery=1 transmit=0`.
O resultado impediu o aceite deste recorte. As verificações anteriores não
cobriram a combinação de remontagem, cache cheio não durável e entrega antiga
fora do cache; seus resultados permanecem válidos apenas nos recortes descritos.
Foi solicitada discussão da reserva e inclusão desse cenário antes de
prosseguir. Naquele momento, nenhuma correção foi aplicada, conforme
a regra 7 do AGENTS.md. O ESP32 ficou em obc, verificado sem cartão; sem SD
no boot, o caminho de retomada não é iniciado.

### Correção autorizada da reserva e conclusão local — 13/11/2026

O responsável autorizou ajustar a reserva após receber o diagnóstico.
`Queue` ganhou uma posição fixa exclusiva para recuperação já persistida,
usada somente quando as 32 posições originais não podem ceder espaço.
Admissões e evicções comuns continuam limitadas ao cache original. Os
consumidores existentes de TX/CONFIRM e a busca por ID também atendem a
reserva; sua liberação não decrementa a ocupação do cache. Não há nova
alocação dinâmica em produção, descarte das 32 cópias ou alteração de FCD1/TT&C.

`occupied`/`high_water` preservam as 32 posições originais. O novo diagnóstico
`recovery_slot` e `STATISTICS::recovery_occupied` mostram a posição adicional.
O máximo físico é 32 cópias do cache mais uma recuperação durável. A fila
mede 2.920 bytes no PC e 2.912 no ESP32, acréscimo fixo de 72 bytes em ambos.

O reproducer original, recompilado com UBSan e sem alterar suas entradas,
passou a observar `occupied=32 restored=1 needs_recovery=0 transmit=1`.
`DeliveryRecoveryCheck` acrescenta prova compartilhada PC/ESP32: preserva
os 32 IDs/conteúdos, recusa a 33ª admissão, recupera duas entregas antigas,
verifica CONFIRM em gravação/falho, drena a fila e preserva TX já em uso.
Os testes e expectativas anteriores não foram alterados.

| Verificação após correção | Resultado |
|---|---|
| Reserva nova, regressão original de entregas e retomada SD | Aprovadas com C++11, -Wall/-Wextra/-Werror e ASan/UBSan, sem desativar LeakSanitizer |
| Builds finais | obc, int006-ttc-check e int008-storage-check aprovados; aviso preexistente uartSetPins preservado |
| Reserva no ESP32 | `[DELIVERY-RECOVERY-CHECK] PASS`, antes de iniciar a bancada integrada; memória temporária liberada antes do receptor |
| Bancada integrada preservada | 339 ACKs, q máximo de 52 ms, maior resposta de 339,611 ms; três episódios, seis tentativas, duas retomadas e um esgotamento; UART máximo de 2 ms e escritor com pelo menos 5.160 bytes livres de pilha |
| Regressão USB original | 46 ACKs, q máximo de 52 ms, maior resposta de 457,390 ms; retentativa identificada e três arquivos com CRC válido |
| Firmware final na placa | obc restaurado com hash de upload verificado; ObcDownlinkCheck.py sem --bench aprovado, sem cartão e com no_sd esperado |

Procedimentos nativos/USB no guia de ferramentas. Evidências novas, mantendo
os relatórios anteriores e o histórico do impedimento:

- [Bancada após correção](../../examples/obc-storage-recovery-2026-11-13-bench-02.json).
- [Regressão após correção](../../examples/obc-storage-recovery-2026-11-13-regression-02.json).
- [Firmware final restaurado](../../examples/obc-storage-recovery-2026-11-13-obc-02.json).

SHA-256 dos firmwares finais:

```text
5c9c494aeebfaa93d499602cdbf2610fd434dda8391449bd0180aef4d53f910f  obc
fa2c2e9fd4854288239ba875c4639c5534c20dc314fd609f00602d016a2bf27d  int008-storage-check
ddfa0ccf9f2a668797955a07b8b505ae2043042ffc7a9360b8bbba40a17efff9  int006-ttc-check
```

Recorte local concluído. Supervisão de tarefas/I2C/TT&C, watchdog, modos e
retomada operacional continuam pendentes nesta INT-008; não mover a tarefa
inteira para `done/`. Ensaios físicos continuam nas TES destinatárias.

### Limites e encaminhamento

Não há hotplug quando SD/tarefa estavam ausentes no boot, recuperação de
esgotamento de memória, reparo/formatação FAT32 ou garantia de recuperar dados
descartados. Perda de cartão durante execução, remontagem física, corte de
energia e durabilidade após reset permanecem para TES-006/TES-009. O recorte
não habilita watchdog/reset do OBC nem define retomada de missão/atuadores.

## Objetivo

Detectar falhas e perda de comunicação, recuperar recursos e registrar o ocorrido.

## Entrega

Supervisionar tarefas críticas e recursos, distinguindo watchdog de tarefas de proteção térmica. Reutilizar GetLinkStats()/last_tc_ms e a saúde/reset do SX1276. Associar horários, persistir falhas e contagem de reinicializações, recuperar periférico/tarefa/OBC e retornar ao estado do ConOps. Não assumir watchdog externo ou partição de recuperação do DP como implementação confirmada.

## Pré-requisitos

- **Tarefas:** [COM-003](../01-comunicacao/done/COM-003-recuperacao-i2c.md), [INT-007](INT-007-comandos-modos-e-protecoes.md)
- **Condições externas:** Definir saúde, tempos, tentativas e retomada com coordenação e setores.

## Definições e condições

- Preservar limites TT&C de entrega TC ao OBC ≤200 ms e resposta ≤2 s; definir detecção/recuperação e critérios de saúde/tentativas por periférico/tarefa/OBC. Reutilizar estatísticas e reset do rádio existentes; ausência ADS-B não equivale à perda de comunicação.
- Considerar limites físicos: watchdog não recupera falha física do ESP32; não existe redundância física confirmada de OBC/armazenamento.

## Critérios de conclusão

Supervisão, limites e recuperação implementados, eventos registrados e verificações locais disponíveis. Atendimento aos tempos e integridade após falhas depende de [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md).

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Integrar a política de retomada do escritor após falha/reset: preservar arquivos, End/Init e nome novo, sem apagar ou reutilizar conteúdo anterior. | Retomada local após falha concluída em 13/11, incluindo reserva com cache cheio; política operacional/reset e comprovação física pendentes |
| [DAD-003](../02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) | Integrar retomada persistente por boot e falhas com os estados definidos em INT-007, sem deduzir autorização de atuação de eventos ou flags voláteis. | Pendente |

## Encaminhamento recebido da COM-003 — 10/11/2026

A [COM-003](../01-comunicacao/done/COM-003-recuperacao-i2c.md) fornece recuperação
no executor, estado `UNAVAILABLE` após esgotamento, `GetDiagnostics` e
`RequestRecovery`. Integrar esses recursos à supervisão e definir detecção
por progresso, limites e ações para callbacks ou chamadas internas do driver
que permanecem executando. O contexto do callback pertence ao chamador até
sua conclusão. O diagnóstico de callback/recuperação e indisponibilidade foi
concluído localmente em 14/11, conforme evidências acima. Ações para driver
sem retorno, watchdog e integração dos consumidores continuam pendentes.

## Evidências

As evidências dos recortes locais de retomada do escritor, diagnóstico das
três tarefas e diagnóstico I2C estão acima. Supervisão dos demais recursos,
watchdog e retomada operacional permanecem abertos; conservar critérios e dependências.

## Referências

- Requisitos relacionados: REQ-OBC-007, REQ-OBC-009, REQ-OBC-010, REQ-OBC-011, REQ-NF-OBC-003, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [tt-c/include/Ttc.h](../../../tt-c/include/Ttc.h)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-MISSAO.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
