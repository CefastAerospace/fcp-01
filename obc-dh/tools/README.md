# Codec e leitura de registros OBC v1/v2

Implementação local de DAD-003 sobre o [contrato v1 proposto em DAD-001](../../common/protocol/obc-data.md). O contrato ainda aguarda aceite dos produtores. Os layouts, unidades, validadores e testes anteriores foram preservados. Não há integração automática ao boot, UART, SD ou TT&C.

Em 05/11/2026, DAD-001 acrescentou modelos em memória de snapshots JSON do Payload com extras textuais. DAD-003 passou a representá-los em registros v2 próprios, de 37–64497 bytes, mantendo layouts e APIs v1. O leitor aceita arquivos antigos, novos ou mistos. Não converter snapshots em `PAYLOAD_DATA` fabricando sessão, sequência ou campos recém-atualizados. Isso não implementa recepção UART ou gravação automática dos novos registros.

## Serviço de arquivos SD/HSPI

[ObcStorage.h](../headers/ObcStorage.h) implementa operações POSIX portáteis; [ObcStorageEsp32.h](../headers/ObcStorageEsp32.h) monta o cartão e coordena acesso no ESP32. A API não interpreta registros ou gera nomes por boot. INT-003 chama sua montagem no boot de bancada, sem abrir arquivos ou gravar registros. O contrato completo e as evidências estão em [COM-005](../.tasks/01-comunicacao/COM-005-armazenamento-sd.md).

No ESP32, chamar `obc_storage::esp32::Init()` uma primeira vez antes dos consumidores, sem concorrência. Usa HSPI e pinagem existente, 4 MHz ajustáveis, montagem `/sd` e diretório `/sd/obc`, sem formatação automática. Preparar o cartão em FAT32; o FatFs instalado não suporta exFAT, inclusive no caso de um cartão de 64 GB. A confirmação elétrica com EPS e os ensaios físicos continuam pendentes.

Fluxo para a integração futura:

1. Conferir `RESULT.status` de `Init` e de `OpenNew(nome)`. O chamador fornece nome novo de até 64 bytes, sem caminho; um arquivo existente nunca é sobrescrito. Usar nome associado ao boot reservado e distinguir novas tentativas de abertura quando necessário, sem reabrir o arquivo anterior para escrita.
2. Codificar os registros no módulo correspondente e passar os bytes a `Append(buffer, tamanho)`. Manter o buffer intacto até o retorno. Cada bloco é escrito e sincronizado; até três tentativas totais reescrevem o mesmo bloco no mesmo offset confirmado. Não há cópia própria, fila ou retenção do bloco depois do retorno.
3. Somente `OK` confirma o bloco pelo runtime. `transferred` é a quantidade da última tentativa; `SYNC_FAILED` pode informar o tamanho completo sem confirmar persistência. `attempts` e `system_error` permitem diagnóstico/contabilização na integração. Falha final bloqueia escritas; preservar o arquivo, executar fechamento/desativação, reinicializar e escolher nome novo. Não repetir indefinidamente ou apagar a cauda para esconder o erro.
4. Chamar e conferir `Close`/`End`. Não confiar apenas no destrutor ou na ausência de erro visível. O limite por arquivo é `INT32_MAX` bytes; ao atingir esse limite ou faltar espaço, sinalizar a falha sem apagar arquivos. Decidir continuação/rotação no fluxo integrado.

Todas as operações do adaptador usam mutex estático sem espera; disputa retorna `BUSY`. A API é apenas para tarefas e não deve ser chamada pelo atendimento FOC. Não acessar SD/HSPI por fora do adaptador. Tempo de execução real e concorrência FreeRTOS ainda exigem ensaio.

Para extração pelo software, `Read(nome, offset, buffer, capacidade, total_size)` lê apenas arquivos fechados, em blocos do chamador. `OK` com zero bytes representa EOF; o resultado também informa tamanho total e bytes lidos. Não usar bytes de uma leitura malsucedida como extração completa. A composição de resposta de `request_log` pertence a INT-006. Para extração física, encerrar/desmontar e copiar os arquivos do diretório `obc/` com um leitor de cartão; não retirar o cartão durante escrita. Trabalhar no computador sobre uma cópia estável, conforme as instruções do leitor abaixo.

Se a montagem falhar, comunicar indisponibilidade sem formatar. Preservar cópia/imagem antes de qualquer reparo externo de FAT32 em bancada. O serviço não repara o sistema de arquivos, não detecta corrupção silenciosa por leitura de verificação e não garante sobrevivência física a corte de energia. O leitor v1/v2 recupera registros íntegros de uma cópia legível, sem recuperar bytes perdidos.

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

[ObcRecordCodec.h](../headers/ObcRecordCodec.h) define `obc_record::RECORD`, com cabeçalho e união dos corpos existentes. O membro ativo deve corresponder a `header.type`. Inicializar o corpo correspondente antes de usar a API; não copiar a estrutura diretamente para o arquivo.

- `Encode(record, buffer, capacity, written)`: valida o registro e a capacidade antes de escrever. Em qualquer falha, `written = 0` e o destino permanece inalterado. Fonte e destino não devem se sobrepor. O cabeçalho deve informar o tamanho canônico retornado por `obc_data::GetRecordSize()`.
- `Decode(buffer, available, record, consumed)`: interpreta o primeiro registro, podendo receber bytes de registros seguintes. Em sucesso, publica cabeçalho/corpo e informa o comprimento consumido. Em falha, `consumed = 0` e o registro de saída anterior permanece inalterado. Buffer nulo é argumento inválido; buffer não nulo com menos de 24 bytes retorna `INCOMPLETE`.
- `ComputeCrc32(data, size)`: CRC-32/ISO-HDLC. Entrada vazia produz zero; ponteiro nulo somente é permitido com tamanho zero. Vetor ASCII `123456789`: `0xCBF43926`.

