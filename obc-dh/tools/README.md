# Codec e leitura de registros OBC v1/v2/v3/v4

## Bancada SD física da TES-006

`tes006-sd-check` usa o serviço `ObcStorageEsp32` e o cartão real. Cada
cenário publica os resultados e os tempos na UART a 115200 baud e chama
`End` antes de concluir. Usar um identificador hexadecimal novo para cada
rodada nominal. Os arquivos ficam em `obc/` no cartão.

```bash
pio run -e tes006-sd-check -t upload --upload-port PORTA
python3 tests/ObcStorageHardwareCheck.py PORTA nominal ID --report NOMINAL.json
python3 tests/ObcStorageHardwareCheck.py PORTA absent ID --report AUSENTE.json
python3 tests/ObcStorageHardwareCheck.py PORTA full ID --report CHEIO.json
python3 tests/ObcStorageHardwareCheck.py PORTA recovered ID --report RETORNO.json
pio run -e obc -t upload --upload-port PORTA
```

Desligar o ESP32 antes de retirar ou inserir o cartão. Para a comparação
pelo leitor, conferir `t6<ID>n.bin` contra os seis bytes `ABCDEF` e o
`extracted_hex` do relatório. Para `full`, preparar no leitor arquivos de
preenchimento fora de `obc/`, deixando uma unidade de alocação livre e
espaço para duas entradas curtas de diretório sob `obc/`. O relatório
identifica `full_terminal` com o índice do primeiro bloco que falhou.
O prefixo confirmado de `t6<ID>f.bin` consiste em `ABC` e os blocos
`full_append` com sucesso, de 4 KiB cada, preenchidos com `0x30 + índice`.
Comparar esse prefixo e medir a cauda pelo leitor. Remover o preenchimento
antes de `recovered` e ao concluir preservar os relatórios da rodada.

## Proteções EPS/Térmico da INT-010

`int010-protection-check` usa a bancada da INT-007 com leituras simuladas.
A segunda partida ADCS injeta bateria baixa e depois sobretemperatura em
T1. O roteiro confirma uma entrega por `cf` em `LOW-POWER` e verifica que
a próxima entrega aguarda `exit_safe`. Registra TCs, TM e eventos do diário.

Executar em `obc-dh/`, com Python contendo pyserial para a etapa USB.

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined \
    -fno-sanitize-recover=undefined -Iheaders src/ObcProtection.cpp \
    src/ObcModes.cpp tests/ObcProtectionCheck.cpp -o /tmp/fcp01-int010-protection-check
/tmp/fcp01-int010-protection-check
pio run -e obc -e int007-modes-check -e int010-protection-check
pio run -e int010-protection-check -t upload --upload-port PORTA
python3 tests/ObcProtectionBenchCheck.py PORTA --report NOVO-RELATORIO.json
pio run -e obc -t upload --upload-port PORTA
```

## Modos e janela ADS-B da INT-007

`int007-modes-check` usa um ESP32 e USB. Reutiliza o umbilical real e
simula Payload, SD, ADCS e proteção. A bancada inicia em `safe`. O
`set_time` libera os snapshots simulados e o limite de frescor de 4 s
vale somente para esse perfil. O segundo início ADCS injeta proteção por
5 s. O roteiro aguarda a gravação da primeira parada ADCS antes de
disparar essa etapa. O teste nativo cobre o prazo nominal de 600 s por
relógio simulado.

Executar em `obc-dh/`, com Python contendo pyserial para a etapa USB.

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-sanitize-recover=undefined -fno-omit-frame-pointer -Iheaders \
    src/ObcModes.cpp tests/ObcModesCheck.cpp -o /tmp/fcp01-int007-modes-check
/tmp/fcp01-int007-modes-check
pio run -e obc -e int006-ttc-check -e int007-modes-check
pio run -e int007-modes-check -t upload --upload-port PORTA
python3 tests/ObcModesBenchCheck.py PORTA --report NOVO-RELATORIO.json
pio run -e obc -t upload --upload-port PORTA
```

O relatório reúne ACKs, TM, marcadores e os instantes dos snapshots aceitos
pelo escritor simulado. O perfil `obc` usa `bench`. A INT-007 registra as
condições de ativação em voo.

## Diário de aquisição FCP4

`int009-acquisition-records-check` liga os dois produtores ao diário simulado.
O escritor emite `[HK-ACQUISITION]` com os bytes aceitos. O roteiro captura
esses bytes sem enviar comandos e pede ao leitor C++ que valide e exporte
os registros. Use um ESP32 ligado só por USB, sem sensores ou cartão.
O ambiente usa endereços provisórios e compartilha a UART0 sem reabri-la.

Dentro de `obc-dh/`, com Python contendo pyserial para a etapa USB:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp src/ObcDelivery.cpp \
    tools/ReadRecords.cpp -o /tmp/fcp01-read-records
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp src/ObcStorage.cpp \
    src/ObcDataFlow.cpp tests/AcquisitionRecordsCheck.cpp -o /tmp/fcp01-acquisition-check
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp src/ObcStorage.cpp \
    src/ObcDataFlow.cpp src/ObcDelivery.cpp tools/DeliveryLogs.cpp -o /tmp/fcp01-delivery-logs
python3 tests/AcquisitionRecordsCheck.py /tmp/fcp01-acquisition-check \
    /tmp/fcp01-read-records /tmp/fcp01-delivery-logs
pio run -e int009-acquisition-records-check -t upload --upload-port PORTA
python3 tests/AcquisitionRecordsBenchCheck.py PORTA \
    --reader /tmp/fcp01-read-records --report NOVO-RELATORIO.json
pio run -e obc -t upload --upload-port PORTA
```

O [contrato FCP4](../../common/protocol/obc-data.md#aquisição-epstérmico--fcp4)
define tamanhos, unidades e disponibilidade. A bancada USB verifica
configurações sem sensores e escrita no backend simulado. Medições
válidas são exercitadas no codec nativo com vetores independentes.

## Bancada conjunta EPS/Térmico — 17/11/2026, em desenvolvimento

**Continuação de 18/11/2026:** transporte UART em prioridade 6 somente nas
bancadas conjuntas, acima do executor I2C de prioridade 5. O diagnóstico
`[HK-UART]` copia tempos no transporte e imprime no loop, sem mudar a
métrica ou o limite anterior. Após reproduzir poll de 13 ms em captura
passiva, o ajuste passou nos roteiros originais de
[aquisição](../examples/int009-uart-diagnosis-2026-11-18/09-acquisition-priority.json)
e [fixtures](../examples/int009-uart-diagnosis-2026-11-18/12-fixtures-priority.json):
poll máximo de 5/2 ms; q máximo de 3 ms em ambos. Nova captura passiva de
90 s também atingiu no máximo 5 ms. Esses resultados não aprovam voo ou
todos os perfis. A regressão INA219 passou, porém a
[LM75A individual reprovou](../examples/int009-uart-diagnosis-2026-11-18/16-thermal-regression.json)
em ping q=210 ms contra 200 ms. Esse ambiente mantém TX/prioridade originais.
Nenhuma outra correção aplicada; persistência por leitura ainda não iniciada.
[obc restaurado/verificado](../examples/int009-uart-diagnosis-2026-11-18/18-obc-restored.json).
Diagnóstico, limitações e proposta seguinte estão na INT-009; comandos e
hashes no [manifesto](../examples/int009-uart-diagnosis-2026-11-18/manifest.json).

**Etapa anterior de 18/11/2026:** as duas bancadas conjuntas configuram o buffer
TX nativo de 8 KiB antes de `Serial.begin(115200)`, verificando configuração
e prontidão da UART. Cinco builds e os roteiros originais passaram:
[fixtures](../examples/int009-completion-2026-11-18/05-fixtures-usb.json)
com 34 TMs/64 registros e q máximo 17 ms;
[aquisição](../examples/int009-completion-2026-11-18/07-acquisition-usb.json)
com 32 TMs/62 registros e q máximo 1 ms. Porém, poll UART de 13 ms excede
o critério de 10 ms do plano. A
[aceitação adicional](../examples/int009-completion-2026-11-18/10-acceptance-summary.json)
permanece não atendida, sem alterar `passed:true` dos roteiros originais.
Implementação interrompida conforme AGENTS.md; nenhum outro ajuste aplicado.
Persistência por operação ainda não iniciada; contratos finais pendentes.
[obc restaurado e cliente original aprovado](../examples/int009-completion-2026-11-18/09-obc-restored.json).

**Histórico anterior: fixtures aprovadas na etapa 40; aquisição conjunta com
estágio SD sob demanda reprovada por q=217 ms contra limite 200 ms.
Implementação interrompida conforme AGENTS.md.** Os comandos
abaixo reproduzem os perfis; a aprovação de fixtures não aprova aquisição
ou operação integrada. O diagnóstico está
na [INT-009](../.tasks/03-integracao/INT-009-aquisicao-eps-termico.md#recorte-conjunto-interrompido--17112026).

**Estágio sob demanda aprovado aplicado:** somente os perfis conjuntos
reservam o estágio SD pelo tamanho obtido de `GetSnapshotSize` durante
ENCODING, fora dos locks. A memória fica estável em READY/WRITING e é
liberada antes de FREE. Limite 64.497 bytes, formatos, filas e descarte
contado preservados. Perfis anteriores mantêm reserva máxima no boot.
Novo teste nativo de buffers e regressões originais de fluxo/partes
passaram com sanitizadores. Cinco builds e rebuild das bancadas passaram.

O [roteiro conjunto reprovou](../examples/int009-housekeeping-2026-11-17-183036/62-on-demand-acquisition-usb.json)
após iniciar todos os produtores e o escritor: `ping q=217 ms`; limite
200 ms intacto. O poll UART também atingiu 13 ms. Os
[49 registros capturados foram interpretados](../examples/int009-housekeeping-2026-11-17-183036/65-partial-captured-records.json)
em arquivos novos somente para diagnóstico, com CRC válido e sem bytes
descartados. Isso não aprova o roteiro original, cujo `passed:false`
permanece. [obc restaurado/verificado](../examples/int009-housekeeping-2026-11-17-183036/64-on-demand-obc-restored.json).
Na etapa anterior, buffer TX UART0 de 8 KiB era proposta ainda não aplicada. Históricos abaixo
conservam as causas anteriores e suas evidências.

Verificação do proprietário dos buffers, dentro de `obc-dh/`, com
ASan/UBSan/LeakSanitizer ativos e execução fora de ptrace. O GNU ld
intercepta o `new[]` sem exceção apenas no executável nativo Linux de
64 bits, sem trocar o allocator da aplicação:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcStorage.cpp src/ObcDataFlow.cpp tests/SnapshotBuffersCheck.cpp \
    -Wl,--wrap=_ZnamRKSt9nothrow_t -o /tmp/fcp01-snapshot-buffers-check
/tmp/fcp01-snapshot-buffers-check
```

**Pilhas estáticas aprovadas aplicadas:** o perfil de aquisição conjunta
usa pilhas próprias EPS/Térmico de 4 KiB com a API estática nativa do
FreeRTOS. Produtores iniciam e progridem; somente I2C pede pilha dinâmica
pendente. Cinco builds passaram. O
[roteiro de aquisição reprovou](../examples/int009-housekeeping-2026-11-17-183036/49-static-acquisition-usb.json):
32 TMs recebidas e zero registros, pois o bloco SD de 31.729 bytes não
coube no maior bloco livre de 26.612 bytes. Nenhuma outra correção foi
aplicada; [obc restaurado/verificado](../examples/int009-housekeeping-2026-11-17-183036/51-static-workers-obc-restored.json).
Reservar o estágio SD pelo tamanho real do snapshot é uma proposta ainda
não implementada. As seções seguintes preservam o histórico das tentativas.

