# INT-009 — Aquisição e integração EPS/Térmico

**Estado:** Em andamento — regressão LM75A e diário FCP4 aprovados em bancada. Habilitação em `obc` aguarda os contratos EPS/Térmico.
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos contratos:** EPS e Térmico

## UART0 preservada e diário FCP4 em bancada

O perfil LM75A abre a UART0 uma vez, com TX de 8 KiB. O umbilical
usa `PORT_MODE::REUSE` para validar o driver e a velocidade sem reabrir
a porta. A seleção explícita preserva os consumidores que configuram
uma porta própria. A tolerância de 1% permite a quantização do divisor
UART do ESP32. Depois da inicialização, o wrapper do SDK encaminha
logs Arduino para o mesmo driver via `log_printfv`.

O [roteiro LM75A original](../../examples/int009-uart-reuse-2026-11-18/02-lm75a-usb.json)
capturou o ID de boot e passou. Foram 27 TMs, q máximo de 1 ms,
resposta máxima de 199,132 ms e pilha mínima de 2.452 bytes.

O [FCP4](../../../common/protocol/obc-data.md#aquisição-epstérmico--fcp4)
registra operações de leitura ou configuração dos três INA219,
incluindo Geral, e dos cinco LM75A. A tarefa produtora publica o
resultado depois do callback I2C e sem esperar pelo SD. Fila e
escritor são os mesmos da TM v3. Boot, início e conclusão monotônicos,
geração I2C, resultado e disponibilidade seguem com cada operação.
O leitor exporta arquivos mistos e a recuperação FCD1 atravessa FCP4
sem marcar o segmento como corrompido.

| Verificação | Resultado |
|---|---|
| Builds de `obc`, LM75A, bancada conjunta e FCP4 | Aprovados |
| Codec nativo, vetores independentes, leitor, fila e diário | Aprovados, incluindo corrupção, truncamento, fila cheia, SD indisponível, falha de escrita e recuperação FCD1 |
| Roteiros nativos anteriores de INA219, LM75A, v1/v2/v3 e FCD1 | Aprovados |
| [FCP4 sem comandos](../../examples/int009-uart-reuse-2026-11-18/05-fcp4-usb.json) | 32 operações dos oito canais sem sensores. Poll máximo de 1 ms |
| [Roteiro conjunto original com FCP4](../../examples/int009-uart-reuse-2026-11-18/06-fcp4-with-telemetry.json) | 32 TMs, 62 registros de boot/referência/TM, 45 operações FCP4 capturadas, q e poll máximos de 1 ms |
| [obc restaurado](../../examples/int009-uart-reuse-2026-11-18/08-obc-restored.json) | Firmware normal gravado e cliente original aprovado |

A bancada usou SD simulado e não tinha sensores. As grandezas válidas
foram verificadas no codec com vetores nativos. Cada produtor conserva
seus testes de driver. Pendem o aceite do mapa físico e dos endereços,
modelo térmico, sinais e unidades, cadência e validade, medição solar
e eventual interface BMS. O firmware `obc` segue com a aquisição
desabilitada até esses aceites. INT-009 e TES mantêm seus estados.

## Segunda inicialização da UART0 pela bancada LM75A

O perfil experimental usou `--wrap=log_printfv`. O binário confirmou a
chamada de `__wrap_log_printf` do SDK para `__wrap_log_printfv` da
bancada e o retorno à implementação original antes da prontidão do
driver. Compilação e gravação passaram. O
[roteiro original](../../examples/int009-lm75a-log-v-2026-11-18/04-lm75a-usb.json)
registrou o boot físico e os cinco canais térmicos. Parou antes de
enviar comandos, com `AssertionError: []` no requisito de ID de boot.
A [ligação](../../examples/int009-lm75a-log-v-2026-11-18/02-linkage.txt)
e o [diff experimental](../../examples/int009-lm75a-log-v-2026-11-18/05-log-printfv-experiment.diff.txt)
foram preservados. O código experimental foi retirado e
[`obc` restaurado e verificado](../../examples/int009-lm75a-log-v-2026-11-18/07-obc-restored.json).

O boot abre a UART0 em `setup()` e chama `LogStartup` antes de
`obc_ttc::Init`. O `UmbilicalLink::Init` reassume a mesma porta com
`port.begin(baud)`. O Arduino ESP32 2.0.14 fecha a instância existente
com `end(false)` e remove seu driver com `uart_driver_delete`. Isso
descarta a fila TX pendente. O relatório cortado termina a saída de SD
logo antes de começar a de TT&C, na mesma transição. A segunda
inicialização explica por que redirecionar os logs para o mesmo driver
não recuperou a linha com o ID de boot.

Proposta para a próxima comparação: o OBC abre e configura sua UART0.
O umbilical deve receber a porta já configurada e validar prontidão e
velocidade antes de usar o enquadramento existente. O construtor atual
continua permitindo que outros consumidores configurem uma porta.
A bancada LM75A deve então repetir a captura com TX de 8 KiB e o
redirecionamento já verificado, usando o roteiro original. Conforme
AGENTS.md, o ajuste de propriedade depende de discussão após a
reprovação. Ainda não há medição nova de latência nem persistência FCP4.

## Escrita comum na UART0 da bancada LM75A

O caminho Arduino de `log_printfv` chama `ets_printf` e alimenta a
FIFO diretamente. No mesmo periférico, `Serial.write` e o umbilical
usam o driver UART. Para que as mensagens de SD e Wire passassem pela
fila do driver, foi testado `--wrap=log_printf` junto ao buffer TX de
8 KiB no perfil individual.

A compilação dos objetos passou, mas o linker rejeitou
`multiple definition of __wrap_log_printf`. O SDK Arduino ESP32
2.0.14 já fornece esse símbolo em `libesp_diagnostics.a`. O experimento
foi retirado e a placa não foi gravada. O
[log da falha](../../examples/int009-thermal-serial-2026-11-18/01-build-failure.txt)
e o [diff experimental](../../examples/int009-thermal-serial-2026-11-18/02-common-uart-experiment.diff.txt)
permanecem disponíveis.

A inspeção dos símbolos confirmou que o wrapper do SDK chama
`log_printfv` por referência externa. A próxima proposta é preservar o
wrapper do SDK e interceptar essa referência no perfil LM75A. Antes da
prontidão da UART, a chamada deve cair na função original. Depois da
prontidão, a mensagem completa deve seguir pelo buffer de `Serial.write`.
O mapa do executável deve comprovar os dois caminhos antes da gravação.
Conforme AGENTS.md, a implementação foi interrompida na falha de
ligação. O roteiro original e a persistência FCP4 permanecem pendentes.

## Experimento TX da bancada LM75A — continuação de 18/11/2026

O responsável autorizou a avaliação de `Serial.setTxBufferSize(8192)`
antes de `Serial.begin(115200)` no perfil LM75A individual. A compilação
e a gravação passaram. O roteiro original capturou o boot do ESP32 e
os cinco canais térmicos, mas terminou em `AssertionError: []` ao exigir
um ID de boot. Nesse ponto, o roteiro ainda não havia enviado comandos.
O resultado não permite comparar `q` com os 210 ms anteriores.

O relatório anterior capturou a linha íntegra `[OBC] Tempo/NVS: OK; boot_id=210`.
No experimento, o erro de `sd_diskio.cpp` começou antes do relatório de
boot e dividiu uma linha com TT&C. Uma mensagem `Wire.cpp` também
interrompeu a linha de prontidão TT&C. Esses registros evidenciam a
concorrência de saídas na UART0. O framework pode escrever por
`ets_printf` fora do buffer de `Serial`. A captura não identifica em
qual ponto a linha de NVS se perdeu.

A alteração experimental foi retirada de `main.cpp` e preservada em
[diff](../../examples/int009-persistence-2026-11-18/02-thermal-buffer-experiment.diff.txt).
O firmware experimental tem SHA-256
`870b5c4342c1c79416906d9a217a510f62d907c64345bfcb9426159093050b4c`.
A [reprovação](../../examples/int009-persistence-2026-11-18/01-thermal-tx-buffer.json)
e a [restauração verificada de obc](../../examples/int009-persistence-2026-11-18/04-obc-restored.json)
foram preservadas. As entradas e expectativas do roteiro continuam
intactas. Conforme AGENTS.md, o próximo ajuste precisa ser discutido.
A proposta é confirmar os caminhos de escrita da UART0 e serializar
as mensagens no mesmo transmissor antes de tentar o buffer novamente.
A persistência FCP4 ainda não foi iniciada.

## Diagnóstico UART, prioridade e regressão LM75A — continuação de 18/11/2026

O responsável autorizou continuar após a interrupção pelo poll de 13 ms.
A instrumentação exclusiva de `OBC_HOUSEKEEPING_BENCH` passou a copiar os
tempos de trabalho e espera do transporte. Não imprime nem consulta I2C
na tarefa UART; o loop emite `[HK-UART]` quando observa novos excessos.
As métricas e os critérios anteriores permanecem inalterados.

As duas primeiras execuções instrumentadas do roteiro original passaram,
com poll máximo de 7/8 ms. A
[captura passiva de 90 s](../../examples/int009-uart-diagnosis-2026-11-18/05-passive-timing.json)
reproduziu poll de 13 ms, sem enviar comandos. A amostra de micros registrou
20 µs de trabalho anterior, 6.966 µs de espera e 21 µs de trabalho atual,
próxima a erros Wire em 10.109/10.112/10.119/10.126/10.133 ms.
As leituras de micros e ms são separadas e podem sofrer preempção entre
elas; os 6.986 µs não são conversão exata da métrica de 13 ms. O limite
e a métrica original em ms não foram substituídos.

O [framework instalado](../../examples/int009-uart-diagnosis-2026-11-18/06-framework-evidence.json)
mostra que `Wire::requestFrom` imprime erros por `log_printfv`/`ets_printf`,
sem usar o buffer TX de `Serial`. Esses callbacks executam no proprietário
I2C Core 1/prioridade 5, acima do transporte UART de prioridade 3. As
observações são compatíveis com atraso por execução de maior prioridade;
não constituem prova de cada instrução responsável pelo pico.

Foi ajustada somente a prioridade UART nas duas bancadas conjuntas para
6, permitindo preemptar o executor I2C. `transport_priority` substitui o
literal na criação; demais ambientes conservam prioridade 3. Pilha de
3.072 bytes, ring, baudrate, I2C, cadências, logs e limites permanecem.
Não há alteração do framework nem redirecionamento de logs. O firmware
operacional continua fora da aquisição EPS/Térmico.

| Verificação | Resultado |
|---|---|
| [Cinco builds após instrumentação](../../examples/int009-uart-diagnosis-2026-11-18/01-diagnostic-builds.txt) e [após prioridade](../../examples/int009-uart-diagnosis-2026-11-18/07-priority-builds.txt) | Aprovados |
| [Aquisição conjunta, roteiro original](../../examples/int009-uart-diagnosis-2026-11-18/09-acquisition-priority.json) | Aprovada: 32 TMs/62 registros, q máximo 3 ms e poll máximo 5 ms; ambos os produtores progrediram |
| [Captura passiva após prioridade, 90 s](../../examples/int009-uart-diagnosis-2026-11-18/10-passive-priority.json) | Poll máximo 5 ms, nenhum excesso >10 ms; sem comandos, não é aceite funcional por si só |
| [Fixtures, roteiro original](../../examples/int009-uart-diagnosis-2026-11-18/12-fixtures-priority.json) | Aprovadas: 35 TMs/65 registros, q máximo 3 ms e poll máximo 2 ms |
| [Regressão INA219 original](../../examples/int009-uart-diagnosis-2026-11-18/14-eps-regression.json) | Aprovada em 30 s, q máximo 84 ms |
| [Regressão LM75A original](../../examples/int009-uart-diagnosis-2026-11-18/16-thermal-regression.json) | **Reprovada:** `ping`, `st:ok`, **q=210 ms**, acima de 200 ms |
| [obc restaurado/verificado](../../examples/int009-uart-diagnosis-2026-11-18/18-obc-restored.json) | Upload com hash verificado e cliente original aprovado |

A regressão LM75A usa `int009-lm75a-check`: conserva prioridade UART 3 e
não recebe o buffer TX exclusivo da bancada conjunta. Antes do ACK
reprovado, após o ACK anterior, os logs contêm 51 linhas/6.589 bytes,
equivalentes a aproximadamente 571,962 ms de transmissão a 115.200 baud
8N1. `main::loop` atende comandos e imprime diagnósticos térmicos em
sequência; a ausência de buffer TX é compatível com retenção do consumidor.
Essa hipótese ainda não foi isolada por comparação na bancada individual.
O perfil anterior não foi corrigido e seu `passed:false` foi preservado.

Conforme AGENTS.md, implementação interrompida após essa nova reprovação.
Próxima proposta, ainda não aplicada: avaliar buffer TX nativo no perfil
LM75A individual e repetir o mesmo roteiro, sem omitir logs ou elevar
limites. Codec v4, persistência por operação e ativação operacional não
foram iniciados. A INT-009 permanece aberta; nenhuma TES foi encerrada.
Os [comandos, hashes e limites](../../examples/int009-uart-diagnosis-2026-11-18/manifest.json)
e a [avaliação consolidada](../../examples/int009-uart-diagnosis-2026-11-18/19-acceptance-summary.json)
preservam as aprovações e a reprovação separadamente. Testes anteriores,
entradas e expectativas permanecem intactos.

## Buffer TX nativo e interrupção pelo intervalo UART — 18/11/2026

O responsável autorizou executar o plano completo em etapas, mantendo os
parâmetros provisórios somente nas bancadas enquanto EPS/Térmico não aprovam
os contratos finais. A persistência planejada inclui cada leitura dos três
INA219 e cinco LM75A, independentemente de pedidos de TM, com fila limitada
e perdas explícitas. Essa ampliação ainda não foi implementada.

A primeira etapa acrescentou `Serial.setTxBufferSize(8192)` antes de
`Serial.begin(115200)` em `src/bench/ObcDataFlowCheck.cpp`, somente sob
`OBC_HOUSEKEEPING_BENCH`. Confere retorno da configuração e prontidão da
UART antes de anunciar a bancada. A configuração não comprova alocação:
o driver nativo realiza a instalação em `begin`. Não há fila de logs,
mudança de baudrate ou descarte deliberado de linhas. O custo é de 8 KiB
para dados TX, além das estruturas internas do driver.

O ESP32-D0WD-V3 rev. 3.1/CH340 foi identificado em `/dev/ttyUSB0` fora do
sandbox, somente por USB e sem sensores, cartão, Pi ou rádio. A primeira
compilação no sandbox foi impedida por `platforms.lock` em filesystem
somente leitura, antes de compilar; a repetição autorizada fora dele
aprovou os cinco ambientes. A restrição e a repetição estão preservadas.

| Verificação | Resultado |
|---|---|
| [Cinco builds](../../examples/int009-completion-2026-11-18/02-tx-buffer-builds.txt) | `obc`, INA219, LM75A e duas bancadas conjuntas aprovados |
| [Fixtures, roteiro original](../../examples/int009-completion-2026-11-18/05-fixtures-usb.json) | `passed:true`; 34 TMs, 64 registros; q máximo 17 ms, resposta máxima 136,575 ms e poll máximo 2 ms |
| [Aquisição, roteiro original](../../examples/int009-completion-2026-11-18/07-acquisition-usb.json) | `passed:true`; 32 TMs, 62 registros; q máximo 1 ms e resposta máxima 174,983 ms; **poll máximo 13 ms** |
| [Aceitação adicional do plano](../../examples/int009-completion-2026-11-18/10-acceptance-summary.json) | **Não atendida:** 13 ms excedem o critério UART preservado de 10 ms |
| [Firmware normal restaurado](../../examples/int009-completion-2026-11-18/09-obc-restored.json) | Upload com hash verificado; cliente original `ObcStationCheck.py` aprovado |

Na aquisição, EPS/Térmico iniciaram e progrediram, com margens observadas
de 2.652/2.444 bytes. Todas as operações retornaram falha esperada pela
ausência de sensores: 21 EPS e 35 térmicas. Heap interno amostrado mínimo
de 60.868 bytes e maior bloco mínimo de 47.092 bytes. Esses dados não
comprovam sensores físicos nem todos os máximos simultâneos.

O buffer TX eliminou a violação de q na amostra comparada com os 217 ms
anteriores; não eliminou o intervalo de atendimento UART de 13 ms. A
origem desse intervalo ainda não foi isolada. Não atribuir a causa a
prioridade, callback I2C ou driver sem medição adicional. O roteiro
original não verifica esse campo; seu `passed:true` foi preservado, e a
divergência foi registrada separadamente, sem trocar expectativas.

Conforme AGENTS.md, a implementação foi interrompida antes de acrescentar
o codec v4, callbacks de aquisição, persistência por operação ou ativação
operacional. Nenhum ajuste adicional de prioridade, cadência, buffer ou
limite foi aplicado. As regressões USB individuais previstas não foram
executadas após esta divergência; os builds individuais passaram.

Os [artefatos e comandos](../../examples/int009-completion-2026-11-18/manifest.json)
preservam revisão e hashes anteriores dos testes. Não houve alteração de
testes, entradas, asserts ou expectativas. `obc` permaneceu como firmware
final na placa. INT-009 continua aberta; nenhuma TES mudou de estado.

## Recorte conjunto interrompido — 17/11/2026

Plano aprovado: conectar cópias EPS/Térmico à TM e ao diário existentes,
somente em duas bancadas novas. O responsável escolheu VIN−
(`busVoltage_V`) como convenção provisória e corrente VIN+ para VIN−,
convertida de A para mA, sem inversão. Não estabelece contrato operacional.

### Implementação parcial

- `ObcHousekeeping` compõe somente os campos já existentes de Bateria,
  Solar e cinco temperaturas, sem I/O ou histórico próprio. Limpa
  disponibilidade anterior, preserva campos de outras fontes e recusa
  números não finitos/conversões não representáveis.
- Sob `OBC_HOUSEKEEPING_BENCH`, o fluxo lê cópias fora de seus locks e
  encaminha a mesma amostra ao TT&C e ao codec v3. O JSON conserva
  `tp:null` quando falta qualquer temperatura; o registro mantém os
  indicadores individuais. Geral conserva os diagnósticos anteriores.
- `int009-housekeeping-check` inicia I2C e os dois produtores antes da
  admissão de memória do fluxo. `int009-housekeeping-fixtures-check`
  inicializa I2C, mas não inicia produtores; injeta sete cenários
  determinísticos somente na entrada do mapeador.
- O backend simulado captura bytes aceitos de BOOT, referência e TM v3
  por diagnóstico USB. Não amplia os arquivos FCD1 nem promete SD físico.
- Não foram removidas funções ou alterados drivers, calibrações,
  protocolos, validações anteriores, limites de memória ou watchdog.

### Provas e reprovação preservadas

O [manifesto](../../examples/int009-housekeeping-2026-11-17-183036/manifest.json)
conserva comandos, revisão e hashes. Provas disponíveis:

| Verificação | Resultado |
|---|---|
| Mapeador novo, C++11/ASan/UBSan/LeakSanitizer | Aprovado: composição, zero/negativos, números inválidos, disponibilidade, fronteiras de idade e codec v3 |
| Regressões nativas anteriores EPS/Térmico/fluxo/downlink/codec e leitor | Aprovadas, com sanitizadores ativos e arquivos de testes intactos |
| Contrato TT&C nativo existente | 25/25 casos aprovados |
| Builds `obc`, INA219, LM75A e duas bancadas novas | Cinco aprovados; somente o aviso externo conhecido de `uartSetPins` |
| Primeira bancada de fixtures | **Reprovada**: 33 TMs recebidas e zero registros capturados; escritor não iniciado |
| `obc` restaurado | Upload verificado e `ObcStationCheck.py` original aprovado |

A reprovação está no [relatório USB](../../examples/int009-housekeeping-2026-11-17-183036/17-fixtures-usb.json)
e na [saída do roteiro](../../examples/int009-housekeeping-2026-11-17-183036/17-fixtures-usb.txt).
A mensagem final `TM nao capturada` decorre da indisponibilidade do escritor,
não comprova defeito no hook de captura. O boot registrou:

```text
RAM apos receptor: byte_heap=103368 byte_largest=47092
RAM apos bloco SD: bloco=0 byte_heap=70584 byte_largest=28660
Escritor inativo: memoria por byte insuficiente para tarefas e margem
```

A soma livre não basta para os dois blocos de snapshot de 32.768 e
31.729 bytes. A redução de 32.784 bytes é compatível com somente o
primeiro bloco alocado; o segundo excede o maior bloco restante.
O fluxo recusou iniciar o escritor conforme sua proteção existente:
zero gravações, uma falha inicial e descartes contabilizados. Os sete
cenários produziram TMs, com maior `q` de 84 ms e resposta de 137,751 ms;
isso não aprova o percurso completo. Os arquivos `.records.bin` e
`.records.jsonl` dessa reprovação estão vazios, sem conteúdo fabricado.

Conforme AGENTS.md, a implementação foi interrompida, sem correção
autônoma ou alteração das expectativas. A ordem de reserva dos blocos e
inicialização dos serviços precisa ser discutida, preservando orçamento
para todas as tarefas e as margens existentes. A causa de fragmentação
não foi isolada por comparação adicional em hardware.

Não foram executadas a bancada conjunta de aquisição nem as regressões
USB individuais INA219/LM75A previstas após esta etapa. A comprovação de
registros da nova bancada permanece reprovada; a placa foi devolvida a
`obc` e [verificada pelo cliente original](../../examples/int009-housekeeping-2026-11-17-183036/19-obc-restored.json).
Guia e reprodução: [ferramentas](../../tools/README.md#bancada-conjunta-epstérmico--17112026-em-desenvolvimento).

**Recorte ainda não concluído; INT-009 permanece Em andamento.** Contratos
finais, firmware operacional, persistência completa do Geral e TES mantêm
seus escopos e estados. Não mover esta task para `done/`.

### Continuação autorizada — 17/11/2026

Após o diagnóstico, o responsável autorizou continuar a investigação e o
ajuste da reserva/inicialização. O manifesto anterior foi preservado em
[20-before-resume-manifest.json](../../examples/int009-housekeeping-2026-11-17-183036/20-before-resume-manifest.json).

Foi adicionada `ReserveHousekeepingBuffers`, exclusiva da nova bancada:
reserva os dois blocos do receptor e os dois de snapshot antes de I2C,
TT&C e produtores, sem iniciar UART ou tarefas. `Init` reutiliza os blocos
SD por um helper proprietário, conservando a admissão após os demais
serviços. Os ambientes anteriores mantêm a reserva durante seu próprio
`Init`. Tamanhos dos buffers/pilhas, margem de 4.096 bytes, prioridades,
limites e todos os roteiros foram preservados.

Os [cinco builds](../../examples/int009-housekeeping-2026-11-17-183036/21-reserved-firmware-builds.txt)
passaram. A [segunda execução do mesmo roteiro](../../examples/int009-housekeeping-2026-11-17-183036/23-reserved-fixtures-usb.json)
**reprovou** novamente, com 34 TMs e zero registros. A reserva antecipada
resolveu a alocação dos blocos; o ponto de falha mudou para a criação da
tarefa escritora:

```text
RAM apos receptor: byte_heap=38836 byte_largest=16372
RAM apos bloco SD: bloco=1 byte_heap=38836 byte_largest=16372
stack_budget=19456 margin=4096 reserva=23552
Escritor inativo: falha ao criar tarefa
```

A recepção iniciou transporte e processamento; `xTaskCreatePinnedToCore`
do escritor retornou falha e o fluxo liberou os blocos SD conforme sua
proteção. A soma e o maior bloco anteriores às três criações passaram na
admissão, mas não garantiram todas as alocações seguintes. A distribuição
exata imediatamente antes da criação do escritor ainda não foi medida;
não atribuir a falha a um componente específico sem essa prova.

Maior `q`: 84 ms; resposta máxima: 157,003 ms. Os sete cenários de TM
voltaram a executar, sem comprovar registros. A bancada conjunta de
aquisição e as regressões USB individuais continuam não executadas.

Implementação novamente interrompida conforme AGENTS.md, sem outro
ajuste ou alteração de expectativas. `obc` foi
[restaurado](../../examples/int009-housekeeping-2026-11-17-183036/24-second-obc-restore-upload.txt)
e o [cliente original passou](../../examples/int009-housekeeping-2026-11-17-183036/25-second-obc-restored.json).
A INT-009 e o recorte permanecem abertos.

### Diagnóstico autorizado das criações — 17/11/2026

O responsável autorizou instrumentar a memória antes/depois das três
criações. A instrumentação existe somente sob `OBC_HOUSEKEEPING_BENCH`:
`[HK-ALLOC]` mede heap/maior bloco e resultado, e o callback do SDK copia
tamanho/capacidades da última alocação recusada. O callback usa proteção
curta, sem I/O ou alocação; os logs saem depois, fora da proteção.
Nenhuma pilha, margem, prioridade, ordem ou expectativa foi alterada
nesta etapa. Não houve nova correção de reserva/inicialização.

O SDK instalado foi inspecionado no binário: `xTaskCreatePinnedToCore`
aloca primeiro a pilha solicitada e depois 344 bytes de controle, ambos
com capacidades `0x804` (`MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`).
[Disassembly preservado](../../examples/int009-housekeeping-2026-11-17-183036/27-sdk-task-allocation.txt)
e [literal das capacidades](../../examples/int009-housekeeping-2026-11-17-183036/28-sdk-task-caps.txt)
referem-se à imagem anterior à instrumentação, identificada no manifesto
preservado antes desta etapa. O callback confirmou as mesmas capacidades
em execução.

Os [cinco builds instrumentados](../../examples/int009-housekeeping-2026-11-17-183036/29-diagnostic-firmware-builds.txt)
passaram. A [captura de diagnóstico](../../examples/int009-housekeeping-2026-11-17-183036/31-allocation-diagnosis.json)
foi concluída sem enviar telecomandos ou injetar tráfego Payload:

| Ponto | Heap interno por byte | Maior bloco | Resultado |
|---|---:|---:|---|
| Antes do transporte | 38.820 bytes | 16.372 bytes | Pilha pretendida: 3.072 bytes |
| Após transporte | 35.372 bytes | 16.372 bytes | Criada |
| Após processamento | 26.804 bytes | 8.180 bytes | Criada, pilha de 8.192 bytes |
| Tentativa do escritor | 26.804 bytes | 8.180 bytes | Recusa de 8.192 bytes; retorno −1 |

O callback registrou exatamente uma recusa: `heap_caps_malloc`,
8.192 bytes, capacidades `0x804`. A alocação da pilha falha antes de
tentar o bloco de controle do escritor. O maior bloco é 12 bytes menor
que a pilha pretendida, apesar do total livre. A checagem inicial de
soma/maior bloco não reserva as três pilhas nem garante a distribuição
restante após criar as anteriores. O tratamento de falha existente
conservou o escritor inativo e liberou os buffers SD.

**Diagnóstico concluído; integração permanece incompleta.** O coletor
[CaptureAllocationDiagnostics.py](../../examples/int009-housekeeping-2026-11-17-183036/CaptureAllocationDiagnostics.py)
registra `diagnosis_captured:true`, `functional_acceptance:not_run` e
nenhuma entrada enviada. Isso não transforma as duas reprovações do
roteiro funcional em aprovação.

Próxima correção proposta, ainda não aplicada: criar as tarefas maiores
do fluxo antes de I2C/produtores, contabilizando no orçamento de admissão
as pilhas desses serviços ainda pendentes. Preservar a reserva dos quatro
buffers, os tamanhos das pilhas e a margem de 4.096 bytes; não reduzir a
pilha do escritor para contornar os 12 bytes faltantes. O ajuste precisa
ser discutido antes de retomar o roteiro funcional.

Ao concluir o diagnóstico, `obc` foi
[restaurado com hash verificado](../../examples/int009-housekeeping-2026-11-17-183036/32-diagnostic-obc-restore-upload.txt)
e o [cliente original passou](../../examples/int009-housekeeping-2026-11-17-183036/33-diagnostic-obc-restored.json).
As verificações e contratos anteriores permanecem intactos. A bancada
conjunta de aquisição e as regressões USB individuais continuam pendentes;
não mover a INT-009 para `done/` nem alterar estados das TES.

### Ordem de tarefas aprovada — continuação

O responsável autorizou criar o fluxo antes de I2C/EPS/Térmico e somar
as pilhas pendentes à admissão. A mudança foi aplicada somente à nova
bancada. Reserva dos quatro buffers preservada; margem de 4.096 bytes
e todos os tamanhos/prioridades/roteiros mantidos. O callback diagnóstico
é registrado antes da primeira criação e reutilizado ao iniciar produtores.
O orçamento passa a 23.552 bytes de pilhas em fixtures e 31.744 bytes
em aquisição, mais a mesma margem: reservas de 27.648/35.840 bytes.
As entradas anteriores mantêm seu orçamento e sua ordem.

As ferramentas temporárias não estavam disponíveis nesta sessão e foram
regeneradas das fontes, sem alterar as evidências permanentes. O
[teste nativo novo passou novamente](../../examples/int009-housekeeping-2026-11-17-183036/37-native-check.txt)
com ASan/UBSan e os [cinco builds](../../examples/int009-housekeeping-2026-11-17-183036/38-task-order-firmware-builds.txt)
foram aprovados.

**Fixtures: roteiro funcional original desta bancada aprovado.**
[Relatório](../../examples/int009-housekeeping-2026-11-17-183036/40-task-order-fixtures-usb.json):
34 TMs, 68 ACKs, 64 registros capturados, sete cenários completos e
interpretação pelo leitor OBC real. Inclui zero/corrente negativa,
indisponibilidades, temperatura parcial e retorno; os indicadores térmicos
individuais do v3 foram conferidos mesmo com `tp:null` no JSON. Maior `q`
41 ms; resposta máxima 149,950 ms; heap amostrado mínimo 18.236 bytes e
menor margem do loop 4.892 bytes. As três tarefas do fluxo iniciaram;
nenhuma recusa nas seis observações de criação.

**Aquisição conjunta: reprovada ao iniciar Térmico.** O
[mesmo roteiro, perfil acquisition](../../examples/int009-housekeeping-2026-11-17-183036/42-task-order-acquisition-usb.json)
registrou `eps_started=1`, `thermal_started=0` e interrompeu antes de
enviar TCs. O fluxo e o escritor já haviam iniciado. Uma
[captura passiva de quatro segundos](../../examples/int009-housekeeping-2026-11-17-183036/43-acquisition-passive-diagnosis.json),
sem comandos, mostrou I2C READY, EPS ativo com progresso e Térmico inativo
com serviço READY. Heap interno 12.916 bytes, maior bloco 4.084 bytes;
a tarefa térmica pede 4.096 bytes contíguos. A margem amostrada EPS foi
2.740 bytes. A aquisição real permanece sem verificação funcional completa;
o ensaio aprovado de fixtures não a substitui.

Conforme AGENTS.md, nenhuma nova correção foi aplicada após esta
reprovação. `obc` foi
[restaurado](../../examples/int009-housekeeping-2026-11-17-183036/44-task-order-obc-restore-upload.txt)
e [aprovado pelo cliente original](../../examples/int009-housekeeping-2026-11-17-183036/45-task-order-obc-restored.json).
As regressões USB individuais INA219/LM75A previstas continuam pendentes.

Próxima alternativa para discussão, ainda não implementada: usar
`xTaskCreateStaticPinnedToCore` com pilhas/controles pertencentes aos
adaptadores EPS/Térmico, exclusivamente na bancada conjunta. Preservar
4 KiB por produtor e as políticas atuais; a memória fica reservada
estaticamente e deixa de depender de dois novos blocos contíguos no heap
fragmentado. A admissão deve contabilizar somente as pilhas ainda
dinâmicas, pois as estáticas já reduzem a RAM livre. O custo passa a RAM
estática, sem reduzir tamanhos ou ampliar dados fictícios.

O coletor de diagnóstico arquivado em `31-allocation-diagnosis` pertence
à imagem da etapa 29: ele encerra ao receber o resultado do fluxo. A ordem
atual emite a prontidão dos produtores depois desse resultado; não aplicar
esse coletor histórico à versão atual sem planejar sua evolução separada.
Os roteiros funcionais e suas expectativas permanecem intactos.

**Recorte parcialmente aprovado: fixtures concluídas; aquisição conjunta
aberta.** INT-009 permanece Em andamento e não foi movida para `done/`.

### Pilhas estáticas aprovadas — continuação

O responsável autorizou `xTaskCreateStaticPinnedToCore` para EPS/Térmico
somente na aquisição conjunta. Cada adaptador agora possui sua pilha
alinhada de 4.096 bytes e `StaticTask_t` com vida permanente. As bancadas
individuais conservam a criação dinâmica anterior; fixtures não reservam
essas pilhas nem iniciam os produtores. A admissão passou a contabilizar
somente os 4 KiB dinâmicos pendentes de I2C, pois as pilhas estáticas já
reduzem a RAM livre. Prioridades, afinidade, cadência, validade, limites,
drivers, margem de 4.096 bytes e roteiros permanecem intactos.

Os [cinco builds](../../examples/int009-housekeeping-2026-11-17-183036/47-static-workers-firmware-builds.txt)
passaram. RAM estática da aquisição: 37.792 para 46.680 bytes; flash
576.465 para 576.477 bytes. Isso altera a distribuição da memória antes
do boot, não apenas a criação tardia das tarefas.

O [mesmo roteiro de aquisição](../../examples/int009-housekeeping-2026-11-17-183036/49-static-acquisition-usb.json)
**reprovou na captura de registros**: os dois produtores iniciaram e
progrediram, 32 TMs e 64 ACKs foram recebidos, mas nenhum registro foi
capturado. Sete tentativas por canal sem sensores produziram 21 operações
EPS e 35 térmicas, com falhas esperadas de indisponibilidade. As margens
registradas ao fim foram 2.652 bytes EPS e 2.444 bytes Térmico.

A reserva antecipada do estágio SD voltou a falhar nesta distribuição:

```text
RAM apos receptor: byte_heap=99048 byte_largest=47092
RAM apos bloco SD: bloco=0 byte_heap=66264 byte_largest=26612
stack_budget=23552 margin=4096 reserva=27648
Escritor inativo: memoria por byte insuficiente para tarefas e margem
```

Somente um dos blocos foi alocado: a redução de 32.784 bytes corresponde
ao bloco de 32.768 bytes com overhead. O segundo pede 31.729 bytes,
maior que os 26.612 bytes contíguos restantes. O fluxo liberou os blocos,
conservou recepção/TM e recusou iniciar o escritor. O callback ainda não
estava registrado nessa fase de reserva; não atribuir a ele uma captura
de recusa que não ocorreu.

Nenhum novo ajuste foi aplicado após a reprovação, conforme AGENTS.md.
`obc` foi [restaurado](../../examples/int009-housekeeping-2026-11-17-183036/50-static-workers-obc-restore-upload.txt)
e [aprovado pelo cliente original](../../examples/int009-housekeeping-2026-11-17-183036/51-static-workers-obc-restored.json).
O perfil de fixtures conserva a aprovação funcional da etapa 40;
não foi reexecutado após esta interrupção. As regressões USB individuais
INA219/LM75A previstas continuam pendentes. Nenhuma TES mudou de estado.

Próximo ajuste proposto para discussão, **ainda não implementado**:
na bancada conjunta, reservar o estágio de snapshot SD somente quando
`GetSnapshotSize` informar o tamanho real do registro, usando os blocos
já previstos até o limite atual de 64.497 bytes. Não reservar o máximo
no boot quando ainda não há snapshot. A reserva/realocação deve ocorrer
fora dos locks e somente com o slot exclusivo em ENCODING, sem tocar
em buffers READY/WRITING. Falha mantém descarte contado e serviços
independentes; formatos, limites, entrada Payload e expectativas ficam
preservados. O firmware normal e as bancadas anteriores mantêm seu
caminho até autorização de integração operacional. Esta alternativa
atua no proprietário do estágio SD e evita reduzir pilhas ou tamanhos
para contornar a fragmentação.

**Aquisição e persistência conjuntas ainda não concluídas.** INT-009
permanece Em andamento e não foi movida para `done/`.

### Estágio SD sob demanda aprovado — continuação

O responsável autorizou reservar o estágio SD pelo tamanho real. A
propriedade dos dois blocos foi consolidada em `obc_flow::SnapshotBuffers`:
limite de 64.497 bytes, divisão até 32.768 + restante, falha explícita e
liberação de reserva parcial. Não há ponteiros novos por registro ou
capacidade ilimitada. Os perfis anteriores continuam reservando o máximo
no boot; somente a bancada conjunta chama `GetSnapshotSize` e reserva
durante ENCODING, fora dos locks. O escritor libera os blocos ainda em
WRITING, antes de publicar FREE. Falha de reserva/codificação conserva
descarte contado e não interrompe serviços independentes.

A reserva antecipada da bancada agora prepara somente o receptor.
O helper antigo de reserva máxima foi substituído pelo proprietário
portátil, sem duplicar a política de slot. O backend simulado só examina
a assinatura quando recebe pelo menos quatro bytes: o segundo bloco
exato pode ter 1–3 bytes. Nenhum vetor ou expectativa foi alterado.

Provas novas:

| Verificação | Resultado |
|---|---|
| [SnapshotBuffers nativo](../../examples/int009-housekeeping-2026-11-17-183036/54-staging-native-check.txt) | ASan/UBSan/LeakSanitizer aprovados: 37/32768/32769/64497 bytes, argumentos inválidos, recusa de cada alocação, liberação/retomada e codec real |
| [Regressão do fluxo](../../examples/int009-housekeeping-2026-11-17-183036/57-old-flow-check.txt) | Roteiro original aprovado, com sanitizadores |
| [Snapshot partido](../../examples/int009-housekeeping-2026-11-17-183036/58-old-parts-check.txt) | Vetores/expectativas originais aprovados; bytes preservados e registro parcial não publicado |
| [Cinco builds](../../examples/int009-housekeeping-2026-11-17-183036/59-on-demand-firmware-builds.txt) | Aprovados; [rebuild das duas bancadas](../../examples/int009-housekeeping-2026-11-17-183036/60-short-block-firmware-builds.txt) também aprovado após guarda de comprimento |

**Execução conjunta ainda reprovada, agora por tempo de fila.** O
[roteiro inalterado](../../examples/int009-housekeeping-2026-11-17-183036/62-on-demand-acquisition-usb.json)
iniciou os dois produtores e o escritor, recebeu 19 TMs e registrou 38
ACKs antes da falha: `ping`, `st:ok`, **q=217 ms**, acima dos 200 ms
originais. O limite não foi ampliado. Até ali, o fluxo indicava 89
registros escritos, zero falhas e sete entregas confirmadas. Heap
amostrado mínimo 69.332 bytes; menor bloco 47.092 bytes nas amostras.
O diagnóstico do transporte também registrou `poll_gap_ms=13`; não
declarar atendimento ao critério UART de 10 ms por esta execução.

O roteiro abortou antes de exportar seus arquivos; eles permanecem
vazios e seu `passed:false` foi preservado. Para diagnóstico, os 49
registros dos logs foram exportados para arquivos **novos** e
[interpretados pelo leitor sanitizado](../../examples/int009-housekeeping-2026-11-17-183036/65-partial-captured-records.json):
49 válidos, 19 TMs, zero trechos/bytes descartados. Isso comprova dados
parciais; não transforma o roteiro funcional em aprovação.

A hipótese principal de latência é a saída síncrona de diagnóstico: as
28 linhas imediatamente anteriores ao ACK reprovado somam 3.899 bytes
em UART de 115.200 baud, e EPS/Térmico imprimem seus canais em sequência
no loop que também consome TCs. O framework inicia UART com buffer TX
zero; nenhuma configuração nova de TX foi aplicada. A causa ainda não
foi isolada por uma comparação com buffer TX.

Próxima proposta, **ainda não implementada**: usar o buffer TX nativo de
8 KiB da UART0, configurado antes de `Serial.begin` somente nas duas
bancadas conjuntas. Conferir configuração/recursos e repetir exatamente
os roteiros, mantendo 115.200 baud, q <=200 ms, resposta <=2 s, frames
<=255 bytes e todos os dados. O custo adicional de RAM fica explícito;
não descartar logs/ACKs nem elevar limites para obter aprovação.

Implementação interrompida após a reprovação conforme AGENTS.md.
`obc` foi [restaurado](../../examples/int009-housekeeping-2026-11-17-183036/63-on-demand-obc-restore-upload.txt)
e [aprovado pelo cliente original](../../examples/int009-housekeeping-2026-11-17-183036/64-on-demand-obc-restored.json).
Reexecução de fixtures e regressões USB individuais permanecem pendentes
nesta revisão. INT-009 continua Em andamento; nenhuma TES mudou de estado.

## Recorte LM75A de bancada — 17/11/2026

O responsável aprovou bancada isolada e a referência **NXP LM75A provisória**,
sem sensores reais, publicação operacional, persistência ou proteção térmica.
O [datasheet LM75A_4, revisão 04 de 10/07/2007](https://www.nxp.com/docs/en/data-sheet/LM75A.pdf)
foi conferido; SHA-256
`fc76b747cd9e78c40d60dfe086aee4aafa8e92b2cf185789714eebae50c68de4`.
Fabricante dos módulos reais, montagem e mapa térmico continuam pendentes.

### Implementação e limites

- Novo driver em [LM75A_Standard](../../../eps-tc/LM75A_Standard/README.md),
  namespace `CEFAST::thermal`: Conf de um byte, Temp de dois bytes,
  diagnóstico Wire e conversor separado do I/O. O driver não inicializa o
  barramento, não espera conversão e não escreve Tos/Thyst.
- Conf preserva bits OS 4–1, zera reservados 7–5 e limpa SHUTDOWN. O
  conversor ignora cinco LSB, estende explicitamente o sinal dos onze bits e
  aceita −55 a +125 °C, com resolução digital de 0,125 °C. Falha preserva
  saída anterior; valores fora da faixa não são limitados artificialmente.
- `ObcThermal` mantém cadência/validade por canal.
  `ObcThermalEsp32` usa exclusivamente `ExecuteChecked`, reutiliza o
  classificador Wire portátil existente e converte fora do callback.
  Configuração de outra geração é recusada dentro do executor; o getter
  também revalida recuperação posterior à publicação.
- O histórico fica em RAM, separado de disponibilidade. O diagnóstico
  `temperature_C` só expõe leitura disponível e `last_valid_C` identifica
  histórico; ambos ficam `null` antes da primeira leitura válida.
- Metadados precedem a criação da tarefa; `starting` não simula operação.
  Cópias pequenas usam lock curto, sem I/O ou logs dentro dele. Progresso
  independe da presença dos dispositivos e não alimenta watchdog próprio.
- `main.cpp` recebeu chamadas somente sob `OBC_THERMAL_BENCH`; o novo
  ambiente `int009-lm75a-check` herda `obc` e acrescenta somente a biblioteca
  local térmica. Firmware normal e bancada INA219 não iniciam LM75A.
  Não houve remoção, novo TC, mudança em TM/codec ou alteração dos testes
  anteriores, calibração EPS ou contratos compartilhados.

### Parâmetros provisórios

| Item | Valor e finalidade |
|---|---|
| Canais / endereços | thermal_1 a thermal_5 / 0x48, 0x49, 0x4A, 0x4B, 0x4C; sem posição física atribuída |
| Leitura / espera inicial | 1.000 ms / 1.000 ms após configurar |
| Retentativa offline / validade | 5.000 ms / amostra antiga após idade superior a 3.000 ms |
| Pedido I2C | Prioridade 0, espera na fila de 100 ms |
| Execução / diagnóstico | Core 1, prioridade 1, pilha inicial 4.096 bytes, rodada de 10 ms / USB a cada 1.000 ms |

O datasheet informa conversão típica de 100 ms, sem máximo nessa
especificação. A espera inicial é provisória, não garantia física.
EPS/Térmico e OBC definirão fabricante, mapa final, periodicidade e validade
operacionais. TES-003/008/009 avaliarão os respectivos cenários físicos.
Leituras podem reconhecer OS em modo interrupt; OS não é usado pela bancada.
Esta entrega não estabelece limiares de proteção nem condições de atuação.

### Verificação, reprovação e ajuste autorizado

`ObcThermalCheck.cpp` executou o driver real com registradores Wire
exclusivos em `tests/thermal_native/`. Cinco grupos cobrem conversão
positiva/negativa/zero/fronteiras, protocolo e preservação dos limites,
erros/saídas preservadas, cadência/validade/geração e cinco canais
independentes. O teste não simula tempo físico, sensores ou executor
FreeRTOS. A guarda portátil testada é a mesma usada pelo callback real.

O novo teste e `ObcEpsCheck.cpp` original passaram com C++11,
`-Wall -Wextra -Werror`, ASan/UBSan e LeakSanitizer ativo fora de ptrace.
Comandos e saídas: [nativo LM75A](../../examples/int009-lm75a-2026-11-17-152917/02-native-check.txt)
e [regressão EPS](../../examples/int009-lm75a-2026-11-17-152917/04-eps-regression-check.txt).

O primeiro build conjunto aprovou `obc` e `int009-ina219-check`, mas
reprovou LM75A: `'Serial' was not declared in this scope`.
`Wire.h` inclui Stream, sem declarar Serial; o adaptador novo não incluía
Arduino diretamente. A implementação foi interrompida conforme AGENTS.md.
O responsável autorizou adicionar somente `Arduino.h` no header
`ObcThermalEsp32.h` e retomar o plano. Nenhum teste ou critério foi alterado.
O mesmo ambiente compilou após esse include. [Reprovação preservada](../../examples/int009-lm75a-2026-11-17-152917/05-firmware-builds.txt)
e [recompilação](../../examples/int009-lm75a-2026-11-17-152917/06-lm75a-build-after-include.txt).
Permanece apenas o aviso externo conhecido de `uartSetPins` no framework.

### Bancadas e estado final

ESP32-D0WD-V3 rev. 3.1/CH340 em `/dev/ttyUSB0`, somente USB.
Uploads tiveram hash verificado pelo esptool; nenhum apagamento integral,
reparo de core dump ou limpeza de NVS foi solicitado. A reserva normal de
boot continua usando a NVS existente.

| Recorte | Resultado observado |
|---|---|
| LM75A, 30,013 s | 54 ACKs, 27 TMs; cinco canais indisponíveis e sem medições fictícias. Sete tentativas por canal, intervalos de 5.002–5.012 ms; 31 amostras de saúde READY com progresso; menor margem de pilha térmica 2.448 bytes |
| Tempos LM75A | Maior `q` 180 ms; resposta máxima 224,653 ms; limites originais de 200 ms/2 s/255 bytes preservados; `tp` permanece `null` |
| INA219 original, 30 s | Roteiro inalterado aprovado: 54 ACKs, 27 TMs, maior `q` 146 ms, resposta máxima 184,645 ms e margem mínima EPS 2.656 bytes |
| Firmware normal restaurado | `ObcStationCheck.py` original aprovado: 64 TCs diretos, 37 TMs, maior `q` 116 ms, RTT máximo 156,5 ms e `no_sd` esperado sem cartão |
| Builds | `obc`: RAM estática 32.704 bytes/flash 432.925; INA219: 33.072/444.661; LM75A: 33.200/442.881. RAM estática não mede pico de execução |

Evidências novas, sem sobrescrever históricos:
[LM75A USB](../../examples/int009-lm75a-2026-11-17-152917/08-lm75a-usb.json),
[INA219 original](../../examples/int009-lm75a-2026-11-17-152917/10-ina219-regression.json),
[obc restaurado](../../examples/int009-lm75a-2026-11-17-152917/12-obc-restored.json)
e [manifesto](../../examples/int009-lm75a-2026-11-17-152917/manifest.json).
O manifesto conserva revisão, alterações anteriores desta sessão,
comandos, versões e hashes, inclusive a comparação dos arquivos que deveriam
permanecer intactos. Procedimento no [guia](../../tools/README.md#lm75a-de-bancada--17112026).

O aviso de core dump inválido diagnosticado na INT-004 permanece nos logs,
sem nova intervenção; a imagem não foi apagada ou reparada. Essa ressalva,
reprovações anteriores e limites de evidência são preservados.

**Recorte LM75A de desenvolvimento concluído.** A placa terminou em `obc`,
sem aquisição térmica operacional. A INT-009 permanece **Em andamento**:
contratos finais, integração/publicação operacional, persistência completa
do canal Geral e comprovação com sensores reais continuam nos seus escopos.
Nenhuma TES ou requisito foi aprovado; não mover esta task para `done/`.

## Objetivo

Integrar os dados de energia e temperatura ao OBC, com disponibilidade e erros explícitos.

## Origem e limites

Escopo extraído da [INT-005](INT-005-sensores-e-adcs.md) na revisão documental de 14/11/2026. Aquisição não implementa proteção térmica ou energética; as ações pertencem à [INT-010](INT-010-protecoes-energia-termico.md). A revisão não implementou drivers nem executou ensaios.

## Entrega

- Reutilizar EPS_INA219_Standard e integrar os 3x INA219 pelo executor I2C. A biblioteca INA219 não implementa aquisição/proteção dos LM75A.
- Integrar aquisição dos 5x LM75A conforme o contrato do setor, sem criar valores fictícios quando um dispositivo estiver indisponível.
- Definir com os produtores unidades, validade, localização das temperaturas, origens/códigos e publicação em registros/TM, reutilizando modelos existentes.
- Mapear erros dos drivers em `ExecuteChecked`, concluir transações antes do retorno do callback e integrar reconfiguração após recuperação do barramento.
- Conciliar medição solar por ADC nos PDFs versus sensores digitais propostos com EPS/ADCS; não reservar ADC silenciosamente. Confirmar se o BMS oferece interface de aquisição, sem presumir endereço.

## Pré-requisitos

- **Tarefas/capacidades:** COM-001/002/003, modelos DAD-001 e coordenação de boot existente da INT-003. O encerramento do boot de voo inteiro não impede preparar a aquisição contra contratos disponíveis.
- **Condições externas:** inventário, montagem, endereços finais de sete bits, grandezas/unidades, posições térmicas, periodicidade e limites dos callbacks fornecidos por EPS/Térmico. `0x40`/`0x41` e `0x48` são propostas anteriores, não mapa completo aprovado.
- Parâmetros de desenvolvimento provisórios seguem o [registro comum](../README.md#parâmetros-provisórios); não estabelecem limites de proteção.

## Verificação de desenvolvimento

OBC mantém regressões para sucesso, dispositivo indisponível, leitura incompleta, erro/timeout, validade dos campos e reconfiguração após recuperação. Reutilizar o serviço e os testes existentes, sem duplicar drivers ou algoritmos dos setores.

## Critérios de conclusão

Aquisição integrada, acesso I2C coordenado, mapeamentos documentados, build e regressões pertinentes aprovados. Contrato necessário ainda indefinido mantém a entrega correspondente aberta. Medições por dispositivo e convivência física pertencem à TES-003/TES-008; sua aprovação não é condição de encerramento desta implementação.

## Encaminhamento para comprovação

- [TES-003](../04-testes/TES-003-i2c-hardware.md): operações reais, frequência, duração e falhas por dispositivo.
- [TES-008](../04-testes/TES-008-integracao.md): dados de energia/temperatura na TM e convivência integrada.
- [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md): recuperação física e efeito sobre os consumidores.

## Evidências

Nenhuma implementação ou aprovação registrada por esta revisão. Registrar versão, contratos, parâmetros, regressões e limitações quando executar.

### Recorte INA219 — 16/11/2026, implementação parcial

Plano autorizado: bancada isolada, três canais em RAM/USB, extensão compatível
do diagnóstico I2C e nenhuma publicação EPS na TM operacional. Canal Geral
permanece sem novo registro persistente. LM75A, proteções e contrato final
continuam fora deste recorte.

Implementação presente, ainda sem aceite de conclusão:

- `ObcEps`: cadência/validade por canal e classificação da informação exposta
  por Wire. `ObcEpsEsp32`: tarefa proprietária do driver, operações no executor
  existente, cópias protegidas e diagnóstico USB.
- Driver EPS: sobrecarga `begin(wire, channel)` e diagnóstico adicional da
  operação. APIs anteriores, calibração e cálculos preservados.
- Ambiente `int009-ina219-check`: biblioteca EPS local e aquisição habilitada
  somente por `OBC_EPS_BENCH`. Boot existente reutilizado; firmware normal
  não inicia aquisição EPS.
- Verificação nova `tests/ObcEpsCheck.cpp`, com registradores Wire de ensaio
  exclusivos em `tests/eps_native/`. Não modifica testes anteriores nem
  comprova executor FreeRTOS, sensores físicos ou concorrência na placa.

Parâmetros provisórios: Solar/Bateria/Geral `0x40`/`0x41`/`0x44`; período
1.000 ms por canal; prioridade de pedido 0; espera na fila 100 ms; tentativa
offline a cada 5.000 ms; amostra antiga após 3.000 ms; espera de conversão
arredondada para 2 ms; tarefa Core 1/prioridade 1/pilha inicial 4.096 bytes.
Finalidade: desenvolvimento de aquisição, não limiares de proteção ou aceite
da montagem. EPS/OBC definirão valores operacionais; comprovação física
permanece TES-003/008/009. Corrente mantém VIN+ para VIN− positiva; as duas
tensões são identificadas separadamente.

Verificação nativa executada:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=undefined -Iobc-dh/tests/eps_native -Iobc-dh/headers -Ieps-tc/EPS_INA219_Standard/src obc-dh/src/ObcEps.cpp eps-tc/EPS_INA219_Standard/src/EPS_INA219.cpp obc-dh/tests/ObcEpsCheck.cpp -o /tmp/fcp01-eps-check
/tmp/fcp01-eps-check
```

Compilação aprovada. Execução fora do sandbox aprovada com ASan/UBSan:
`INA219: driver real, disponibilidade parcial, erros, cadencia e retomada aprovados`.
Tentativa dentro do sandbox encontrou a restrição conhecida de LeakSanitizer
sob ptrace; não houve desativação de sanitizers nem mudança de expectativas.

Build em `obc-dh/`: `pio run -e obc -e int009-ina219-check`.

| Ambiente | Resultado |
|---|---|
| `obc` | Aprovado; RAM estática 32.704 bytes, flash 432.925 bytes. Aviso externo já conhecido em `esp32-hal-uart.c:153`. |
| `int009-ina219-check` | Reprovado: macro Xtensa `EPS`, definida como `192` em `xtensa/config/specreg.h:109`, colide com o namespace existente `CEFAST::EPS`. |

Erro decisivo: `error: expected identifier before numeric constant`, na
declaração `namespace EPS`. Os demais erros sobre tipos ausentes decorrem
dessa colisão. O teste nativo não inclui os headers Xtensa e não revela o
conflito. Implementação interrompida conforme AGENTS.md, sem corrigir o
namespace ou modificar o framework por iniciativa própria.

Próxima decisão proposta: usar namespace sem colisão, como `CEFAST::eps`,
atualizando consumidores/exemplo. Isso muda um nome público que o plano
previa preservar; depende de ajuste explícito com o responsável OBC/EPS.

Sem upload ou execução na placa nesta operação. Regressões anteriores ainda
não foram reexecutadas; entradas e expectativas permanecem intactas.
Nenhuma aprovação física ou encerramento da INT-009 foi registrado.

### Continuação autorizada — 16/11/2026

O responsável aprovou o namespace `CEFAST::eps` e o plano de conclusão do
recorte. Driver, consumidor, exemplo, README e referências de tipos do teste
INA219 foram atualizados. Não houve alteração de entradas, asserts ou
resultados esperados; a macro Xtensa e o framework permanecem intactos.
O diagnóstico da reprovação anterior é conservado acima como histórico.

Novo roteiro `tests/ObcEpsBenchCheck.py`: observa 30 s com ESP32/USB sem
sensores, registra os três canais/valores indisponíveis, cadência offline,
progresso/margem de pilha e continuidade TC/TM. Mantém os limites existentes
de entrega ≤200 ms e resposta ≤2 s; usa relatório exclusivo e conserva
resultados parciais em falha. Sintaxe e `--help` aprovados com o Python do
PlatformIO e pyserial 3.5. Não houve execução do roteiro na placa.

| Verificação executada | Resultado |
|---|---|
| Driver/política INA219, ASan/UBSan | Aprovado; disponibilidade parcial, causas, cadência e retomada. |
| Mesmo teste com `-DEPS=192`, ASan/UBSan | Aprovado; sem colisão com a macro. |
| Boot, UBSan | Aprovado; ordem e oito combinações preservadas. |
| Escalonamento I2C, UBSan | Aprovado. |
| Saúde I2C, ASan/UBSan | Aprovado; limites, recuperação, wrap e eventos. |
| Watchdog, ASan/UBSan | Aprovado. |
| Build `obc` | Aprovado; RAM estática 32.704 bytes, flash 432.925 bytes. |
| Build `int009-ina219-check` | Aprovado; RAM estática 33.072 bytes, flash 444.405 bytes. |
| Inspeção dos símbolos ELF | Inicializador/worker EPS presentes somente na bancada. |

Comandos, saídas, revisão-base, hashes e indicação explícita de USB não
executado estão em [obc-eps-2026-11-16-native.json](../../examples/obc-eps-2026-11-16-native.json).
As verificações existentes de boot, escalonamento/saúde I2C, watchdog e
downlink foram comparadas com HEAD e permanecem byte a byte intactas.
ASan/LeakSanitizer foram executados fora do sandbox, sem desativar sanitizers.

Frames estáticos do adaptador: callback 208 bytes, worker 752 bytes e
diagnóstico no loop 1.200 bytes. Não são margem de pilha medida ou soma
completa das cadeias de chamada. O aviso externo de `esp32-hal-uart.c:153`
permanece, sem alteração do framework.

Pendência para concluir o recorte: conectar ESP32, executar o roteiro USB
novo e restaurar/verificar `obc`, conforme o [guia](../../tools/README.md#ina219-de-bancada--16112026).
Nenhuma porta ttyUSB/ttyACM estava disponível nesta operação; não houve
upload. Não declarar execução na placa ou mover esta INT para `done/`.
Contrato final EPS, TM operacional, persistência completa do Geral e LM75A
continuam pendentes nos seus escopos.

### Primeira execução USB — 16/11/2026, após conexão da placa

ESP32 identificado fora do sandbox em `/dev/ttyUSB0`, ponte USB `1A86:7523`,
ESP32-D0WD-V3 revisão v3.1. Upload de `int009-ina219-check` aprovado, com
hashes da flash conferidos pelo esptool. Nenhum código ou validação foi
alterado nesta execução.

O roteiro aprovado reprovou durante a coleta inicial, antes da janela de
observação de 30 s. Primeiro diagnóstico de canal: `channel=SOLAR
address=0x00 available=0 has_valid=0 attempted_ms=0 driver=OK result=0`.
O esperado permanece `address=0x40`, conforme a configuração aprovada.
Não modificar o teste para aceitar endereço zero.

Diagnóstico da causa: `published` inicia zerado; os metadados corretos são
preparados somente no snapshot local de `RunWorker`. A publicação compartilhada
acontece após as operações dos três canais. `ReportDiagnostics` usa como
referência inicial o instante zero do boot e pode executar antes dessa primeira
publicação. No log da falha, o boot já havia ultrapassado 1 s e o executor I2C
ainda estava em callback. Assim, o relatório expõe estado inicial zerado como
se fosse uma amostra publicada. Não é evidência de sensor no endereço `0x00`.

Proposta para discussão, ainda não implementada: inicializar os metadados
compartilhados antes de criar a tarefa, distinguir inicialização de primeira
rodada concluída e emitir diagnósticos de amostra somente após publicação
real. Não acrescentar atraso fixo nem alterar entrada, timeout ou resultado
esperado do roteiro. Interrupção conforme regra 7 de AGENTS.md.

Conforme o procedimento aprovado, `obc` foi restaurado e os hashes da flash
conferidos. `ObcDownlinkCheck.py` original passou: seis ACKs, `q` máximo
11 ms e maior resposta 67,133 ms. A placa ficou com o firmware normal.

Evidências preservadas:

- [Bancada reprovada](../../examples/obc-eps-2026-11-16-usb.json): linhas originais e erro, sem sobrescrever o resultado.
- [Firmware normal restaurado](../../examples/obc-eps-2026-11-16-restored.json): roteiro original aprovado.
- [Manifesto USB](../../examples/obc-eps-2026-11-16-usb-manifest.json): versão da placa, hashes, resultados e limites desta execução.

O recorte e a INT-009 continuam abertos. Nenhuma TES foi aprovada ou mudou
de estado; não houve conclusão por upload, compilação ou diagnóstico.

### Ajuste autorizado e segunda execução USB — 16/11/2026

O responsável autorizou corrigir a inicialização/publicação. Alterações
restritas ao adaptador `ObcEpsEsp32` e à sua documentação de API:

- `MakeInitialSnapshot` concentra canal/endereço e estados `NotInitialized`,
  sem duplicar a preparação entre Init e worker.
- Init publica esses metadados sob proteção antes de criar a tarefa.
- `active` confirma a primeira rodada realmente publicada. Até ela,
  `ReportDiagnostics` informa `starting`, sem emitir amostras ou progresso
  fictícios. Nenhum atraso fixo foi acrescentado.

Os dois builds passaram. `obc` conserva RAM estática 32.704 bytes e flash
432.925 bytes; bancada conserva RAM estática 33.072 bytes e usa flash
444.661 bytes. Upload da bancada e hashes da flash aprovados.

O mesmo `ObcEpsBenchCheck.py`, byte a byte intacto, avançou além da falha
anterior: `starting` foi observado e os canais apareceram em `0x40`, `0x41`
e `0x44`, indisponíveis com medições `null`. Isso verifica o ajuste de
publicação no caso antes reprovado, mas não conclui o roteiro.

Nova reprovação, ainda na coleta inicial: `[EPS-HEALTH] active=1 service=3
progress_ms=2350 age_ms=7 operations=1 failures=1 stack_free_bytes=2736`.
O esperado do roteiro permanece serviço READY (`1`), não UNAVAILABLE (`3`).

Evidência de diagnóstico:

- Primeira configuração Solar: Wire `status=5`, resultado
  `TRANSFER_TIMEOUT`; código do driver `I2C_ERROR`.
- Executor: `cause=5 failure=5 cycles=1 recovered=0 exhausted=1`.
  Os códigos correspondem a `SCL_LOW`, conforme `I2cRecovery.h`.
- Recuperação esgotou com SCL lido baixo. O callback iniciou, ultrapassou o
  limite diagnóstico existente e retornou; a tarefa EPS continuou progredindo.
- Bateria e Geral não foram configurados porque o serviço ficou indisponível.

Ainda não há confirmação da causa elétrica: solicitar informação sobre
jumpers, sensores e pull-ups nos GPIOs 21/22. O framework habilita pull-ups
internos ao inicializar o controlador; a recuperação existente libera as
linhas em dreno aberto e depende dos pull-ups da montagem. Esses fatos não
comprovam, sozinhos, qual ligação ou configuração causou a falha inicial.
Não mudar níveis, recuperação, timeout ou expectativa do roteiro por inferência.

Execução interrompida para diagnóstico conforme AGENTS.md. Não foi aplicada
segunda correção. `obc` foi restaurado com hashes conferidos e o roteiro
original passou: seis ACKs, `q` máximo 31 ms e resposta máxima 72,826 ms.
Não houve mudança dos dois roteiros de validação.

Novas evidências, preservando a reprovação anterior:

- [Bancada USB 02](../../examples/obc-eps-2026-11-16-usb-02.json).
- [obc restaurado 02](../../examples/obc-eps-2026-11-16-restored-02.json).
- [Manifesto USB 02](../../examples/obc-eps-2026-11-16-usb-02-manifest.json).

O recorte continua aberto pelo barramento indisponível; a janela de 30 s não
foi concluída. Nenhuma comprovação física ou conclusão de TES foi inferida.

### Recorte de bancada concluído — execução USB 03, 16/11/2026

O responsável informou que removeu todas as ligações externas e deixou o
ESP32 livre, somente com USB. A mesma bancada e o mesmo roteiro foram
repetidos, sem qualquer alteração de código ou validação desde a execução
02. Hashes de fontes e dos dois firmwares foram comparados e são idênticos.

Upload da bancada aprovado e hashes da flash conferidos. O roteiro
`ObcEpsBenchCheck.py` passou integralmente. Resultados observados:

| Medida | Resultado |
|---|---|
| Janela de observação | 30,038820127 s |
| TC/ACK e TM | 54 ACKs e 27 TMs; campos EPS operacionais continuam `null` |
| Maior `q` | 94 ms, dentro do limite existente de 200 ms |
| Maior resposta | 171,717540001 ms, dentro do limite existente de 2 s |
| Canais | Solar `0x40`, Bateria `0x41`, Geral `0x44`; indisponíveis sem sensores, sem medições fictícias |
| Novas tentativas | Sete instantes por canal; intervalos de 5.002 a 5.011 ms |
| Saúde EPS | 32 amostras, serviço sempre READY (`1`) e progresso contínuo |
| Menor margem de pilha EPS observada | 2.656 bytes; não representa pior caso com sensores reais |

A falha `SCL_LOW` anterior não se repetiu com o ESP32 livre. A comparação
mostra que a condição externa mudou e o software permaneceu igual; não
identifica qual ligação externa produzia o comportamento anterior nem
comprova a montagem integrada. As duas reprovações anteriores são preservadas.

Ao terminar, `obc` foi restaurado com hashes da flash conferidos. O roteiro
original `ObcDownlinkCheck.py` passou: seis ACKs, `q` máximo 10 ms e maior
resposta 97,340 ms. A placa ficou com o firmware normal, sem aquisição EPS
operacional habilitada.

Evidências finais:

- [Bancada USB 03 aprovada](../../examples/obc-eps-2026-11-16-usb-03.json).
- [obc restaurado 03](../../examples/obc-eps-2026-11-16-restored-03.json).
- [Manifesto USB 03](../../examples/obc-eps-2026-11-16-usb-03-manifest.json): métricas, hashes, condições e comparação com a execução 02.

O recorte autorizado de implementação/bancada INA219 está concluído. A
INT-009 inteira permanece Em andamento e não é movida para `done/`: ainda
inclui os contratos finais, publicação operacional, persistência completa
do canal Geral e aquisição LM75A. Ensaios independentes com sensores reais
continuam nas TES-003/008/009, sem alteração de critérios ou estados.