O codec não faz I/O, não aloca memória e não procura registros automaticamente. Retornos distinguem argumento inválido, buffer insuficiente, entrada incompleta, assinatura incorreta, versão/tipo desconhecido, comprimento inválido, CRC incorreto e conteúdo inválido. Cabeçalhos são verificados antes de confiar no comprimento. Dados só são publicados após CRC e validação semântica.

A codificação é explícita em little-endian. `memcpy` é usado somente entre reais e seus padrões inteiros de bits ou campos textuais, sem aliasing ou acessos desalinhados. Campos indisponíveis e padding textual são escritos como zero; na leitura, a máscara determina a disponibilidade e os valores ausentes são normalizados. Descrições e timestamps recebem NUL na RAM, fora dos bytes persistidos. Os bits dos reais disponíveis, inclusive zero negativo, são preservados.

### Snapshots v2

O mesmo header expõe `GetSnapshotSize`, `EncodeSnapshot` e `DecodeSnapshot` para `PAYLOAD_SNAPSHOT`, sem ampliar a união `RECORD` do v1. O [contrato v2](../../common/protocol/obc-data.md#persistência-de-snapshots--v2) especifica assinatura `FCP2`, versão 2/tipo 8, cabeçalho de 24 bytes, corpo variável e CRC-32. Strings são comprimento u8 seguido por bytes sem NUL/padding; campos ausentes não ocupam espaço. Um registro vazio tem 37 bytes; o máximo é 64497.

Primeiro obter o tamanho exato e conferir o resultado. Depois fornecer buffer suficiente a `EncodeSnapshot`, conservando o modelo e o buffer em regiões separadas. Codificação valida tudo antes de escrever; decodificação valida CRC/conteúdo antes de publicar em duas passagens, sem outro snapshot temporário completo. Falhas deixam destino preservado e contagem zero. A fonte deve permanecer estável durante a chamada. `DecodeSnapshot` aceita registros posteriores na entrada e informa apenas o comprimento do primeiro.

O modelo de até 64672 bytes e o buffer de até 64497 bytes pertencem ao chamador; não alocar ambos na pilha padrão de uma tarefa. Não há reserva desses buffers no firmware de bancada. Medidas Xtensa `-Os -fstack-usage` dos helpers foram 3280 bytes para codificar uma aeronave e 3408 para percorrer o corpo; acrescentar quadros dos chamadores e da tarefa ao dimensionar pilha. Esses números não garantem margem de execução em uma tarefa de 4096 bytes.

Exportação: `type: "payload_snapshot"`, `version: 2`, envelope temporal existente e `data: {"id": ..., "aircraft": [...]}`. A lista inclui ICAO, identificação, posição, altitude, `tick` e extras chave/valor. Campos ausentes são `null`; texto disponível não UTF-8 recebe `null`, `<nome>_encoding` e `<nome>_hex`, inclusive em chaves/valores dos extras. Isso conserva informação sem produzir JSON inválido. `id` repetido não elimina snapshots, e `tick` nunca é substituído por UTC reconstruído.

## Compilar o leitor nativo

Dentro de `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iheaders \
    src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp \
    tools/ReadRecords.cpp -o /tmp/fcp01-read-records
/tmp/fcp01-read-records entrada.bin > registros.jsonl
```

O executável também aceita `--help`. O binário fica fora do repositório. O leitor usa o mesmo decoder C++ do firmware, sem manter outra implementação de layout em produção. A ferramenta TT&C existente interpreta logs do enlace com outro envelope e não foi modificada para assumir que um arquivo OBC seja um log RF.

Usar uma cópia estável e seekable do arquivo de um único OBC, já extraída para o computador. Entrada por stdin ou arquivo em gravação não é suportada. Extração pelo cartão/umbilical e o tratamento de `request_log` pertencem a COM-005/INT-006; esta ferramenta apenas lê o arquivo fornecido. Nunca usar o próprio arquivo de entrada como destino do redirecionamento de stdout.

## Recuperação e diagnósticos

Cada candidato precisa ter assinatura, versão/tipo, comprimento válido, CRC e conteúdo válidos. V1 exige seus tamanhos canônicos; v2 exige o corpo variável consumir exatamente o comprimento declarado. Trechos inválidos são descartados da interpretação, sem modificar a origem. A busca avança por `FCP1`/`FCP2` e não salta o comprimento declarado por um candidato inválido; assim, não esconde um registro íntegro que comece dentro de um trecho danificado. A janela nativa é limitada a 64497 bytes e conserva possíveis assinaturas entre blocos. O programa mantém também um modelo de snapshot, não todos os snapshots do arquivo.

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
| unix_ms | Unix estimado por `ObcTime::Reconstruct()`, ou `null` |
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
    tools/ReadRecords.cpp -o /tmp/fcp01-read-records
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

## Pendências

DAD-003 permanece em andamento enquanto DAD-001 não estiver consolidado e a pendência dos estados duráveis de retomada não estiver explicitamente resolvida. Burn Wire é representado somente pelos eventos já previstos; os eventos não autorizam atuação nem comprovam liberação após reset. Esta implementação não conclui automaticamente TES-007, não gera estatísticas operacionais ativas e não comprova interoperabilidade ou comportamento físico.