**Ordem aprovada aplicada:** após reservar os quatro buffers e iniciar
TT&C, a bancada cria o fluxo antes de I2C/produtores. A admissão soma
4 KiB pendentes em fixtures ou 12 KiB em aquisição, além das pilhas do
fluxo e da margem original. O roteiro de fixtures passou sem mudanças:
[34 TMs e 64 registros lidos](../examples/int009-housekeeping-2026-11-17-183036/40-task-order-fixtures-usb.json).
A [aquisição conjunta reprovou](../examples/int009-housekeeping-2026-11-17-183036/42-task-order-acquisition-usb.json):
EPS iniciou e Térmico não, com maior bloco de 4.084 bytes e pilha de
4.096 bytes. Captura passiva confirmou I2C READY e EPS progredindo.
Nenhum novo ajuste foi feito; [obc restaurado/verificado](../examples/int009-housekeeping-2026-11-17-183036/45-task-order-obc-restored.json).
Os relatos anteriores abaixo são históricos. O coletor de diagnóstico
da etapa 31 pertence à imagem 29 e não acompanha a ordem atual dos
marcadores; os roteiros funcionais permanecem intactos.

**Diagnóstico posterior concluído:** `[HK-ALLOC]` registra memória antes e
depois de criar transporte/processamento/escritor. O callback do SDK
somente copia metadados da recusa, sem I/O ou alocação. A
[captura sem telecomandos/Payload](../examples/int009-housekeeping-2026-11-17-183036/31-allocation-diagnosis.json)
confirmou recusa da pilha de 8.192 bytes do escritor com heap de 26.804
bytes e maior bloco de 8.180 bytes. O diagnóstico não altera a ordem das
tarefas ou suas pilhas; não aprova o roteiro funcional. Cinco builds
passaram e [obc foi restaurado/verificado](../examples/int009-housekeeping-2026-11-17-183036/33-diagnostic-obc-restored.json).

O [coletor de diagnóstico](../examples/int009-housekeeping-2026-11-17-183036/CaptureAllocationDiagnostics.py)
recebe a porta e `--report ARQUIVO_NOVO`, lê no máximo 15 s e não envia
comandos. Exige os seis pontos e callback registrado; seu campo
`diagnosis_captured` indica coleta, enquanto `functional_acceptance`
permanece `not_run`. A correção da ordem/orçamento das tarefas ainda não
foi aplicada. As reprovações funcionais abaixo continuam preservadas.

**Continuação autorizada:** os quatro buffers grandes agora são
reservados pelo fluxo antes de I2C/TT&C/produtores. A admissão de memória
das tarefas permanece após esses serviços, com a mesma margem. Cinco
builds passaram; o mesmo roteiro voltou a reprovar, desta vez na criação
da tarefa escritora: 34 TMs, zero registros. A distribuição de memória
entre as criações de tarefas ainda precisa de investigação; implementação
novamente interrompida. [Segunda reprovação](../examples/int009-housekeeping-2026-11-17-183036/23-reserved-fixtures-usb.json)
e [obc novamente restaurado/verificado](../examples/int009-housekeeping-2026-11-17-183036/25-second-obc-restored.json)
preservados. Os resultados da primeira tentativa abaixo continuam históricos.

`int009-housekeeping-check` reutiliza a bancada de fluxo/TT&C com SD e
Payload simulados, inicializa I2C e inicia os produtores INA219/LM75A
existentes. `int009-housekeeping-fixtures-check` não inicia produtores;
substitui somente as cópias entregues ao mapeador por sete cenários de
cinco segundos, iniciados no primeiro pedido de TM: normal,
zero/corrente negativa, Bateria ausente, Solar ausente, temperatura #3
ausente, indisponibilidade total e retorno. Depois conserva o retorno.

O mapeador portátil usa VIN− provisório e converte A para mA sem inverter
o sinal. O JSON mantém `tp:null` quando qualquer temperatura está
indisponível; o TM v3 mantém disponibilidade individual. O modo permanece
`bench`, o envio sob demanda e Geral continua somente nos diagnósticos.
Não há proteção, atuação, aquisição operacional ou contrato final novo.

`[HK-CHECK]` identifica a origem e o instante de cada composição;
`[HK-HEALTH]` informa memória/pilha. `[HK-RECORD]` captura somente os bytes
de BOOT, referências e TM aceitos pelo backend simulado após a verificação
de estabilidade existente. Não é extração dos arquivos FCD1 ou prova de
durabilidade física. O roteiro novo gera `.records.bin`/`.records.jsonl`
ao lado do relatório, sem sobrescrever destinos, e usa o leitor real.

Verificação portátil, dentro de `obc-dh/`, com sanitizadores ativos e
execução fora de ptrace:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Iheaders \
    src/ObcHousekeeping.cpp src/ObcData.cpp src/ObcTime.cpp \
    src/ObcRecordCodec.cpp src/ObcEps.cpp src/ObcThermal.cpp \
    tests/ObcHousekeepingCheck.cpp -o /tmp/fcp01-housekeeping-check
/tmp/fcp01-housekeeping-check
pio run -e obc -e int009-ina219-check -e int009-lm75a-check \
    -e int009-housekeeping-check -e int009-housekeeping-fixtures-check
```

O leitor é compilado pelo procedimento [Primeira etapa INT-006](#primeira-etapa-int-006-fluxo-tm-v3-e-snapshot-partido).
Com Python contendo pyserial, porta identificada e destinos novos:

```bash
pio run -e int009-housekeeping-fixtures-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcHousekeepingBenchCheck.py /dev/ttyUSB0 --source fixtures \
    --reader /tmp/fcp01-read-records --report /tmp/housekeeping-fixtures-novo.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStationCheck.py /dev/ttyUSB0 --report /tmp/housekeeping-obc-novo.json
```

O perfil sem sensores usa o outro ambiente e `--source acquisition`; sua
execução USB permanece pendente. A reprovação de fixtures ocorreu antes
da captura: um dos blocos de snapshot não foi alocado e o escritor não
iniciou, mantendo os limites existentes. A composição por USB produziu
33 TMs, mas nenhum BOOT/referência/TM foi escrito; o roteiro preservou
`passed:false`. Os dois arquivos de registros dessa tentativa estão vazios.

[Evidências preservadas](../examples/int009-housekeeping-2026-11-17-183036/manifest.json):
nativos novos e anteriores aprovados, 25 casos TT&C, cinco builds,
primeira reprovação USB e `obc` restaurado/verificado pelo cliente original.
As bancadas anteriores e suas entradas/expectativas foram preservadas.
O recorte e a INT-009 permanecem abertos; nenhuma TES foi aprovada.

## LM75A de bancada — 17/11/2026

`int009-lm75a-check` habilita somente a aquisição dos cinco canais térmicos
por `OBC_THERMAL_BENCH`, reutilizando boot, executor I2C e TT&C existentes.
Não habilita INA219, publicação térmica na TM, gravação SD ou proteção.
`obc` continua padrão e não inicia aquisição EPS/Térmico.

Referência provisória aprovada para desenvolvimento:
[NXP LM75A, LM75A_4, revisão 04 de 10/07/2007](https://www.nxp.com/docs/en/data-sheet/LM75A.pdf).
O fabricante dos módulos reais continua sujeito à confirmação de Térmico.
O [driver local](../../eps-tc/LM75A_Standard/README.md) lê Conf como um byte,
preserva seus bits OS, limpa SHUTDOWN e mantém os bits reservados zerados;
não altera Tos/Thyst. Temp é lido em dois bytes e convertido fora do executor:
11 bits com sinal, resolução de 0,125 °C, cinco LSB ignorados e faixa válida
de −55 a +125 °C. Leitura de registrador pode reconhecer OS em modo interrupt;
essa saída não é usada pela bancada.

### Parâmetros e disponibilidade

| Item | Valor provisório |
|---|---|
| Canais / endereços de sete bits | thermal_1 a thermal_5 / 0x48 a 0x4C |
| Leitura / espera inicial após configuração | 1.000 ms / 1.000 ms |
| Nova tentativa offline / amostra antiga | 5.000 ms / idade superior a 3.000 ms |
| Pedido ao executor | Prioridade 0, espera de fila 100 ms |
| Tarefa / rodada | Core 1, prioridade 1, pilha inicial 4.096 bytes / 10 ms |
| Diagnóstico USB | Uma rodada por segundo |

O datasheet informa conversão típica de 100 ms, sem máximo nessa
especificação. A espera inicial não comprova tempo máximo físico.
Endereços e nomes não definem A0–A2, posição térmica ou mapa de voo.
EPS/Térmico e OBC definirão os parâmetros operacionais; TES-003/008/009
mantêm a comprovação física correspondente.

Cada canal preserva a última leitura válida, mas falha, idade excessiva,
serviço indisponível ou geração I2C diferente tornam a amostra indisponível.
`temperature_C` mostra somente leitura disponível; `last_valid_C` identifica
histórico. Ambos são `null` antes da primeira leitura válida. Não interpretar
zero de uma estrutura ainda sem validade como medição.

`[THERMAL]` registra endereço, disponibilidade, instantes e diagnóstico do
driver/Wire/executor. `[THERMAL-HEALTH]` registra primeira rodada publicada,
progresso, pedidos concluídos, falhas, serviço e margem de pilha.
Metadados precedem a criação da tarefa; `starting` não simula operação.
O getter revalida a geração de recuperação, sem I/O ou logs sob lock.
Não há nova inscrição no watchdog ou política de proteção térmica.

### Verificação nativa e USB

Dentro de `obc-dh/`, com compilador C++ e Python contendo pyserial:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Itests/thermal_native -Iheaders \
    -I../eps-tc/LM75A_Standard/src src/ObcThermal.cpp src/ObcEps.cpp \
    ../eps-tc/LM75A_Standard/src/Lm75a.cpp tests/ObcThermalCheck.cpp \
    -o /tmp/fcp01-lm75a-check
/tmp/fcp01-lm75a-check
pio run -e obc -e int009-ina219-check -e int009-lm75a-check
pio run -e int009-lm75a-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcThermalBenchCheck.py /dev/ttyUSB0 --report /tmp/lm75a-usb-novo.json
pio run -e int009-ina219-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcEpsBenchCheck.py /dev/ttyUSB0 --report /tmp/ina219-regressao-novo.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStationCheck.py /dev/ttyUSB0 --report /tmp/lm75a-obc-novo.json
```

Usar a porta identificada e destinos novos. ASan/LeakSanitizer exigem execução
fora de ptrace neste ambiente; não desativar sanitizadores. O teste nativo
usa o driver real com registradores exclusivos em `tests/thermal_native/`;
os stubs e vetores INA219 anteriores permanecem intactos.

A bancada exige ESP32 livre de ligações externas, somente USB. Durante
30 s, o roteiro verifica cinco canais indisponíveis, valores `null`, pelo
menos cinco tentativas por canal, intervalo offline mínimo de 5.000 ms,
progresso e atendimento `ping`/TM. Conserva os limites de 200 ms/2 s/255 bytes.
As leituras positivas e os erros de registradores são exercitados no
computador; não há sensor positivo simulado no firmware.

Restaurar e verificar `obc` também após falha. O roteiro não grava firmware
nem altera critérios anteriores. Relatórios exclusivos, inclusive falhas,
e os resultados de 17/11 estão na
[INT-009](../.tasks/03-integracao/INT-009-aquisicao-eps-termico.md).
O core dump inválido já diagnosticado na INT-004 permanece preservado; não
é reparado ou apagado por este procedimento.

## INA219 de bancada — 16/11/2026

**Situação atual:** recorte INA219 de bancada concluído. Com o ESP32 livre
de ligações externas, o mesmo firmware/roteiro da execução 02 passou na
[execução USB 03](../examples/obc-eps-2026-11-16-usb-03.json), sem alterar
código ou critérios: 30,039 s, 54 ACKs, 27 TMs, `q` máximo 94 ms e resposta
máxima 171,718 ms. Foram observadas 32 amostras de saúde com serviço READY;
menor margem de pilha EPS de 2.656 bytes e novas tentativas entre 5.002 e
5.011 ms. A falha SCL_LOW não se repetiu nessa condição; a ligação externa
responsável não foi identificada. Reprovações anteriores permanecem abaixo.

