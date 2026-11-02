# Codec e leitura de registros OBC v1

Implementação local de DAD-003 sobre o [contrato v1 proposto em DAD-001](../../common/protocol/obc-data.md). O contrato ainda aguarda aceite dos produtores. Os layouts, unidades, validadores e testes anteriores foram preservados. Não há integração automática ao boot, UART, SD ou TT&C.

## Codec do firmware e do computador

[ObcRecordCodec.h](../headers/ObcRecordCodec.h) define `obc_record::RECORD`, com cabeçalho e união dos corpos existentes. O membro ativo deve corresponder a `header.type`. Inicializar o corpo correspondente antes de usar a API; não copiar a estrutura diretamente para o arquivo.

- `Encode(record, buffer, capacity, written)`: valida o registro e a capacidade antes de escrever. Em qualquer falha, `written = 0` e o destino permanece inalterado. Fonte e destino não devem se sobrepor. O cabeçalho deve informar o tamanho canônico retornado por `obc_data::GetRecordSize()`.
- `Decode(buffer, available, record, consumed)`: interpreta o primeiro registro, podendo receber bytes de registros seguintes. Em sucesso, publica cabeçalho/corpo e informa o comprimento consumido. Em falha, `consumed = 0` e o registro de saída anterior permanece inalterado. Buffer nulo é argumento inválido; buffer não nulo com menos de 24 bytes retorna `INCOMPLETE`.
- `ComputeCrc32(data, size)`: CRC-32/ISO-HDLC. Entrada vazia produz zero; ponteiro nulo somente é permitido com tamanho zero. Vetor ASCII `123456789`: `0xCBF43926`.

O codec não faz I/O, não aloca memória e não procura registros automaticamente. Retornos distinguem argumento inválido, buffer insuficiente, entrada incompleta, assinatura incorreta, versão/tipo desconhecido, comprimento inválido, CRC incorreto e conteúdo inválido. Cabeçalhos são verificados antes de confiar no comprimento. Dados só são publicados após CRC e validação semântica.

A codificação é explícita em little-endian. `memcpy` é usado somente entre reais e seus padrões inteiros de bits ou campos textuais, sem aliasing ou acessos desalinhados. Campos indisponíveis e padding textual são escritos como zero; na leitura, a máscara determina a disponibilidade e os valores ausentes são normalizados. Descrições e timestamps recebem NUL na RAM, fora dos bytes persistidos. Os bits dos reais disponíveis, inclusive zero negativo, são preservados.

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

Cada candidato precisa ter assinatura, versão/tipo, comprimento canônico, CRC e conteúdo válidos. Trechos inválidos são descartados da interpretação, sem modificar a origem. A busca avança por assinatura e não salta o comprimento declarado por um candidato inválido; assim, não esconde um registro íntegro que comece dentro de um trecho danificado. A janela binária é limitada a 512 bytes e conserva possíveis assinaturas entre blocos.

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

## Pendências

DAD-003 permanece em andamento enquanto DAD-001 não estiver consolidado e a pendência dos estados duráveis de retomada não estiver explicitamente resolvida. Burn Wire é representado somente pelos eventos já previstos; os eventos não autorizam atuação nem comprovam liberação após reset. Esta implementação não conclui automaticamente TES-007, não gera estatísticas operacionais ativas e não comprova interoperabilidade ou comportamento físico.
