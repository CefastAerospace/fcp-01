# Codec e leitura de registros OBC v1/v2/v3

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