O [obc restaurado 03](../examples/obc-eps-2026-11-16-restored-03.json) passou
no roteiro original: seis ACKs, `q` máximo 10 ms e resposta máxima 97,340 ms.
[Manifesto final](../examples/obc-eps-2026-11-16-usb-03-manifest.json) conserva
hashes e comparação das versões. A INT-009 permanece aberta por contratos,
TM operacional e LM75A; sensores reais e montagem integrada não foram
aprovados por essa bancada.

`int009-ina219-check` reutiliza o boot central e habilita a aquisição dos três
INA219 somente por `OBC_EPS_BENCH`. `obc` continua padrão e não inicia essa
aquisição. Os valores permanecem em RAM/USB; TM operacional e SD não recebem
campos EPS nesta etapa. LM75A e proteções não estão implementados por este
recorte. Estado e limitações na [INT-009](../.tasks/03-integracao/INT-009-aquisicao-eps-termico.md).

O driver usa `CEFAST::eps`; o namespace anterior `CEFAST::EPS` colidia com a
macro Xtensa `EPS=192`. Consumidores precisam atualizar o nome. Calibração,
cálculos e funções existentes permanecem; o framework não foi modificado.
`begin(wire, channel)` configura um sensor e `GetI2cDiagnostic()` conserva
etapa, registrador, código de Wire e quantidade recebida. Leitura curta é
`READ_INCOMPLETE`, sem inventar NACK ou timeout não exposto por `requestFrom`.

A tarefa EPS (Core 1/prioridade 1/pilha inicial de 4 KiB) é proprietária do
driver; I/O ocorre exclusivamente no executor I2C, sem logs ou espera de
conversão nos callbacks. Configuração e aquisição são solicitações separadas.
As cópias protegidas não contêm I/O. Falha de um canal não invalida os demais;
mudança do ciclo de recuperação exige nova configuração antes de outra leitura.

Parâmetros provisórios: endereços Solar/Bateria/Geral `0x40`/`0x41`/`0x44`,
aquisição a cada 1.000 ms, tentativa offline a cada 5.000 ms, amostra antiga
após 3.000 ms, conversão após 2 ms, prioridade de pedido 0 e fila de até
100 ms. Não são aprovações elétricas ou limites de proteção. Corrente em A
conserva VIN+ para VIN− positiva; as tensões VIN− e VIN+ são identificadas
separadamente, sem escolher seu mapeamento operacional.

`[EPS]` informa canal, disponibilidade, idade, último resultado e valores;
sem amostra válida, as medições são `null`. Histórico conservado após falha
não é leitura disponível. `[EPS-HEALTH]` informa progresso, resultados e
margem de pilha; não acrescenta reset automático por falha do sensor.

Verificação nativa dentro de `obc-dh/`, com ASan/LeakSanitizer fora de ptrace:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -Itests/eps_native -Iheaders -I../eps-tc/EPS_INA219_Standard/src \
    src/ObcEps.cpp ../eps-tc/EPS_INA219_Standard/src/EPS_INA219.cpp \
    tests/ObcEpsCheck.cpp -o /tmp/fcp01-eps-check
/tmp/fcp01-eps-check
g++ -std=c++11 -Wall -Wextra -Werror -DEPS=192 \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -Itests/eps_native -Iheaders -I../eps-tc/EPS_INA219_Standard/src \
    src/ObcEps.cpp ../eps-tc/EPS_INA219_Standard/src/EPS_INA219.cpp \
    tests/ObcEpsCheck.cpp -o /tmp/fcp01-eps-macro-check
/tmp/fcp01-eps-macro-check
pio run -e obc -e int009-ina219-check
```

Ambos os testes EPS, regressões existentes de boot/escalonamento/saúde I2C/
watchdog e os dois builds passaram. Comandos, saídas e hashes estão no
[relatório local versionado](../examples/obc-eps-2026-11-16-native.json).
O nome `native` identifica a comprovação dos testes; o relatório também
conserva metadados dos builds e registra explicitamente USB não executado.

Bancada de desenvolvimento, somente ESP32/USB, sem sensores ou outros
periféricos externos. Use um Python com pyserial; nesta máquina está no
ambiente do PlatformIO (`~/.local/share/uv/tools/platformio/bin/python`).
Substitua `/dev/ttyUSB0` pela porta identificada e use relatórios novos:

```bash
pio run -e int009-ina219-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcEpsBenchCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-eps-usb-novo.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-eps-normal-novo.json
```

O roteiro observa 30 s: executor pronto, aquisição ativa, três canais
indisponíveis com medições `null`, cadência de nova tentativa de pelo menos
5.000 ms, progresso contínuo e TC/TM com entrega ≤200 ms e resposta ≤2 s.
Registra a margem de pilha, sem apresentá-la como comprovação de pior caso.
Relatório é exclusivo e conserva falhas. Restaurar e verificar `obc` também
se o roteiro falhar; ele não realiza upload/restauração automaticamente.

Na etapa anterior não havia porta ESP32 disponível: nenhum upload, execução
USB ou restauração física foi realizado. Sintaxe e `--help` do roteiro
foram verificados com o Python do PlatformIO/pyserial 3.5. O Python global
não possui pyserial instalado. O recorte continua aberto até execução USB;
isso não altera ensaios, critérios ou estados das TES.

Após conexão da placa, a [primeira execução USB](../examples/obc-eps-2026-11-16-usb.json)
reprovou durante a coleta inicial: primeiro canal `SOLAR` com `address=0x00`
em vez de `0x40`. O diagnóstico pode executar antes da primeira publicação
do worker e ler o snapshot compartilhado zerado. Não é aceite do endereço
zero ou motivo para alterar o roteiro. Naquela execução, a correção de
inicialização/publicação aguardava discussão; nenhum código ou critério
foi alterado durante a coleta reprovada.
O [firmware normal restaurado](../examples/obc-eps-2026-11-16-restored.json)
passou no roteiro original: seis ACKs, `q` máximo 11 ms e resposta máxima
67,133 ms. [Manifesto USB](../examples/obc-eps-2026-11-16-usb-manifest.json)
conserva placa, hashes e resultados. O recorte continua aberto.

O ajuste de publicação foi autorizado e implementado: metadados existem
antes da criação da tarefa e o diagnóstico informa `starting` até a
primeira rodada publicada. A [segunda execução, com o mesmo roteiro](../examples/obc-eps-2026-11-16-usb-02.json),
superou a falha de endereço zero, mas reprovou em `service=3` (UNAVAILABLE).
Primeira configuração retornou timeout (`Wire status=5`); o executor
registrou recuperação esgotada com `cause=5 failure=5` (SCL_LOW).
A causa elétrica ainda precisava de confirmação nessa etapa. Não alterar expectativas,
polarização ou recuperação sem diagnóstico e discussão com o responsável.
O [obc restaurado 02](../examples/obc-eps-2026-11-16-restored-02.json) passou
no roteiro original: seis ACKs, `q` máximo 31 ms e resposta máxima 72,826 ms.
O [manifesto 02](../examples/obc-eps-2026-11-16-usb-02-manifest.json) registra
hashes e a preservação dos roteiros. O recorte permanece aberto.

## Watchdog e reset OBC — 15/11/2026

A INT-012 usa o watchdog de tarefas do SDK instalado (IDF 4.x, Arduino
ESP32 2.0.14), com timeout de 5 s e panic/reboot. Inscreve manualmente a
tarefa loop, sem `enableLoopWDT`, sem remover IDLE0 e sem alterar o watchdog
de interrupção. Arma depois de iniciar a serial e alimenta entre etapas
concluídas do setup; chamada de inicializador bloqueada não alimenta.

No runtime, a cada 100 ms como alvo, consulta cópias dos proprietários e
alimenta somente se nenhum recurso criado exceder os seguintes limites.
Tarefa inativa não bloqueia; espera/ociosidade legítima continua publicando
progresso. Erro retornado, `NO_MEMORY`, rádio/SD ausente ou silêncio não
significam travamento. Se o progresso retorna antes do disparo, a alimentação
é retomada. Loop/coleta bloqueado também deixa de alimentar.

| Recurso | Limite de ação, estritamente maior que |
|---|---:|
| UART | 500 ms |
| Processamento | 5.000 ms |
| Escritor, por operação | 10.000 ms |
| Executor I2C | 2.000 ms |
| TT&C | 5.000 ms |

O executor I2C desperta ocioso a cada 1.000 ms e publica progresso antes/
depois do atendimento e recuperação. Não executa transações extras, muda
prioridades ou cancela callbacks. Diagnósticos anteriores mantêm seus
limites. EVENT/FAULT 265 identifica suspensão/retomada por recurso.

`reset_obc` agenda um `esp_timer` one-shot de 1.500.000 us, cujo callback
executa `esp_restart()` no serviço nativo de timers. O prazo começa no
agendamento bem-sucedido, antes de registro/ACK. Repetição não o adia. ACK
recusado ou consumidor bloqueado não cancela o reset. Sem timer/agendamento,
retorna `state_rejected`; argumentos diferentes de ausente/null/{} retornam
`invalid_param`. Não espera SD, transmissão, confirmação ou filas vazias.
EVENT 266 é tentativa de registro, não confirmação de persistência.

Falhas ao armar watchdog ou criar timer são explícitas e independentes;
execução degradada não satisfaz o aceite da INT. Reutiliza NVS/boot_count e
causa de reset do BOOT; serial também mostra `reset_reason`. Não apaga NVS,
fabrica boot ou restaura UTC de outro boot. Nenhuma garantia de saída elétrica
segura, watchdog externo, partição alternativa ou retomada de missão.

Todos os valores são provisórios de desenvolvimento, acordados pelo
responsável OBC-DH para esta versão. Valores/política finais pertencem a
OBC-DH, coordenação e setores afetados na INT-013; comprovação independente
na TES-009. Tempo nominal até watchdog inclui limite mais 5 s, avaliação,
panic e boot; não equivale a prazo certificado ou watchdog externo do DP.

### Executar as bancadas

Somente ESP32/USB, sem cartão/Pi/rádio. O fluxo/TT&C e FreeRTOS são reais;
SD/Payload são simulados. Injeções são finitas e selecionadas na compilação,
sem comando novo no firmware normal. Cookie RTC exclusivo de bancada,
validado por caso/causa, impede repetir a injeção após o reset esperado.
O cenário I2C inicializa seu executor e pode deixar escritor inativo;
demais casos conservam os buffers originais do fluxo sem esse executor.
O firmware normal conserva a sequência NVS–I2C–SD–TT&C–fluxo.

| Caso | Ambiente | Verificação |
|---|---|---|
| 0 | `int012-watchdog-check` | Argumentos, reset, repetição e deduplicação |
| 1 | `int012-uart-check` | UART sem progresso |
| 2 | `int012-process-check` | Processamento sem progresso |
| 3 | `int012-writer-check` | Escritor sem retorno da operação |
| 4 | `int012-i2c-check` | Callback I2C sem retorno |
| 5 | `int012-ttc-check` | TT&C sem retorno do transporte |
| 6 | `int012-supervisor-check` | Loop sem progresso |
| 7 | `int012-startup-check` | Inicializador sem retorno após reserva NVS |
| 8 | `int012-resume-check` | UART retorna antes do disparo; eventos 265 |
| 9 | `int012-ack-full-check` | Quatro vagas reais de ACK ocupadas |
| 10 | `int012-reset-blocked-check` | Consumidor bloqueia depois do agendamento |
| 11 | `int012-arm-error-check` | Falha de armamento explícita; timer independente |
| 12 | `int012-timer-error-check` | Timer indisponível rejeita reset |
| 13 | `int012-schedule-error-check` | Falha ao agendar não declara reset pendente |

Selecionar ambiente e número correspondente. O roteiro reinicia por RTS;
usar relatório novo, nunca sobrescrever evidência:

```bash
pio run -e int012-uart-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcWatchdogBenchCheck.py /dev/ttyUSB0 --case 1 \
    --report /tmp/int012-uart-novo.json
pio run -e int012-memory-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcLimitsCheck.py /dev/ttyUSB0 --case memory \
    --report /tmp/int012-memory-novo.json
pio run -e int012-waits-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcTaskHealthBenchCheck.py /dev/ttyUSB0 --bench \
    --report /tmp/int012-waits-novo.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcWatchdogBenchCheck.py /dev/ttyUSB0 --production \
    --report /tmp/int012-obc-novo.json
python3 tests/ObcResourcesBenchCheck.py /dev/ttyUSB0 \
    --report /tmp/int012-obc-resources-novo.json
```

Restaurar e verificar `obc` também depois de falha. Verificações antigas
de recursos/I2C/escritor/downlink foram executadas nos seus ambientes
compatíveis, sem modificar scripts, vetores ou resultados esperados.

Verificação portátil da decisão, executando ASan/LeakSanitizer fora de ptrace:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Iheaders \
    src/ObcWatchdog.cpp tests/ObcWatchdogCheck.cpp -o /tmp/obc-watchdog-check
/tmp/obc-watchdog-check
```

Os 14 cenários novos passaram. Timer medido em 1.500 ms nos quatro casos
de bancada que reiniciam por comando; tolerância de desenvolvimento do
roteiro é ±100 ms, retorno do boot entre 1.400 e 3.500 ms após agendamento.
Pausa até novo boot: UART 5.911 ms, processamento 10.470 ms, escritor
15.535 ms, I2C 7.554 ms, TT&C 10.542 ms, supervisor 5.482 ms e setup
5.441 ms. São observações desta placa/versão, incluindo panic e boot.
Pausas cedem CPU: comprovaram `ESP_RST_TASK_WDT` (6); comando comprovou
`ESP_RST_SW` (3). Travamento de interrupção pode acionar outra proteção antes.
Perfil de memória e esperas passou com watchdog armado e critérios originais.

A reprovação inicial ocorreu antes de enviar TC: TT&C reabriu a serial
antes de concluir marcador e a criação antecipada de I2C fragmentou o heap
do escritor simulado. Após diagnóstico e autorização, somente a bancada
passou a drenar o marcador e criar I2C no caso dedicado. Buffers, sequência
de produção e as 48 validações preexistentes ficaram intactos.

Evidências, hashes e casos liberados na
[INT-012](../.tasks/03-integracao/done/INT-012-watchdog-reset-obc.md) e no
[manifesto](../examples/obc-watchdog-2026-11-15-manifest.json).
TES-009 recebe mecanismo/roteiro para comprovação independente; TES-008
conserva verificação elétrica e INT-013 conserva retomada de voo.

## Supervisão de recursos TT&C e memória — 15/11/2026

`Ttc::GetDiagnostics()` fornece uma cópia protegida de inicialização,
tarefa criada, início/conclusão de `Update`, ciclos concluídos, última
disponibilidade observada do rádio, tentativas e contato válido RF/cabo.
Mede a margem histórica da pilha fora do lock. Não acessa rádio/UART nem
deduz progresso pela chegada de TCs. `Update` manual não fabrica uma tarefa.
Os contatos possuem indicador de existência: millis zero é válido.

O consumidor OBC encaminha a cópia a cada 100 ms como alvo, inclusive se o
fluxo Payload estiver inativo. `ObcResourceHealth` avalia progresso TT&C
separado da disponibilidade do rádio. Idade >2.000 ms produz suspect;
novo avanço retorna a ok. Idade é modular u32: a primeira detecção deve
ocorrer antes de um wrap completo; suspeita detectada persiste até avanço.
O diagnóstico depende de o consumidor/loop e seus locks continuarem
executando; não substitui watchdog global.

Memória é avaliada a cada segundo. Os valores abaixo foram definidos pelo
responsável para esta implementação, sem alterar aceitação de mensagens:

| Métrica | Entrada | Retorno |
|---|---|---|
| Heap interno acessível por byte | Livre <8.192 bytes | Livre >=12.288 bytes |
| Maior bloco desse heap | <4.096 bytes | >=6.144 bytes |
| Margem histórica de pilha | <1.024 bytes por tarefa ativa | Alerta permanece durante a vida da tarefa |

As métricas de heap usam `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`.
O mínimo histórico de heap é informação, não condição de retorno. UART,
processamento, escritor, executor I2C, TT&C e loop têm pilhas identificadas
separadamente; tarefa ausente não é margem zero. O processamento publica
recusas `NO_MEMORY` cumulativas/saturadas, sem depender do heap da próxima
amostra. Rejeição conserva o snapshot anterior e a possibilidade de retomada.

`[TTC-HEALTH]` separa progresso, rádio, prontidão cabeada, contato/idade por
meio e contagem de tentativas. Silêncio não declara perda de enlace.
`[MEMORY-HEALTH]` publica pressões, pilhas e recusas; os prefixos antigos
permanecem. Transições usam EVENT/FAULT OBC 260–264, conforme o
[catálogo](../../common/protocol/obc-data.md#event-e-fault). Sem boot válido,
somente USB; fila/SD indisponíveis conservam perdas explícitas. Não existe
reset por memória baixa ou recuperação adicional fora do proprietário.

Verificação portátil, dentro de `obc-dh/`, com os sanitizadores ativos fora
de ambiente com ptrace:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcResourceHealth.cpp tests/ObcResourceHealthCheck.cpp \
    -o /tmp/fcp01-resource-health-check
/tmp/fcp01-resource-health-check
```

Na pasta `tt-c/`, as suítes originais continuam intactas. A suíte nova usa
a API real, meios falsos e tarefa real no último caso:

```bash
pio test -e native
pio test -e esp32 --upload-port /dev/ttyUSB0 --test-port /dev/ttyUSB0
pio test -e esp32-diagnostics --upload-port /dev/ttyUSB0 --test-port /dev/ttyUSB0
```

Na pasta `obc-dh/`, com somente ESP32/USB e Python contendo pyserial:

```bash
pio run -e int008-resources-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcResourcesBenchCheck.py /dev/ttyUSB0 --bench \
    --report /tmp/obc-resources-bench-novo.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcResourcesBenchCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-resources-normal-novo.json
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-resources-downlink-novo.json
```

Usar nomes novos: os roteiros recusam sobrescrever relatórios. Cada upload
substitui o firmware; restaurar e verificar `obc` também após falha.
Selecionar a porta identificada na máquina, não presumir `/dev/ttyUSB0`.

`int008-resources-check` reutiliza o fluxo e a tarefa TT&C reais. Seu rádio
falso injeta TC RF, pausa finita de pelo menos 2.300 ms fora dos locks e
indisponibilidade/retorno pelo mecanismo existente. Memória/pilhas são
substituídas somente na cópia da bancada: pressão, banda de histerese,
retorno, margem histórica e uma recusa entre amostras. Não exaure heap ou
pilha físicos. A bancada de memória anterior continua responsável pela
recusa real do parser e preservação/retomada do snapshot.

O roteiro exige EVENT/FAULT 260–264 confirmados pelo backend SD simulado e
ausência de reboot. A pausa deliberada TT&C impede atendimento normal:
seu caso mede detecção/retorno; TC q<=200 ms, ACK<=2 s e quadros<=255 bytes
são verificados antes/depois. Roteiros históricos de cadência normal não
devem ser aplicados à fase de pausa desta bancada.

### Entrega ao setor de testes

| Caso disponível | Ferramenta | Limitação / condição |
|---|---|---|
| Progresso TT&C, rádio, memória/pilhas e eventos | `int008-resources-check` + `ObcResourcesBenchCheck.py --bench` | ESP32/USB; rádio, amostras de memória e SD simulados |
| Diagnóstico normal sem periféricos | `obc` + `ObcResourcesBenchCheck.py` | Rádio indisponível é esperado; USB/progresso ativos, escritor inativo |
| Progresso UART/processamento/escritor | `int008-health-check` + `ObcTaskHealthBenchCheck.py --bench` | Pausas/SD simulados; ver seção própria |
| Diagnóstico/recuperação I2C | `int008-i2c-health-check` + `ObcI2cHealthBenchCheck.py --bench` | Controlador/GPIOs simulados; físico exige montagem e injeção acordadas |
| Retomada limitada do escritor | `int008-storage-check` + `ObcStorageRecoveryBenchCheck.py` | Backend simulado; cartão/FAT32/alimentação são ensaios independentes |

O setor de testes executa os casos, registra versão, montagem, entradas,
esperado/observado e impedimentos. Os registros locais OBC não aprovam os
ensaios por inferência. TES-009 mantém watchdog/reset dependentes de
INT-012, retomada de missão/atuações de INT-013/011 e condições físicas de
I2C/UART/SD. Nenhum checklist ou limite de aceitação foi alterado.

## Diagnóstico I2C — 14/11/2026

`ObcI2cHealth` avalia os diagnósticos do executor no loop existente, com
período alvo de 100 ms. Callback acima de 100 ms ou recuperação acima de
200 ms fica suspect; igualdade não dispara. Serviço ocioso sem sensores
não vence. `service` distingue uninitialized/ready/recovering/unavailable;
`state=ok` significa ausência de atraso, não disponibilidade do serviço.

`[I2C-HEALTH]` publica estado, serviço, atividade, idade, limite, instante
da amostra u32, tempo monotônico u64, changed, fila, causas e contadores.
Transições observadas são imediatas na avaliação; amostras periódicas saem
a cada segundo. O diagnóstico USB continua mesmo sem boot válido.
EVENT/FAULT OBC 259 usa o layout existente de 96 bytes e exige boot válido;
fila/SD podem perder registros, com contagem explícita. A primeira amostra
normal não gera evento, e callback/ociosidade normais não geram um evento
por operação. Recuperações curtas entre amostras podem passar sem transição.

Não há cancelamento, nova tarefa em produção, reset ou pedido automático
de recuperação. APIs, contexto dos callbacks, 20 ms por transferência,
duas tentativas e orçamento cooperativo de 100 ms permanecem. O diagnóstico
depende do loop/escalonamento/mutex funcionando; não é watchdog global.

Verificação portátil, dentro de `obc-dh/`, com sanitizadores ativos e
execução fora de ambiente com ptrace:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcI2cHealth.cpp tests/ObcI2cHealthCheck.cpp \
    -o /tmp/fcp01-i2c-health-check
/tmp/fcp01-i2c-health-check
```

O teste cobre limites, primeira amostra, ociosidade, wrap u32, suspeita sem
repetição e mantida após detecção, retorno, indisponibilidade e EVENT/FAULT
codificados/decodificados pelo codec real. Não desativar LeakSanitizer.

Bancada e regressões somente com ESP32/USB, sem sensores, cartão, Pi, rádio
ou jumpers. Usar Python com pyserial e nomes novos para os relatórios:

```bash
pio run -e int008-i2c-health-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcI2cHealthBenchCheck.py /dev/ttyUSB0 --bench \
    --report /tmp/obc-i2c-health-bench.json
pio run -e i2c-recovery-check -t upload --upload-port /dev/ttyUSB0
python3 tests/I2cRecoveryBenchCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-i2c-health-recovery.json
pio run -e int008-health-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcTaskHealthBenchCheck.py /dev/ttyUSB0 --bench \
    --report /tmp/obc-i2c-health-tasks.json
pio run -e int006-ttc-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0 --bench \
    --report /tmp/obc-i2c-health-downlink.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcI2cHealthBenchCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-i2c-health-normal.json
python3 tests/ObcTaskHealthBenchCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-i2c-health-tasks-normal.json
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-i2c-health-downlink-normal.json
```

A bancada nova reutiliza o executor FreeRTOS, TT&C e fluxo existentes.
Somente nela, um cliente adicional mantém callback por pelo menos 350 ms
e chamada de desativação por pelo menos 450 ms, fora dos locks. A espera
consulta o prazo em micros e cede CPU; não depende apenas da quantidade de
ticks. Controlador/GPIOs e SD/Payload são simulados. Verifica quatro ciclos,
dois recuperados e dois esgotados, contexto preservado além do prazo de
fila, ausência de repetição automática e retomada por pedido explícito.
Os roteiros históricos e suas entradas/expectativas permanecem intactos.

Recorte aprovado em 14/11: suspeitas aos 170/279 ms, 25 ACKs, maior resposta
de 91,924 ms, sem falso alerta na ociosidade/recuperação normal. Regressões
nativas e USB originais passaram. `obc` restaurado e verificado nos três
roteiros; restaurar também após eventual falha. A reprovação inicial da
pausa, os dois ajustes autorizados e todos os relatórios estão na
[INT-008](../.tasks/03-integracao/done/INT-008-supervisao-e-retomada.md).

## Supervisão diagnóstica das tarefas — 14/11/2026

`ObcTaskHealth` avalia progresso de UART, processamento e escritor. Os
proprietários publicam atividade e tempo de avanço; o loop existente avalia
as cópias com período alvo de 100 ms. Suspeita exige idade maior que 100 ms
na UART, 2.000 ms no processamento ou 5.000 ms no escritor. Esses limites
diagnósticos não substituem o critério UART de 10 ms ou os testes existentes.

`[HEALTH]` informa task/state/activity/age_ms/limit_ms/completions/sampled_ms.
Mudanças para suspect/ok são emitidas imediatamente na avaliação; amostras
periódicas acompanham os diagnósticos de um segundo. O campo changed separa
transições de amostras. Tarefas não criadas ficam inactive. Rodadas sem dados
e retornos de erro também comprovam progresso; ausência de aeronaves não
indica tarefa travada. Na varredura, cada leitura/listagem renova o progresso.
A espera entre tentativas SD é recovery_wait, sem esconder operações de
montagem que não retornam. Não há tarefa supervisora nova, reset ou exclusão
de tarefas. A avaliação depende de o loop/escalonamento/locks progredirem.

Transições usam EVENT/FAULT v1 de 96 bytes, origem OBC e código 256/UART,
257/processamento ou 258/escritor. O evento recebe boot e tempo monotônico
da avaliação. A fila existente pode recusar o registro ou o SD pode falhar;
as perdas continuam contabilizadas, sem prometer persistência ou repetir
eventos a cada amostra. TT&C/FCD1 e os arquivos anteriores não mudam.

Verificação nativa, dentro de `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcTaskHealth.cpp tests/ObcTaskHealthCheck.cpp \
    -o /tmp/fcp01-task-health-check
/tmp/fcp01-task-health-check
```

O teste passou com limites/fronteiras, zero válido, publicação posterior à
amostra, tarefas ausentes/nunca executadas, espera/varredura com progresso,
retorno, saturação do contador e eventos reais codificados/decodificados.
Executar LeakSanitizer sem ptrace, sem desativar sanitizadores.

Bancada com somente ESP32/USB, sem cartão, Pi, rádio ou jumpers:

```bash
pio run -e int008-health-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcTaskHealthBenchCheck.py /dev/ttyUSB0 --bench \
    --report /tmp/obc-health-bench.json
pio run -e int006-ttc-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0 --bench \
    --report /tmp/obc-health-regression.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcTaskHealthBenchCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-health-normal.json
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-health-downlink-normal.json
```

Usar Python com pyserial e nomes novos; o roteiro novo recusa relatório
existente. `int008-health-check` reutiliza os vetores antigos e acrescenta
somente injeções exclusivas dessa bancada: UART 300 ms, processamento
2.500 ms e escrita 5.500 ms. Essas pausas violam deliberadamente progresso;
não aplicar o roteiro de cadência normal a esse ambiente. O roteiro próprio
exige uma suspeita/retorno por tarefa, varredura maior que 6 s com 32 operações
progredindo sem falso alerta, espera real de recuperação SD e comandos ativos.
Sem --bench, exige UART/processamento ok e escritor inactive sem periféricos.

Os cinco builds obc/uart-receiver-check/int006-ttc-check/int008-storage-check/
int008-health-check passaram. Execução USB aprovada em 14/11: um par
suspect/ok por tarefa, varredura de 6.465 ms e espera SD de 5.581 ms sem
falso alerta. Na bancada nova, 111 ACKs e maior resposta de 166,315 ms.
A regressão original aprovou 44 ACKs e três arquivos com CRC válido.
`obc` restaurado e aprovado pelos dois roteiros, sem SD/Pi; nenhum falso
alerta e `no_sd` esperado. Recorte local concluído, sem comprovar periféricos
físicos, supervisão I2C/TT&C ou watchdog global. Restaurar e verificar obc
após qualquer execução, inclusive em falha.

Estado e evidências na [INT-008](../.tasks/03-integracao/done/INT-008-supervisao-e-retomada.md).

## Retomada do escritor SD — 13/11/2026

**Ajuste autorizado da reserva:** a revisão encontrou bloqueio ao reconsultar
o SD com 32 entregas não persistidas e uma pendência antiga fora do cache.
Após discussão com o responsável, uma posição fixa exclusiva de recuperação
passou a atender esse caso. Preserva as 32 admissões e suas cópias; a posição
extra não admite dados novos do Payload. `occupied`/`high_water` mantêm seu
significado, e `recovery_slot` indica a ocupação adicional. Não muda FCD1/TT&C.

O escritor existente tenta recuperar uma falha terminal sem reiniciar o ESP32:
até três ciclos End/Init/novo segmento por episódio, com 5 s de espera antes
de cada tentativa. A espera começa após o retorno do ciclo anterior; operações
de montagem continuam síncronas e não têm prazo físico comprovado. Somente
um registro normal confirmado renova o orçamento. BOOT, referência temporal e
eventos próprios de recuperação não bastam. Após esgotamento, a gravação fica
indisponível até reinicialização manual; UART e TT&C continuam ativos.

Não há hotplug se o SD já estava ausente no boot, reparo de FAT32, exclusão de
arquivos ou repetição automática do registro incerto. Arquivos e caudas parciais
são preservados. A retomada usa nome exclusivo, mesmo boot, contadores
cumulativos e referência temporal vigente em RAM. O escritor continua sendo
o único proprietário de I/O. As bancadas históricas não fornecem callbacks
de remontagem e conservam suas entradas e expectativas.

Verificação portátil, dentro de `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcStorage.cpp src/ObcDataFlow.cpp src/ObcDelivery.cpp \
    tests/ObcStorageRecoveryCheck.cpp -o /tmp/fcp01-storage-recovery-check
/tmp/fcp01-storage-recovery-check
```

O teste usa arquivos temporários POSIX, tempo explícito e falhas de montagem,
abertura, metadados e escrita, incluindo cauda parcial. Confere intervalos,
renovação somente por dados, esgotamento, colisões sem sobrescrita, referência
do mesmo boot, contadores e reconsulta sem alterar IDs em trânsito. Executar
ASan/LeakSanitizer fora de ambientes com ptrace, sem desativar sanitizadores.

Verificação adicional da reserva com o mesmo código executado no PC e no ESP32:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Iheaders -Isrc/bench \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcStorage.cpp src/ObcDataFlow.cpp src/ObcDelivery.cpp \
    src/bench/DeliveryRecoveryCheck.cpp tests/ObcDeliveryRecoveryCheck.cpp \
    -o /tmp/fcp01-delivery-recovery-check
/tmp/fcp01-delivery-recovery-check
```

O cenário preserva IDs/conteúdo das 32 cópias não duráveis, recupera duas
entregas antigas em sequência e confirma drenagem sem bloqueio. Confere
reserva ocupada durante CONFIRM, liberação mesmo após falha persistindo
CONFIRM, recusa da 33ª admissão e consumidor TX já em uso. A fila mede
2.920 bytes no nativo, contra 2.848 antes do ajuste. Não há alocação
dinâmica adicional em produção. A bancada ESP32 executa essa mesma função
antes de iniciar o fluxo integrado e libera sua memória temporária; falha
impede iniciar o fluxo. As verificações anteriores permanecem intactas.

Bancada adicional com ESP32/USB, sem jumper ou cartão, usando Python com
pyserial e nomes novos para os relatórios:

```bash
pio run -e int008-storage-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStorageRecoveryBenchCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-storage-recovery-bench.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0 \
    --report /tmp/obc-storage-recovery-normal.json
```

`int008-storage-check` reutiliza TT&C, UART e escritor reais, com o backend
FCD1 simulado existente. As injeções e o controle de nomes ficam exclusivamente
nessa bancada. Os três episódios exigem seis tentativas, duas retomadas,
um esgotamento e nenhuma quarta tentativa. Durante montagem simulada de
300 ms, o roteiro aplica `set_time` e confere a referência antes dos dados.
Mantém ping/TM/request_log e confirma sete entregas. O relatório registra
quadros, tempos, pilhas e diagnóstico. Não demonstra integridade física,
remontagem real, alimentação ou durabilidade em reset. Restaurar `obc`
também após falha, preservando o diagnóstico antes de qualquer correção.

Resultados e hashes estão na
[INT-008](../.tasks/03-integracao/done/INT-008-supervisao-e-retomada.md).

## Limites de capacidade e memória — 11/11/2026

O consumidor OBC fixa `espressif32@6.6.0`. As bancadas adicionais
`int006-capacity-check` e `int006-memory-check` herdam TT&C integrado,
com entrada Payload e SD simulados. Exigem somente ESP32/USB, sem jumper.
Não herdam a injeção NO_SPACE de `int006-stress-check` e não alteram os
vetores dos ambientes anteriores.

Executar dentro de `obc-dh/`, com Python contendo pyserial. Escolher
relatórios novos: `ObcLimitsCheck.py` exige --report e recusa substituição.
Sempre restaurar obc, inclusive em falha, e preservar o diagnóstico antes
de alterar firmware ou critérios.

```bash
pio run -e int006-capacity-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcLimitsCheck.py /dev/ttyUSB0 --case capacity --report /tmp/obc-capacity.json
pio run -e int006-memory-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcLimitsCheck.py /dev/ttyUSB0 --case memory --report /tmp/obc-memory.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0 --report /tmp/obc-limits-normal.json
```

Capacidade: 40 snapshots pequenos alteram a altitude de uma aeronave.
O backend retém a primeira gravação DATA até os 40 resultados; a barreira
cede CPU e expira em 8 s. Antes de confirmar pelo solo, o roteiro exige
32 admissões, oito perdas e ocupação/pico 32. Depois libera confirmações
pela USB, exige FIFO e esvaziamento e verifica uma 41ª atualização aceita.
Somente esse ambiente reserva 66 arquivos FCD1, suficientes para os 33
pares DATA/CONFIRM. A espera total para saturação e drenagem é de 180 s.

Memória: cinco sessões FCPU independentes fornecem referência pequena,
JSON válido de 65.536 bytes completado com espaços, snapshot denso de
63.181 bytes (20 aeronaves/32 extras), vetor de 50.034 bytes que esgota
o parser e referência pequena alterada. O caso denso admite READY ou
NO_MEMORY; o vetor de esgotamento exige NO_MEMORY. Em rejeição, ID,
contagem e CRC de toda a representação do snapshot devem permanecer
iguais. A instrumentação lê o snapshot somente no contexto proprietário,
após publicação, e não reserva outra cópia grande. As confirmações são
retidas nesse cenário, com até 21 entregas distintas e 32 arquivos.

`BuildFlowLimitsFixtures.py` usa JSON/struct/zlib e gera somente os novos
artefatos em .pio. `BuildFlowFixtures.py` e flow-fixtures.fcpu permanecem
preservados. Os hooks e barreiras dos limites são compilados somente nas
duas novas bancadas; não há novos comandos ou APIs do firmware de produção.
O roteiro intercala ping/TM/leitura, verifica limites de latência/cadência,
pilhas e quadros e confere DATA extraído por CRC e conteúdo.

Resultados e hashes ficam na
[INT-006](../.tasks/03-integracao/INT-006-fluxo-de-dados.md).
Esses ensaios não comprovam UART entre placas, SD físico, RF, persistência
após reset, taxa de missão ou aceitação simultânea de todos os máximos.

## Estresse em um ESP32 e USB — 11/11/2026

`int006-stress-check` estende a bancada TT&C com injeções exclusivas do
backend simulado. Preserva os bytes FCPU, a capacidade normal de 32 arquivos,
os ambientes anteriores e o firmware de produção. Requer apenas placa/USB,
sem jumper, rádio ou cartão. `ObcStressCheck.py` recusa relatório existente.

Executar dentro de `obc-dh/`, com Python contendo pyserial e nomes novos de
relatório. Restaurar `obc` também se o roteiro falhar; preservar a evidência
e discutir o diagnóstico antes de alterar código ou critérios.

```bash
pio run -e int006-stress-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStressCheck.py /dev/ttyUSB0 --report /tmp/obc-stress-bench.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0 --report /tmp/obc-stress-normal.json
```

O roteiro verifica leitura bloqueada por 1.500 ms com timeout local de
1.000 ms, caixa ocupada inclusive após expiração, descarte da resposta
tardia e leitura posterior correta. `busy.bin` injeta BUSY; `io.bin` devolve
READ_FAILED com bytes parciais que não podem aparecer no pacote. `ok.bin`
e `slow.bin` usam o mesmo conteúdo de dez bytes, somente nesse ambiente.
Seis pings em rajada verificam atendimento durante o bloqueio.

O solo de ensaio retém a primeira confirmação por 10 s e verifica o mesmo
ID/conteúdo na retentativa. Injeta confirmação desconhecida, duplicada e
atrasada, exige avanço FIFO das sete entregas e contabilização das falhas.
Após oito arquivos FCD1, o backend injeta NO_SPACE na próxima gravação:
é um limiar do ensaio, não a capacidade física ou normal. O diário fica
inativo, mas USB, leitura e confirmações em RAM precisam continuar.

O roteiro exige q ≤200 ms, ACK/lg ≤2 s, quadros ≤255 bytes, n consecutivo,
transporte ≤10 ms e pilhas positivas. Passou com 59 ACKs, q máximo de
32 ms, maior ACK em 1.060,223 ms (timeout de leitura) e retentativa em
10.000,149 ms. Evidências e limites na
[INT-006](../.tasks/03-integracao/INT-006-fluxo-de-dados.md).

Separadamente, os 17 testes TT&C existentes passaram na placa com
FakeTransport. Para reproduzir, a partir da raiz do repositório:

```bash
pio test -d tt-c -e esp32 --upload-port /dev/ttyUSB0 --test-port /dev/ttyUSB0
pio run -d obc-dh -e obc -t upload --upload-port /dev/ttyUSB0
```

Essa suíte cobre fila comum cheia preservando ACK prioritário, falha local,
reserva dos quatro eventos TX e confirmações repetidas. A bancada USB não
afirma ter preenchido essa fila. Nenhum recorte comprova cartão físico,
reinício da estação, RF, recuperação persistente após reset ou timeout
operacional definitivo.

## Cliente real da estação e TM/tempo — 11/11/2026

`ObcStationCheck.py` complementa a bancada de entregas com as APIs reais
`Link`, `SessionLog`, `DeliveryStore`, `list_files` e `download_file`.
Não usa o demo da estação nem altera o código TT&C. Cada execução conserva
log, IDs e downloads em um diretório OBC com o mesmo nome do relatório,
sem .json. O roteiro recusa destinos existentes para preservar evidências.

Compilar o leitor existente e executar dentro de `obc-dh/`, com Python
contendo pyserial e somente ESP32/USB:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcDelivery.cpp tools/ReadRecords.cpp -o /tmp/fcp01-read-records
pio run -e int006-ttc-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStationCheck.py /dev/ttyUSB0 --bench \
    --reader /tmp/fcp01-read-records --report /tmp/obc-station-bench.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStationCheck.py /dev/ttyUSB0 --report /tmp/obc-station-normal.json
```

Escolher nomes novos se os destinos já existirem. A bancada exige boot
novo e conserva Payload/SD simulados. O cliente real confirma entregas,
lista e baixa três arquivos fechados; CRC e interpretação são conferidos
com `zlib` e `ReadRecords`. Downloads bem-sucedidos não conservam .part;
tentativa de arquivo ausente pode deixar .part vazio, comportamento do
cliente preservado como evidência. O normal sem periféricos exige no_sd.

O mesmo roteiro verifica UTC desconhecido, argumentos inválidos de
set_time, época zero, referência atual com tolerância de um segundo,
ac=7/depois 0 na bancada e fontes ausentes em null. Repete os 50 pedidos
alternados do roteiro histórico e mantém uma TM pendente por vez.
Verifica q ≤200 ms, resposta ≤2 s, quadro ≤255 bytes, sequência n,
transporte ≤10 ms e pilhas positivas nos serviços ativos.

Passaram os dois recortes em 11/11, sem alterar firmware ou testes antigos:
[cliente/bancada](../examples/obc-station-2026-11-11-bench.json) e
[cliente/obc](../examples/obc-station-2026-11-11-obc.json). Downloads, IDs,
logs e JSON Lines interpretados estão nos diretórios correspondentes.
Procedimentos, versão, hashes e métricas estão na INT-006. Essa aprovação
normal não demonstra durabilidade da estação sob falha, cartão físico,
RF, produtor Pi ou precisão temporal externa. A placa ficou com `obc`.

## Integração TT&C e request_log — 11/11/2026

**Reexecução USB após correção da admissão:** o roteiro original passou
em `int006-ttc-check` e em `obc` sem periféricos. A reprovação anterior foi
preservada. Os relatórios, diagnóstico e estado final da placa estão na
[INT-006](../.tasks/03-integracao/INT-006-fluxo-de-dados.md).
A bancada aprovada extraiu três arquivos com CRC válido. Não substitui
ensaios com cartão, rádio ou estação reais.

A admissão considera 23.552 bytes de pilhas pendentes e margem em heap
interno acessível por byte, além do maior bloco necessário. O parser mantém
seu teto de 98.304 bytes e NO_MEMORY previsto pelo contrato, sem reserva
exclusiva. O cenário não garante todos os máximos simultâneos. Os logs
`[FLOW-MEMORY]` registram pico do parser, heap por byte, maior bloco e mínimo.

O firmware atual conecta entregas identificadas e extração ao transporte
TT&C existente. O responsável OBC aceitou o contrato de `ttc-json.md`.
Timeout de confirmação de 10 s é provisório, incluindo fila de transmissão;
o valor operacional exige medição. A caixa de `request_log` tem um pedido
e prazo local de 1 s; expiração gera `busy` sem reutilizar trabalho em curso.

Verificação portátil, dentro de `obc-dh/`, usando a dependência do consumidor:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -Iheaders -I../tt-c/include -I.pio/libdeps/obc/ArduinoJson/src \
    src/ObcDownlink.cpp src/ObcStorage.cpp ../tt-c/src/TtcProtocol.cpp \
    tests/ObcDownlinkCheck.cpp -o /tmp/fcp01-obc-downlink-check
/tmp/fcp01-obc-downlink-check
```

O executável verifica mapeamento, null/zero, conteúdo estável, limites de
representação, expiração da caixa, descarte de resultado tardio, destino,
paginação e blocos de arquivos temporários reais, inclusive EOF e erros.
ASan/UBSan e a regressão original `ObcDeliveryCheck` passaram em 11/11.
LeakSanitizer exige execução sem ptrace; não reduzir as verificações do
programa para contornar a restrição do ambiente.

Os quatro ambientes abaixo compilaram; a execução USB posterior teve os
resultados e o histórico registrados acima. Dentro de `obc-dh/`, com
ESP32/USB e Python contendo pyserial:

```bash
pio run -e obc -e int006-check -e int006-delivery-check -e int006-ttc-check
pio run -e int006-ttc-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0 --bench
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDownlinkCheck.py /dev/ttyUSB0
```

A bancada nova usa TT&C/umbilical reais e mantém Payload, SD e solo simulados.
O roteiro perde o primeiro `cf`, confere conteúdo e ID na retentativa,
exercita ping durante extração e reconstrói arquivos FCD1 com CRC conferido
por `zlib`. O firmware normal sem SD responde `no_sd`. A bancada antiga
`int006-delivery-check` conserva exclusivamente seu transporte simulado.
O relatório padrão é `/tmp/fcp01-obc-downlink.json`; preservar uma cópia
associada ao binário e às condições para encaminhamento ao setor de testes.

Na reexecução aprovada foram usados nomes novos, sem sobrescrever a falha:
[bancada](../examples/obc-downlink-2026-11-11-bench-02.json) e
[firmware normal](../examples/obc-downlink-2026-11-11-obc-02.json).

`ObcTtcCheck.py` e `ObcDataFlowCheck.py` continuam associados às versões
históricas, sem alteração de entradas/expectativas. O segundo espera
`request_log = invalid_cmd` e não aceita `ac`. O roteiro novo cobre a
evolução de entregas/extração, sem substituir todos os cenários temporais,
de missão ou de concorrência da TES-008. RF, UART entre placas, cartão,
persistência da estação, congestionamento e resets físicos exigem ensaios
próprios. As APIs usadas por `python -m fcp_gs logs list|get` passaram com
o SD simulado no roteiro complementar acima; extração com cartão físico
e a montagem completa da estação continuam pendentes.

## Segunda etapa INT-006 — entregas FCD1

O fluxo acrescenta controle de entregas, cache fixo de 32 posições e diário
DATA/CONFIRM nos segmentos existentes. O leitor atualizado interpreta também
FCD1. Formato, limites da recuperação e proposta `request_log` estão no
[contrato de entregas](../../common/protocol/obc-delivery.md).

Compilar e verificar dentro de `obc-dh/`.

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcStorage.cpp src/ObcDataFlow.cpp src/ObcDelivery.cpp \
    tests/ObcDeliveryCheck.cpp -o /tmp/fcp01-delivery-check
/tmp/fcp01-delivery-check

g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcDelivery.cpp tools/ReadRecords.cpp -o /tmp/fcp01-read-records
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcStorage.cpp src/ObcDataFlow.cpp src/ObcDelivery.cpp \
    tools/DeliveryLogs.cpp -o /tmp/fcp01-delivery-logs
python3 tests/DeliveryLogsCheck.py /tmp/fcp01-delivery-logs /tmp/fcp01-read-records
```

`ObcDeliveryCheck` exercita consumidores independentes, ACK durante gravação,
retentativas, falhas, saturação e resets. A regressão da reserva combina
consulta pendente, rajada de recepção e falha SD, confirma avanço FIFO e
exercita substituição de cópias duráveis conservando espaço para recuperação.
`DeliveryLogsCheck` produz vetores
independentes com struct/zlib, conserva pendências sob CONFIRM inválido ou
incompleto, recupera 40 entregas e confere extração byte a byte por blocos.

Para extrair uma cópia local de diretório já fechado, usar os comandos abaixo.
`read` entrega bytes brutos em stdout. `recover` produz JSON Lines das entregas
pendentes ordenadas e diagnóstico em stderr. Seus códigos são 0 para leitura
íntegra, 1 para corrupção/cauda/órfão com recuperação conservadora e 2 para
uso ou I/O inválido. IDs são strings hexadecimais de 16 caracteres. O leitor
histórico conserva sua saída anterior e acrescenta `delivery_data` e
`delivery_confirm`, com componentes do ID em strings decimais.

```bash
/tmp/fcp01-delivery-logs /copia/obc list
/tmp/fcp01-delivery-logs /copia/obc read boot_0000000000000001_00000000.bin 0 256 > /tmp/bloco.bin
/tmp/fcp01-delivery-logs /copia/obc recover > /tmp/pendencias.jsonl
```

A bancada adicional usa o mesmo receptor UART, escritor lento e scanner,
com respostas de arquivos fechados simuladas e tentativas/ACK simulados.
O armazenamento simulado retém até 32 registros FCD1 e então injeta falta
de espaço. O timeout de 200 ms é exclusivo do ensaio. O relatório conserva
UART, pilhas, ocupação, perdas e os instantes originais das retentativas.

```bash
pio run -e obc -e int006-check -e int006-delivery-check
pio run -e int006-delivery-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDeliveryBenchCheck.py /dev/ttyUSB0
pio run -e obc -t upload --upload-port /dev/ttyUSB0
```

Em 10/11/2026 passaram os testes nativos novos, as regressões de
armazenamento/fluxo/codecs/leitor e os três builds acima. A execução da nova
bancada, memória dinâmica, pilhas e cadência UART continuam pendentes porque
este ambiente não disponibilizou porta ESP32. O build `obc` usou 31.376 bytes
de RAM estática e 408.641 bytes de flash. O cache portátil mediu 2.848 bytes
no executável nativo. Medições dinâmicas exigem a placa.

**Estado atual — revisão documental de 10/11/2026:** modelos, codecs e leitor estão implementados localmente; a primeira etapa INT-006 já liga UART, tempo e gravação. Aceites dos produtores, cartão físico e precisão RF continuam pendentes. O [contrato OBC](../../common/protocol/obc-data.md) define atualizações individuais v1, snapshots v2 e TM v3. O leitor aceita arquivos mistos sem reescrever a origem.

O snapshot JSON do Payload é diferente de `PAYLOAD_DATA` v1 e do pacote de aeronave TT&C. Não fabricar sessão, sequência, disponibilidade ou atualizações individuais a partir de `id`. DAD-003/TES-007 encerraram os respectivos escopos portáteis; o uso integrado e os ensaios têm suas próprias tarefas.

## Serviço de arquivos SD/HSPI

[ObcStorage.h](../headers/ObcStorage.h) implementa operações POSIX portáteis; [ObcStorageEsp32.h](../headers/ObcStorageEsp32.h) monta o cartão e coordena acesso no ESP32. O serviço não interpreta registros nem gera nomes por boot. `ObcStartup` chama sua montagem; o diário da INT-006 cria e grava os arquivos depois, quando disponível. Contrato e evidências do serviço estão em [COM-005](../.tasks/01-comunicacao/done/COM-005-armazenamento-sd.md); o fluxo está na [INT-006](../.tasks/03-integracao/INT-006-fluxo-de-dados.md).

No ESP32, chamar `obc_storage::esp32::Init()` uma primeira vez antes dos consumidores, sem concorrência. Usa HSPI e pinagem existente, 4 MHz ajustáveis, montagem `/sd` e diretório `/sd/obc`, sem formatação automática. Preparar o cartão em FAT32; o FatFs instalado não suporta exFAT, inclusive no caso de um cartão de 64 GB. A confirmação elétrica com EPS e os ensaios físicos continuam pendentes.

Contrato de uso do serviço, já empregado pelo diário integrado:

1. Conferir `RESULT.status` de `Init` e de `OpenNew(nome)`. O chamador fornece nome novo de até 64 bytes, sem caminho; um arquivo existente nunca é sobrescrito. Usar nome associado ao boot reservado e distinguir novas tentativas de abertura quando necessário, sem reabrir o arquivo anterior para escrita.
2. Codificar os registros no módulo correspondente e passar os bytes a `Append(buffer, tamanho)`. Manter o buffer intacto até o retorno. Cada bloco é escrito e sincronizado; até três tentativas totais reescrevem o mesmo bloco no mesmo offset confirmado. Não há cópia própria, fila ou retenção do bloco depois do retorno.
3. Somente `OK` confirma o bloco pelo runtime. `transferred` é a quantidade da última tentativa; `SYNC_FAILED` pode informar o tamanho completo sem confirmar persistência. `attempts` e `system_error` permitem diagnóstico/contabilização na integração. Falha final bloqueia escritas; preservar o arquivo, executar fechamento/desativação, reinicializar e escolher nome novo. Não repetir indefinidamente ou apagar a cauda para esconder o erro.
4. Chamar e conferir `Close`/`End`. Não confiar apenas no destrutor ou na ausência de erro visível. O limite por arquivo é `INT32_MAX` bytes; ao atingir esse limite ou faltar espaço, sinalizar a falha sem apagar arquivos. O diário atual rotaciona entre registros; falha terminal interrompe sua escrita. Recuperação automática permanece na INT-008.

Todas as operações do adaptador usam mutex estático sem espera; disputa retorna `BUSY`. A API é apenas para tarefas e não deve ser chamada pelo atendimento FOC. Não acessar SD/HSPI por fora do adaptador. Tempo de execução real e concorrência FreeRTOS ainda exigem ensaio.

Para extração pelo software, `Read(nome, offset, buffer, capacidade, total_size)` lê apenas arquivos fechados, em blocos do chamador. `OK` com zero bytes representa EOF; o resultado também informa tamanho total e bytes lidos. Não usar bytes de uma leitura malsucedida como extração completa. A composição de resposta de `request_log` pertence a INT-006. Para extração física, encerrar/desmontar e copiar os arquivos do diretório `obc/` com um leitor de cartão; não retirar o cartão durante escrita. Trabalhar no computador sobre uma cópia estável, conforme as instruções do leitor abaixo.

Se a montagem falhar, comunicar indisponibilidade sem formatar. Preservar cópia/imagem antes de qualquer reparo externo de FAT32 em bancada. O serviço não repara o sistema de arquivos, não detecta corrupção silenciosa por leitura de verificação e não garante sobrevivência física a corte de energia. O leitor v1/v2/v3 recupera registros íntegros de uma cópia legível, sem recuperar bytes perdidos.

Verificação nativa, em `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcStorage.cpp tests/ObcStorageCheck.cpp \
    -Wl,--wrap=write,--wrap=read,--wrap=lseek,--wrap=fsync,--wrap=close \
    -o /tmp/fcp01-storage-check
/tmp/fcp01-storage-check
```

Esse teste usa arquivos reais e falhas de I/O injetadas apenas no executável de teste. Não exige cartão, não exercita o mutex/montagem e não substitui TES-006/TES-009.

## Codec do firmware e do computador

[ObcRecordCodec.h](../headers/ObcRecordCodec.h) define `obc_record::RECORD`, com cabeçalho e união dos corpos existentes. O membro ativo deve corresponder a `header.type`; a API dedicada v3 também usa o membro `telemetry`. As APIs `Encode`/`Decode` abaixo são do v1. Inicializar o corpo correspondente antes de usar a API; não copiar a estrutura diretamente para o arquivo.

- `Encode(record, buffer, capacity, written)`: valida o registro e a capacidade antes de escrever. Em qualquer falha, `written = 0` e o destino permanece inalterado. Fonte e destino não devem se sobrepor. O cabeçalho deve informar o tamanho canônico retornado por `obc_data::GetRecordSize()`.
- `Decode(buffer, available, record, consumed)`: interpreta o primeiro registro, podendo receber bytes de registros seguintes. Em sucesso, publica cabeçalho/corpo e informa o comprimento consumido. Em falha, `consumed = 0` e o registro de saída anterior permanece inalterado. Buffer nulo é argumento inválido; buffer não nulo com menos de 24 bytes retorna `INCOMPLETE`.
- `ComputeCrc32(data, size)`: CRC-32/ISO-HDLC. Entrada vazia produz zero; ponteiro nulo somente é permitido com tamanho zero. Vetor ASCII `123456789`: `0xCBF43926`.

O codec não faz I/O, não aloca memória e não procura registros automaticamente. Retornos distinguem argumento inválido, buffer insuficiente, entrada incompleta, assinatura incorreta, versão/tipo desconhecido, comprimento inválido, CRC incorreto e conteúdo inválido. Cabeçalhos são verificados antes de confiar no comprimento. Dados só são publicados após CRC e validação semântica.

A codificação é explícita em little-endian. `memcpy` é usado somente entre reais e seus padrões inteiros de bits ou campos textuais, sem aliasing ou acessos desalinhados. Campos indisponíveis e padding textual são escritos como zero; na leitura, a máscara determina a disponibilidade e os valores ausentes são normalizados. Descrições e timestamps recebem NUL na RAM, fora dos bytes persistidos. Os bits dos reais disponíveis, inclusive zero negativo, são preservados.

### Snapshots v2

O mesmo header expõe `GetSnapshotSize`, `EncodeSnapshot` e `DecodeSnapshot` para `PAYLOAD_SNAPSHOT`, sem ampliar a união `RECORD` do v1. O [contrato v2](../../common/protocol/obc-data.md#persistência-de-snapshots--v2) especifica assinatura `FCP2`, versão 2/tipo 8, cabeçalho de 24 bytes, corpo variável e CRC-32. Strings são comprimento u8 seguido por bytes sem NUL/padding; campos ausentes não ocupam espaço. Um registro vazio tem 37 bytes; o máximo é 64497.

Primeiro obter o tamanho exato e conferir o resultado. Depois fornecer buffer suficiente a `EncodeSnapshot`, conservando o modelo e o buffer em regiões separadas. Codificação valida tudo antes de escrever; decodificação valida CRC/conteúdo antes de publicar em duas passagens, sem outro snapshot temporário completo. Falhas deixam destino preservado e contagem zero. A fonte deve permanecer estável durante a chamada. `DecodeSnapshot` aceita registros posteriores na entrada e informa apenas o comprimento do primeiro.

O modelo de até 64672 bytes e o buffer de até 64497 bytes pertencem ao chamador; não alocar ambos na pilha padrão de uma tarefa. O fluxo integrado reserva o modelo no receptor e, para o escritor, dois blocos de saída no heap. Ver a seção INT-006 abaixo; as bancadas isoladas têm reservas próprias. Medidas Xtensa `-Os -fstack-usage` dos helpers foram 3280 bytes para codificar uma aeronave e 3408 para percorrer o corpo; acrescentar quadros dos chamadores e da tarefa ao dimensionar pilha. Esses números não garantem margem de execução em uma tarefa de 4096 bytes.

Exportação: `type: "payload_snapshot"`, `version: 2`, envelope temporal existente e `data: {"id": ..., "aircraft": [...]}`. A lista inclui ICAO, identificação, posição, altitude, `tick` e extras chave/valor. Campos ausentes são `null`; texto disponível não UTF-8 recebe `null`, `<nome>_encoding` e `<nome>_hex`, inclusive em chaves/valores dos extras. Isso conserva informação sem produzir JSON inválido. `id` repetido não elimina snapshots, e `tick` nunca é substituído por UTC reconstruído.

## Compilar o leitor nativo

Dentro de `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcDelivery.cpp tools/ReadRecords.cpp -o /tmp/fcp01-read-records
/tmp/fcp01-read-records entrada.bin > registros.jsonl
```

O executável também aceita `--help`. O binário fica fora do repositório. O leitor usa o mesmo decoder C++ do firmware, sem manter outra implementação de layout em produção. A ferramenta TT&C existente interpreta logs do enlace com outro envelope e não foi modificada para assumir que um arquivo OBC seja um log RF.

Usar uma cópia estável e seekable do arquivo de um único OBC, já extraída para o computador. Entrada por stdin ou arquivo em gravação não é suportada. Extração pelo cartão/umbilical e o tratamento de `request_log` pertencem a COM-005/INT-006; esta ferramenta apenas lê o arquivo fornecido. Nunca usar o próprio arquivo de entrada como destino do redirecionamento de stdout.

## Recuperação e diagnósticos

Cada candidato precisa ter assinatura, versão/tipo, comprimento válido, CRC e conteúdo válidos. V1 e TM v3 exigem seus tamanhos canônicos; v2 exige o corpo variável consumir exatamente o comprimento declarado. Trechos inválidos são descartados da interpretação, sem modificar a origem. A busca avança por `FCP1`/`FCP2`/`FCP3` e não salta o comprimento declarado por um candidato inválido; assim, não esconde um registro íntegro que comece dentro de um trecho danificado. A janela nativa é limitada a 64497 bytes e conserva possíveis assinaturas entre blocos. O programa mantém também um modelo de snapshot, não todos os snapshots do arquivo.

Somente JSON Lines vai para stdout. stderr informa intervalos descartados `[início,fim)`, causas, cauda incompleta e um resumo de registros válidos/trechos/bytes. Quantidade de bytes descartados não determina quantas mensagens foram perdidas. Um arquivo vazio é aceito, com saída vazia.

| Código | Significado |
|---:|---|
| 0 | Todos os bytes interpretados sem corrupção/truncamento |
| 1 | Houve trechos descartados ou registro incompleto; registros íntegros ainda foram exportados |
| 2 | Uso inválido, arquivo inacessível/não seekable, erro de leitura/saída ou falha de recurso |

Recuperar significa localizar registros íntegros posteriores. O CRC não permite reconstruir os bytes perdidos, não solicita reenvio UART e não comprova recuperação do FAT32 após corte de energia. Esse comportamento não substitui COM-004, COM-005 ou os ensaios físicos.

## Horário reconstruído e JSON

A primeira passagem coleta somente referências válidas; a segunda exporta registros na ordem original. A ferramenta guarda `K` referências, não o arquivo inteiro, com memória O(K) e busca temporal O(log K). Não deve haver alteração concorrente da entrada entre as passagens.

Por boot, selecionar a última referência anterior ou simultânea ao evento; antes da primeira, aplicar a primeira retroativamente. Empates no mesmo instante usam a última ocorrência no arquivo. A seleção é pelo instante monotônico, não pela ordem de chegada das referências ao arquivo. Nunca aplicar referência de outro boot nem inferir o intervalo desligado.

A exportação conserva o envelope lógico do contrato e acrescenta:

| Campo | Significado |
|---|---|
| file_offset | Posição em bytes do registro validado no arquivo |
| unix_ms | Unix estimado por `obc_time::Reconstruct()`, ou `null` |
| time_status | `estimated`, `no_reference` ou `out_of_range` |
| reference_offset | Posição da referência utilizada, ou `null` quando não existe |

Ausência de UTC ou resultado fora da faixa temporal não significa corrupção binária: o registro permanece exportado com `null` e estado explícito. A compensação vem da referência gravada, não do valor global da compilação atual. O timestamp local do Payload e o `ts` original da TM permanecem em `data`, sem reescrita.

Os campos opcionais aparecem como `null`; inteiros de 64 bits saem em decimal sem conversão para double. Reais usam precisão suficiente para round-trip e separador decimal independente da configuração regional. O consumidor JSON deve preservar inteiros de 64 bits. Aspas, barras e caracteres de controle são escapados.

Se uma descrição não for UTF-8 válida, o registro binário continua válido: `description` sai como `null`, acompanhado de `description_encoding = "invalid_utf8"` e `description_hex` com os bytes do texto. Essa indicação não é um descarte binário e não muda o código de saída para 1. Texto válido, incluindo UTF-8, permanece texto. Os exemplos JSON do contrato são projeções legíveis, não mudanças no protocolo UART ou LoRa.

## Verificações reproduzíveis

Dentro de `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcData.cpp src/ObcRecordCodec.cpp tests/ObcRecordCodecCheck.cpp \
    -o /tmp/fcp01-codec-check
/tmp/fcp01-codec-check
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcDelivery.cpp tools/ReadRecords.cpp -o /tmp/fcp01-read-records
python3 tests/ReadRecordsCheck.py /tmp/fcp01-read-records
```

O teste C++ compara os sete tipos com vetores completos independentes, gerados com `struct`/`zlib` da biblioteca padrão Python. Também verifica CRC conhecido, todos os pontos de truncamento/capacidade insuficiente, preservação das saídas, ausência, limites e registros consecutivos. O teste Python cria entradas independentes e executa o leitor C++ real; ele não é um segundo leitor de produção. Confere JSON, recuperação, fronteiras da janela, referências sucessivas, corrupção, offsets, texto e preservação byte a byte dos arquivos originais.

Para snapshots v2, executar também:

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcData.cpp src/ObcRecordCodec.cpp tests/SnapshotCodecCheck.cpp \
    -o /tmp/fcp01-snapshot-codec-check
python3 tests/SnapshotRecordsCheck.py /tmp/fcp01-snapshot-codec-check /tmp/fcp01-read-records
```

O script gera quatro vetores independentes com `struct`/`zlib` em diretório temporário e invoca o teste C++ real. Confere lista vazia, ICAO isolado, dados/extras opcionais e limite máximo, comparação byte a byte, buffers insuficientes, prefixos truncados, CRC e corpo inválidos com saída preservada. Em seguida executa o leitor real com arquivos v1/v2/mistos, referências v1 para snapshots v2, corrupção/truncamento, fronteira da janela, textos não UTF-8 e origem intacta. Os testes v1 anteriores permanecem sem alteração.

## Primeira etapa INT-006: fluxo, TM v3 e snapshot partido

O leitor atual aceita v1/v2/v3 no mesmo arquivo. TM v3 acrescenta
disponibilidade aos contadores, sem reescrever arquivos antigos ou alargar
os bits aceitos no v1. Usar o leitor atualizado também ao reverter o firmware.

Executar dentro de `obc-dh/`, com as dependências do ambiente `obc` instaladas:

```bash
python3 tools/PatchArduinoJson.py
g++ -std=c++11 -Wall -Wextra -Werror -O1 -g \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Iheaders -I.pio/libdeps/obc/ArduinoJson/src \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcStorage.cpp src/PayloadReceiver.cpp src/ObcDataFlow.cpp \
    tests/ObcDataFlowCheck.cpp -o /tmp/fcp01-data-flow-check
/tmp/fcp01-data-flow-check

g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    tests/TelemetryV3Check.cpp -o /tmp/fcp01-telemetry-v3-check
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcStorage.cpp src/ObcDataFlow.cpp \
    tests/SnapshotPartsCheck.cpp -o /tmp/fcp01-snapshot-parts-check
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    src/ObcDelivery.cpp tools/ReadRecords.cpp -o /tmp/fcp01-read-records
python3 tests/TelemetryV3Check.py /tmp/fcp01-telemetry-v3-check /tmp/fcp01-read-records
python3 tests/SnapshotPartsCheck.py /tmp/fcp01-snapshot-parts-check /tmp/fcp01-read-records
```

Os vetores usam `struct`/`zlib`; o teste partido verifica fronteiras de
campos/CRC, snapshot máximo de 64.497 bytes, escrita integral e falha na
segunda parte. Um registro parcial não incrementa `records_written` nem
é publicado pelo leitor. O teste do fluxo verifica época zero, volta do
contador de 32 bits, referências entre boots, fila cheia, saturação,
rotação com BOOT/referência e falhas de escrita. O teste do receptor
anterior permanece separado e inalterado. Executar ASan/LeakSanitizer
fora de ambientes com ptrace, sem desabilitar os sanitizadores.

As observações de arquivo/SD são distintas: arquivos POSIX e falhas simuladas
comprovam a lógica local; o cartão e a ponta Pi conservam seus ensaios.
Para a verificação USB e a bancada com SD lento, consultar
[examples/README.md](../examples/README.md#bancada-integrada-int-006).

## Recuperação I2C COM-003

`I2cBus` oferece `ExecuteChecked` com classificação da falha e o callback
booleano de `Execute` no mesmo executor. `GetDiagnostics` consulta estado,
causas, contadores, fila e duração do callback. Após esgotamento,
`RequestRecovery` solicita um ciclo ao worker e seu retorno indica aceitação.
A conclusão pode ser observada no estado publicado por `GetDiagnostics`.

A política provisória consta da
[COM-003](../.tasks/01-comunicacao/done/COM-003-recuperacao-i2c.md). O núcleo usa
duas tentativas por ciclo e orçamento cooperativo de 100 ms. Uma operação
iniciada conserva seu contexto até retornar. Cada callback deve concluir
as transações de Wire e liberar seus locks antes do retorno. A recuperação
do controlador exige sua posse exclusiva e os pull-ups da montagem.

O comando nativo abaixo executa o núcleo e o executor de produção. As
primitivas FreeRTOS são substituídas por threads e semáforos nativos somente
nesse executável. A bancada ESP32 usa as primitivas reais. Ambos simulam
GPIOs, resultados do controlador e relógio da recuperação. A espera das
barreiras é calculada como retenção do callback mais 1.000 ms e serve para
diagnosticar falhas do roteiro.

```bash
g++ -std=c++11 -Wall -Wextra -Werror -pthread \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -DOBC_I2C_RECOVERY_CHECK -Itests/native -Iheaders \
    src/I2cRecovery.cpp src/I2cBus.cpp tests/native/NativeI2cRuntime.cpp \
    src/bench/I2cRecoveryCheck.cpp -o /tmp/fcp01-i2c-recovery-check
/tmp/fcp01-i2c-recovery-check
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=undefined -fno-sanitize-recover=undefined \
    tests/I2cSchedulingCheck.cpp -o /tmp/fcp01-i2c-scheduling-check
/tmp/fcp01-i2c-scheduling-check
```

Os cenários verificam classificação, recuperação na primeira/segunda
tentativa, SDA/SCL presos, até nove pulsos, falha de desativação/inicialização,
orçamento inclusive wrap e suas fronteiras, fila concorrente, expiração,
esgotamento e nova recuperação explícita. A revisão identificou intermitência
no cenário de contexto com espera de fila de 1 ms. O responsável autorizou
fila de 1.000 ms e retenção do callback iniciado por 1.005 ms. O diagnóstico,
a autorização e a verificação estão registrados na COM-003.

Os valores de bancada ficam nas constantes `context_queue_wait_ms` e
`context_hold_ms`, no início de `src/bench/I2cRecoveryCheck.cpp`. A retenção
é calculada como prazo de fila mais 5 ms. Alterar esses valores exige
recompilar e carregar a bancada. O relatório imprime os valores usados e
confere que a duração do callback alcançou o prazo da fila. No uso
operacional, cada chamada de `Execute` ou `ExecuteChecked` pode fornecer
seu próprio `max_queue_wait_ms`.

Para executar no ESP32 com Python contendo pyserial, usar a sequência abaixo.
O roteiro envia `g` para iniciar os cenários uma vez, espera PASS por até
20 segundos e grava resultado, métricas e eventual falha em JSON. A coluna
`stack` do relatório nativo é zero. Na placa, ela informa os bytes livres
observados na pilha FreeRTOS do executor.

```bash
pio run -e obc -e i2c-recovery-check -e int006-check -e int006-delivery-check
pio run -e i2c-recovery-check -t upload --upload-port /dev/ttyUSB0
python3 tests/I2cRecoveryBenchCheck.py /dev/ttyUSB0
pio run -e obc -t upload --upload-port /dev/ttyUSB0
```

Em 10/11/2026, o roteiro da bancada passou no ESP32 com 3 ciclos,
5 tentativas, 2 recuperações, 1 esgotamento e 3.276 bytes livres na pilha.
Com o ajuste aprovado, o callback máximo mediu 1.005.392 µs. O cenário
nativo passou em cinco execuções consecutivas com ASan/UBSan, e os quatro
builds passaram.
O firmware normal foi restaurado e aprovou os 75 cenários do roteiro USB
existente. A recuperação física e o tempo de execução do driver permanecem
para a TES-009. Relatório da bancada em
`/tmp/fcp01-i2c-recovery-bench.json`.

## Pendências

DAD-003 e TES-007 foram concluídas em 09/11/2026 no escopo dos codecs,
leitor e verificação portátil. A consolidação dos produtores fica nas
INT-004/INT-005/INT-006; estados duráveis e retomada de atuações ficam em
INT-007/INT-008, com ensaios físicos em TES-008/TES-009. Burn Wire é
representado apenas pelos eventos disponíveis: eles não autorizam atuação
nem confirmam liberação. Estatísticas operacionais e interoperabilidade
continuam dependendo da integração; os critérios de validação foram preservados.
