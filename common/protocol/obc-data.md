# Dados OBC — registros v1, snapshots v2 e TM v3

**Estado atual — revisão documental de 10/11/2026:** modelos, validadores, codecs e leitor têm implementação local, com evidências nas tarefas DAD-001/DAD-003/TES-007. A primeira etapa INT-006 já liga recepção de snapshots, referência temporal, gravação e TM. Os aceites de Payload, TT&C, ADCS, EPS e Térmico continuam nas integrações correspondentes; ensaios físicos não são presumidos por este contrato.

Referência C++: [ObcData.h](../../obc-dh/headers/models/ObcData.h), [ObcData.cpp](../../obc-dh/src/ObcData.cpp) e [ObcRecordCodec.h](../../obc-dh/headers/ObcRecordCodec.h). Procedimentos: [guia do leitor](../../obc-dh/tools/README.md). Estados históricos, hashes e decisões das entregas de 01/11 e 05/11 permanecem nas tarefas concluídas, não descrevem ausência de parser ou integração no firmware atual.

O contrato separa dados lógicos, transporte e persistência. O conteúdo previsto do Payload são snapshots JSON completos; o receptor local usa o [FCPU v1](payload-uart.md), com cabeçalho/CRC e ACK/NACK binários também transmitidos pelo cabo ao Pi. Aceite, implementação do produtor e tráfego permanecem na INT-006. Não confundir esses controles com o formato binário interno do SD.

Os registros individuais v1 são preservados para compatibilidade; não são uma conversão automática dos snapshots. Snapshots usam v2, e a TM integrada usa v3 para representar contagens indisponíveis. TT&C conserva seu [contrato JSON](ttc-json.md), com `n` atribuído pelo enlace e limites de 245/255 bytes. Não existe equivalência entre o tamanho de uma struct, um registro persistido e um quadro de transporte.

## 0. Snapshot do Payload — modelo em memória

Referência recebida: [payload.json](../../obc-dh/payload.json), objeto com `id` e `messages`, indexado por ICAO. Não é uma atualização parcial: cada objeto completo descreve a lista inteira. A coleção pode crescer, diminuir ou permanecer com os mesmos ICAOs e valores atualizados. Ordem não define identidade. O arquivo de exemplo é preservado. Os modelos são independentes do parser, implementado em `PayloadReceiver`.

### Tipos e disponibilidade

`PAYLOAD_SNAPSHOT` contém `id` u64 opaco (inclusive zero), `stamp` do tipo existente `EVENT_STAMP`, `aircraft_count` e array de `AIRCRAFT_STATE`. O boot deve ser não zero; ms monotônicos podem ser zero. Não interpretar `id` como sessão, sequência, contagem ADS-B ou garantia de ordenação. Ausência de `id` não pode ser convertida implicitamente em zero pelo parser.

| Origem JSON | Campo de AIRCRAFT_STATE | Representação e mapeamento implementado no receptor local |
|---|---|---|
| Chave em `messages` | `icao` | u32 limitado a 24 bits; interpretar seis hexadecimais sem distinção de caixa. Se `ICAO Address` existir, conferir concordância. |
| `Identification` | `identification` | Texto original, até 16 bytes. |
| `Latitude` / `Longitude` | `latitude` / `longitude` | f64 em graus, finitos, nos intervalos −90..90 / −180..180. Ambas disponíveis ou ambas indisponíveis. |
| `Altitude` | `altitude` | f64 finito em pés; interpretar estritamente número e sufixo ` feet`. Zero e valores negativos são permitidos. |
| `tick` | `tick` | Texto original de até 40 bytes, conservando frações e fuso. Não é UTC validado nem referência absoluta do OBC. |
| Outros campos e textos não decodificados | `extras` | Array de `EXTRA_FIELD`, pares textuais `key`/`value`. Inclui velocidades e diagnósticos sem inventar unidades ou calcular rumo. |

`available_fields` usa `SNAPSHOT_LATITUDE`, `SNAPSHOT_LONGITUDE`, `SNAPSHOT_ALTITUDE`, `SNAPSHOT_IDENTIFICATION` e `SNAPSHOT_TICK`. Os três primeiros reutilizam os bits existentes de posição/altitude; os dois últimos pertencem somente ao snapshot. Não confundir a máscara completa com `PAYLOAD_DATA`. Bits não definidos são rejeitados. Campo ausente ou `null` deixa seu bit desmarcado; bytes numéricos/textuais indisponíveis são ignorados, sem fabricar validade para zero. Strings disponíveis podem ser vazias; chaves de extras não podem. Validadores não verificam UTF-8 ou o calendário de `tick`.

Uma aeronave com apenas ICAO, identificação ou extras é válida. `96298 (not decoded)` e `26514 (not decoded)` devem permanecer como textos nos extras, com posição indisponível. O parser deve consumir o texto numérico inteiro, sem interpretar o prefixo de uma mensagem de diagnóstico como coordenada. Se somente uma coordenada for válida, conservar os textos recebidos nos extras e deixar o par indisponível. Textos conhecidos não interpretáveis também permanecem nos extras; não descartar informação silenciosamente.

### Limites locais e validação

- Até **20 aeronaves**, **32 extras por aeronave**, **32 bytes por chave** e **64 bytes por valor**. Limites textuais excluem NUL; arrays reservam um byte adicional. Esses limites cobrem a amostra, mas ainda precisam do aceite do produtor.
- Extras representam os valores string presentes no exemplo. Outros tipos JSON precisam de contrato explícito; não converter objetos, arrays, números ou `null` silenciosamente em texto. Não há preservação byte a byte da formatação do JSON original.
- `Validate(AIRCRAFT_STATE)` verifica ICAO, máscara, par de coordenadas, domínios numéricos, terminação e limite dos textos ativos, contagem e unicidade das chaves dos extras (sensíveis a maiúsculas).
- `Validate(PAYLOAD_SNAPSHOT)` verifica boot, contagem, aeronaves e unicidade dos ICAOs. Lista vazia é válida. Retorna o `STATUS` existente: `OK`, `INVALID_LENGTH` ou `INVALID_VALUE`, sem modificar a entrada.
- O receptor deve verificar limites antes de converter contagens ou preencher arrays. Dados acima do limite exigem falha explícita; truncar uma aeronave ou um campo produziria um snapshot incompleto. O tamanho máximo UART **não** é `sizeof` dessas estruturas: o contrato FCPU local admite 65536 bytes de JSON e 65562 bytes por DATA completo; o aceite do produtor permanece pendente.

Os arrays do modelo têm capacidade fixa. Na verificação de 05/11/2026, no compilador nativo e na toolchain Xtensa ESP32: `EXTRA_FIELD` = 98 bytes, `AIRCRAFT_STATE` = 3232 bytes e `PAYLOAD_SNAPSHOT` = 64672 bytes. Não alocar o snapshot na pilha padrão de uma tarefa FreeRTOS. O receptor atual reserva o snapshot e o quadro em dois blocos do heap. O fluxo integrado coordena sua publicação e reserva outros dois blocos para codificação/escrita quando o SD está disponível. Tamanhos do modelo não são limites de consumo total do firmware.

### Substituição e limites de evidência

O receptor publica a nova lista somente após receber e validar o snapshot completo. ICAOs ausentes saem da lista ativa; os registros históricos não são apagados. `messages: {}` válido limpa a lista. Entrada inválida, incompleta ou acima dos limites conserva o estado anterior. `Validate` não gerencia essa substituição.

Os campos de uma aeronave podem ter idades diferentes. `tick` não comprova atualização de todos os campos. Snapshots podem omitir mensagens ADS-B intermediárias; não satisfazem, por si, o registro/timestamp de cada mensagem. Essa incompatibilidade deve ser resolvida com Payload e coordenação sem alterar os critérios de validação existentes. `id` é tratado localmente como opaco. Aceite do produtor, unidades das velocidades, idade dos campos e parâmetros de tráfego/retenção permanecem abertos na INT-006; sequência/repetição do enlace têm implementação local no FCPU.

`PAYLOAD_DATA` continua sendo atualização individual com sessão/sequência.
`ttc::AIRCRAFT_DATA` continua sendo conteúdo de downlink e depende do contrato
TT&C. Nenhum desses tipos tem o mesmo contrato do snapshot. Os layouts e
comportamentos v1, o script do Payload e os testes anteriores foram preservados;
codec e leitor foram estendidos para v2.
[PayloadSnapshotCheck.cpp](../../obc-dh/tests/PayloadSnapshotCheck.cpp) verifica
somente os modelos; as verificações do codec/leitor v2 estão descritas abaixo.
COM-004 implementa parser/publicação em `PayloadReceiver` conforme o contrato
UART vinculado acima; `Validate` continua sem gerenciar a lista ativa.

## Persistência de snapshots — v2

Um snapshot completo ocupa um único registro, inclusive quando sua lista está vazia. Não serializar a memória de `PAYLOAD_SNAPSHOT`. Reutilizar little-endian, f64 IEEE 754 e CRC-32/ISO-HDLC do v1; `id` conserva os 64 bits e não identifica sessão, sequência ou deduplicação. Boot e instante OBC são independentes de `tick`.

| Offset | Campo | Bytes | Definição v2 |
|---:|---|---:|---|
| 0 | assinatura | 4 | ASCII `FCP2` |
| 4 | version | 1 | `2` |
| 5 | type | 1 | `8`, somente snapshot |
| 6 | total_bytes | 2 | Cabeçalho + corpo variável + CRC |
| 8 | boot_id | 8 | Boot OBC não zero |
| 16 | monotonic_ms | 8 | Instante OBC, inclusive zero |
| 24 | id | 8 | Identificador opaco do produtor, inclusive zero |
| 32 | aircraft_count | 1 | 0–20 aeronaves |
| 33 | aeronaves | variável | Ordem abaixo, sem padding |
| total_bytes − 4 | CRC | 4 | Mesmo algoritmo v1, cobrindo todos os bytes anteriores |

Cada aeronave contém, na ordem: ICAO u24, máscara u8, latitude/longitude/altitude f64 somente quando disponíveis (nessa ordem), identificação e `tick` somente quando disponíveis, contagem u8 dos extras e cada par chave/valor. Cada string contém um comprimento u8 seguido pelos bytes significativos, sem NUL ou padding. Cada extra grava comprimento/chave e depois comprimento/valor. Os limites e domínios são os do modelo acima: 16/40 bytes para identificação/`tick`, 32/64 por chave/valor e até 32 extras.

NUL dentro do comprimento declarado é inválido: o modelo C++ não representa sufixos significativos após esse byte. String disponível vazia é permitida; chave vazia continua inválida. Demais bytes são preservados, sem impor validação UTF-8 ou calendário que o modelo não possui. Chaves de extras e ICAOs precisam ser únicos dentro de suas coleções. Máscaras desconhecidas, campos fora do domínio e bytes sobrando no corpo são rejeitados.

Registro vazio: **37 bytes**. Uma aeronave somente com ICAO: **42 bytes**. Maior aeronave: **3223 bytes**; maior snapshot: `37 + 20 × 3223 = 64497 bytes`, dentro do comprimento u16. Isso não altera o limite v1 de 512 bytes nem os tamanhos de UART/LoRa. Leitor v1 não reconhece `FCP2`; usar o leitor atualizado para arquivos v2 ou mistos. Não migrar ou reescrever arquivos antigos automaticamente. Os tipos 1–7 permanecem v1; o tipo 8 usa a API de snapshot. A API dedicada de TM v3 usa o membro `telemetry` da união `RECORD`, sem incorporar o snapshot nessa união.

### Codec, memória e publicação

Em `obc_record`, `GetSnapshotSize(snapshot, total_bytes)` valida o modelo e calcula o tamanho exato. `EncodeSnapshot(snapshot, buffer, capacity, written)` valida modelo/capacidade antes da primeira escrita. `DecodeSnapshot(buffer, available, snapshot, consumed)` verifica cabeçalho, comprimento, CRC e todo o conteúdo antes de publicar a saída. Reutilizam o `STATUS` existente; contagens são zero em falha. `written`/`consumed` incluem cabeçalho e CRC. Fonte e destino não podem se sobrepor e a fonte deve permanecer estável durante a chamada.

O codec não faz I/O ou alocação dinâmica e não amplia `RECORD` para conter snapshots. O chamador fornece modelo e buffer, fora da pilha padrão das tarefas. Codificação copia apenas uma aeronave por vez. Decodificação usa uma passagem de validação e outra de publicação, com uma aeronave temporária e até 20 ICAOs, sem outro snapshot completo. Ausências e slots inativos são zerados na saída. Falha preserva o destino, inclusive quando o registro tem CRC válido e conteúdo inválido.

O objeto do modelo ocupa 64672 bytes; o buffer máximo codificado exige outros 64497 bytes. O fluxo integrado provisiona modelo e saída fora da pilha, usando dois blocos para a saída; falhas de reserva são explícitas. Com Xtensa `-Os -fstack-usage`, os quadros individuais medidos foram 3280 bytes em `WriteAircraft`, 3408 em `ReadSnapshotBody`, 80 em `EncodeSnapshot` e 96 em `DecodeSnapshot`. Esses valores não são o máximo total da cadeia de chamadas; dimensionar também a pilha da tarefa consumidora e o restante do firmware.

### Leitura, compatibilidade e verificações

O leitor atual aceita v1/v2/v3 no mesmo arquivo, com janela máxima de 64497 bytes e um modelo de snapshot no programa nativo. Reutiliza referências temporais v1 por boot, inclusive recebidas depois do snapshot. Exporta `version: 2`, `type: "payload_snapshot"` e `data: {"id": ..., "aircraft": [...]}` no envelope existente. Cada aeronave tem `icao`, `identification`, `lat`, `lon`, `alt`, `tick` e `extras: [{"key": ..., "value": ...}]`; ausências viram `null`. Textos disponíveis inválidos em UTF-8 viram `null` com campos `<nome>_encoding: "invalid_utf8"` e `<nome>_hex`, preservando os bytes como já ocorria com descrições v1. `tick` original não é sobrescrito pela reconstrução temporal.

Lista vazia é um snapshot válido; lista incompleta não é publicada. CRC/comprimento/conteúdo inválido causa descarte relatado por offset e busca do próximo `FCP1`, `FCP2` ou `FCP3`. Um candidato inválido não autoriza saltar seu comprimento declarado. Assinaturas dentro de um registro válido não produzem registros adicionais. A origem permanece intacta. Os códigos de saída continuam 0 para leitura íntegra, 1 para descarte/truncamento recuperável e 2 para erro de uso/I/O/recurso.

[SnapshotCodecCheck.cpp](../../obc-dh/tests/SnapshotCodecCheck.cpp) e [SnapshotRecordsCheck.py](../../obc-dh/tests/SnapshotRecordsCheck.py) comparam vetores independentes `struct`/`zlib`, verificam limites e saídas preservadas e executam o leitor real sobre arquivos mistos, corrompidos e truncados. Os testes v1 foram mantidos e passaram sem alteração de entradas/asserts/resultados esperados. Comandos e limites estão no [guia](../../obc-dh/tools/README.md). Os testes de codec/leitor não comprovam UART, cartão físico ou estados duráveis de atuação; a ligação UART–registros é responsabilidade da INT-006 e os estados operacionais ficam na INT-007/INT-008.

## 1. Convenções comuns

- Versão inicial: **1**. Incluir campo ou alterar largura, unidade, significado ou ordem do layout binário exige outra versão. Leitores v1 rejeitam versões/tipos desconhecidos; não presumem compatibilidade por comprimento.
- Inteiros sem sinal nas larguras indicadas, exceto `rssi` com sinal de 16 bits. Ordem little-endian. Reais: IEEE 754 binary32/binary64, também little-endian, sem reinterpretar ponteiros desalinhados. `float` e `double` têm representações verificadas na compilação.
- Comprimentos indicam bytes persistidos, sem padding, alinhamento, ponteiros ou NUL de strings C++. `sizeof(struct)` não define o arquivo.
- Campos opcionais usam máscara de disponibilidade no binário e `null` no exemplo JSON. Zero numérico é válido quando o bit está marcado. Na codificação, posição indisponível recebe bytes zero, mesmo que a RAM ainda contenha outro valor; na leitura, o bit determina a disponibilidade. Reais disponíveis devem ser finitos; NaN/Inf não representam `null`.
- Tipos internos não contêm filas, drivers ou relógio global. O chamador fornece os metadados e, quando aplicável, escolhe uma referência temporal explicitamente.
- `boot_id` é o contador confirmado de 64 bits do OBC, não zero. `monotonic_ms` é o instante de recepção validada de dados ou ocorrência do evento no OBC, em 64 bits. Não é a contagem de chamadas do loop.
- O horário de bancada do Payload permanece separado. Ausência de referência absoluta não transforma zero em UTC válido: conservar o instante relativo e usar o estado de [ObcTime](../../obc-dh/headers/ObcTime.h). Nenhuma referência permite inferir o intervalo desligado entre boots.
- Os exemplos JSON são lógicos, não dumps de memória ou esquemas de transporte. Os inteiros de 64 bits precisam de um leitor que preserve inteiros, como Python; não converter silenciosamente para `Number` de JavaScript acima de `2^53 - 1`.

## 2. Enquadramento do arquivo

Cada registro é independente e contém cabeçalho de **24 bytes**, corpo de tamanho fixo para o tipo v1 e CRC de **4 bytes**. Limite total de **512 bytes**, incluindo todas as partes. Não há padding entre registros. Esses valores não se aplicam ao quadro LoRa ou UART.

| Offset | Campo | Bytes | Definição |
|---:|---|---:|---|
| 0 | assinatura | 4 | ASCII `FCP1`, bytes `46 43 50 31` |
| 4 | version | 1 | `1` |
| 5 | type | 1 | ID da tabela abaixo |
| 6 | total_bytes | 2 | Cabeçalho + corpo + CRC, little-endian |
| 8 | boot_id | 8 | Identificação confirmada do boot OBC |
| 16 | monotonic_ms | 8 | Instante relativo no OBC |
| 24 | corpo | variável por tipo | Campos na ordem documentada |
| total_bytes − 4 | CRC | 4 | CRC-32/ISO-HDLC, little-endian |

CRC-32/ISO-HDLC: polinômio normal `0x04C11DB7`, refletido `0xEDB88320`, inicial `0xFFFFFFFF`, reflexão de entrada/saída e XOR final `0xFFFFFFFF`. Cobre assinatura, cabeçalho e corpo, excluindo os quatro bytes do CRC. Vetor de referência ASCII `123456789`: `0xCBF43926`. Algoritmo e codec estão implementados em `ObcRecordCodec`; o leitor nativo usa o mesmo decoder e procura registros íntegros após trechos inválidos. CRC não reconstrói bytes corrompidos nem garante recuperação do FAT32 após corte de energia.

| ID | Tipo lógico | Corpo | Total v1 |
|---:|---|---:|---:|
| 1 | mission | 77 | **105** |
| 2 | telemetry | 71 | **99** |
| 3 | event | 68 | **96** |
| 4 | fault | 68 | **96** |
| 5 | statistics | 56 | **84** |
| 6 | boot | 12 | **40** |
| 7 | time_reference | 8 | **36** |

No v1, tamanho diferente do canônico é inválido, mesmo quando menor que 512. O limite 512 não autoriza bytes extras em um tipo conhecido. O modelo C++ de cabeçalho deixa a assinatura implícita; os validadores verificam versão, tipo, tamanho e boot. O decoder verifica também assinatura, truncamento e CRC antes de publicar os dados; a busca de registros posteriores fica no leitor de arquivos.

## 3. Atualização do Payload — mission

**Formato individual v1 preservado:** esta seção não define o snapshot JSON atual nem obriga o produtor a emitir atualizações individuais. A integração não fabrica esse registro a partir de `id` ou de campos acumulados. A preservação de cada mensagem ADS-B continua pendente com Payload/coordenação.

Uma mensagem transporta **somente os campos recém-atualizados**. Se chegar velocidade, a posição em cache não é copiada para essa atualização. A agregação por aeronave e a idade dos valores acumulados pertencem à integração OBC; presença não prova precisão física ou o instante de aquisição no receptor ADS-B.

| Ordem no corpo | Campo C++ / JSON | Tipo persistido | Unidade/semântica |
|---:|---|---|---|
| 1 | session_id | u64 | Não zero, identifica a execução do produtor |
| 2 | seq_id | u32 | Sequência dentro da sessão, começa em zero |
| 3 | icao | u24 | RAM: u32 limitado a `0xFFFFFF`; JSON: seis hexadecimais maiúsculos |
| 4 | msg_type | u8 | Código numérico da origem, 1–255; exemplos atuais: 3/4 |
| 5 | available_fields | u8 | Máscara abaixo; omitida no JSON legível, derivada dos `null` |
| 6 | latitude / lat | f64 | Graus, −90 a +90 |
| 7 | longitude / lon | f64 | Graus, −180 a +180 |
| 8 | altitude / alt | f64 | Pés; permite zero e altitude negativa |
| 9 | velocity / vel | f64 | Nós, não negativa |
| 10 | heading | f64 | Graus, `0 <= heading < 360` |
| 11 | timestamp | 20 bytes | UTC local opcional do Pi: `YYYY-MM-DDTHH:MM:SSZ` |

Bits da máscara: 0 = latitude; 1 = longitude; 2 = altitude; 3 = velocidade; 4 = rumo; 5 = timestamp local. Bits 6/7 reservados e rejeitados. Latitude e longitude são um par: ambas disponíveis ou ambas indisponíveis. Exigir pelo menos um dado de voo disponível; timestamp sozinho não forma uma observação. A estrutura C++ usa 21 bytes para `timestamp`, incluindo NUL na RAM; o arquivo usa 20. Validar calendário gregoriano, anos 0001–9999 e segundos 00–59; sem frações, fusos alternativos ou segundos intercalares no v1.

`session_id` é opaco ao OBC: não é seu boot nem um horário Unix. O produtor deve gerar nova sessão a cada execução e antes de esgotar `seq_id`; não dar volta na sequência dentro da sessão. Retransmitir preserva sessão, sequência e conteúdo. O modelo v1 não implementa geração de sessões, retenção ou deduplicação. Suas regras não substituem as do FCPU, cuja sequência de transporte começa em 1; adoção de um produtor legado exige acordo explícito na INT-006. A validade do último `seq_id` é aceita pelo modelo para permitir fechar a sessão; não incrementar além dele.

Exemplo de posição, máscara binária `0x27`:

```json
{"version":1,"type":"mission","boot_id":1,"monotonic_ms":5000,"data":{"session_id":42,"seq_id":12,"icao":"E4953A","msg_type":3,"lat":-19.95438,"lon":-43.6116,"alt":39000,"vel":null,"heading":null,"timestamp":"2026-09-08T00:21:44Z"}}
```

Exemplo de velocidade, máscara binária `0x18`, sem reutilizar posição anterior:

```json
{"version":1,"type":"mission","boot_id":1,"monotonic_ms":5000,"data":{"session_id":42,"seq_id":13,"icao":"E4953A","msg_type":4,"lat":null,"lon":null,"alt":null,"vel":489,"heading":37,"timestamp":null}}
```

**Migração ainda não executada:** o script atual conserva cache, usa `msg_type` textual e um `val_flags` booleano que fica verdadeiro após altitude positiva e posição. Isso não equivale à máscara desta proposta. Também não se pode deduzir disponibilidade dos zeros iniciais do script. Preservar o script/testes existentes e coordenar uma implementação explícita do produtor e do adaptador UART antes da adoção. O uso de f64 no OBC não aumenta a precisão de um formato UART quantizado: conversões e erro de eventual migração do legado precisam de acordo na INT-006. Os 105 bytes do arquivo não são o tamanho proposto para a UART.

## 4. TM local — telemetry

Conservar os campos de `ttc::TELEMETRY_DATA`, com a máscara local adicional de u32. Os tipos portáteis evitam incluir ArduinoJson/headers dos drivers; não substituem os tipos da biblioteca TT&C. Na integração, mapear os campos explicitamente, sem reinterpretar memória. As chaves JSON abaixo são as do contrato TT&C; não incluem `n`. A máscara não é enviada pelo LoRa.

Ordem do corpo: `available_fields` u32; `timestamp` u32 (s Unix); `mode` u8; `vbat`, `ibat`, `vsolar`, `isolar` f32; cinco `temps` f32; `rate_dps`, `azimuth_deg` f32; `aircraft_count` u16; `message_count` u32; `rssi` i16; `snr` f32; `rx_count`, `error_count`, `radio_resets` u16. Total: 71 bytes.

| Bits | Campos C++ / JSON | Unidade |
|---|---|---|
| 0 | timestamp / ts | s Unix; zero válido se disponível |
| 1, 2 | vbat / vb; ibat / ib | V; mA |
| 3, 4 | vsolar / vs; isolar / is | V; mA |
| 5–9 | temps[0–4] / tp[0–4] | °C, disponibilidade independente |
| 10, 11 | rate_dps / w; azimuth_deg / az | °/s; graus |
| 12, 13 | rssi; snr | dBm; dB |

Bits 14–31 reservados. Modo obrigatório: 0 bench, 1 safe, 2 detumb, 3 sun, 4 mission, 5 lowpwr. `aircraft_count/ac`, `message_count/msg`, `rx_count/rx`, `error_count/err`, `radio_resets/rst` são contadores obrigatórios; zero significa contagem zero. Os três últimos preservam os 16 bits com volta definidos pelo TT&C. Se uma contagem ainda não estiver disponível, não fabricar zero: aguardar sua origem antes de formar este registro. Leituras numéricas disponíveis devem ser finitas; não aplicar limites físicos de EPS/ADCS/Térmico ainda não acordados.

Exemplo, máscara local `0x3FFF`:

```json
{"version":1,"type":"telemetry","boot_id":1,"monotonic_ms":5000,"data":{"ts":1760000123,"md":"mission","vb":7.81,"ib":420,"vs":9.1,"is":310,"tp":[21.5,22,23.1,20.9,24.3],"w":12.4,"az":37,"ac":3,"msg":1200,"rssi":-81,"snr":9.5,"rx":10,"err":0,"rst":0}}
```

Uma temperatura ausente vira `null` na posição correspondente do exemplo JSON e tem seu bit desmarcado. EPS fornece corrente em A: a integração deve converter explicitamente para mA. Não escolher aqui o significado de VIN+/VIN−, sinal da bateria, posições térmicas ou eixo ADCS. No envio TT&C, a máscara de disponibilidade acrescentada na INT-004 representa UTC desconhecido como `ts:null`.

### TM persistida com disponibilidade de contadores — v3

A primeira etapa da INT-006 acrescenta `FCP3`, versão **3**, tipo **9**,
para registrar a TM sem fabricar contagens indisponíveis. O cabeçalho de
24 bytes, a ordem/largura dos campos do corpo de 71 bytes e o CRC são os
mesmos mecanismos existentes; o total continua **99 bytes**. Não é um
novo pacote de rádio e não altera o formato v1 de telemetria.

Bits 0–13 mantêm o significado local. Os novos bits são:

| Bit | Campo disponível |
|---|---|
| 14 | `aircraft_count/ac` |
| 15 | `message_count/msg` |
| 16 | `rx_count/rx` |
| 17 | `error_count/err` |
| 18 | `radio_resets/rst` |

Bits 19–31 são rejeitados. Modo continua obrigatório. Valor zero com bit
marcado é válido; sem o bit, os bytes persistidos são zerados e o leitor
exporta `null`. Temperaturas conservam disponibilidade individual.
O validador v1 continua rejeitando seus bits reservados 14–31.

`EncodeTelemetryV3` e `DecodeTelemetryV3` são APIs dedicadas. Na saída do
decoder, `RECORD` conserva versão 3/tipo 9, com corpo `telemetry`. O leitor
atual aceita arquivos mistos v1/v2/v3, preservando a origem. Leitores antigos
não interpretam v3; para reverter o firmware, conservar o leitor atualizado
e os arquivos existentes. Não há migração ou reescrita automática.

No fluxo atual, `ac` só fica disponível após snapshot válido; lista vazia
produz zero disponível. `msg` permanece indisponível, pois `id` é opaco e
o produtor não informa a quantidade de mensagens ADS-B. A TM persistida
usa a mesma amostra de enlace devolvida por `SendTelemetry`; o JSON de
rádio conserva seus arredondamentos existentes.

### Gravação integrada e buffers partidos

`EncodeSnapshotParts` produz o mesmo registro v2 em dois blocos, sem cópia
intermediária. No ESP são 32.768 + 31.729 bytes, mantendo o máximo de
64.497 bytes. `EncodeSnapshot` contínuo e seus testes permanecem válidos.
Modelo e blocos devem permanecer estáveis e não se sobrepor.

O diário cria `boot_<boot hexadecimal de 16 posições>_<segmento de 8 posições>.bin`
exclusivamente, sem sobrescrever. Cada segmento começa com BOOT e, quando
existir, a última referência temporal confirmada, com metadados originais.
Rotaciona entre registros no limite `INT32_MAX`. Colisão ou falha terminal
desativa o escritor; recuperação automática pertence à INT-008.

O bloco de snapshot é exclusivo até terminar a escrita. A fila separada
tem oito registros pequenos de até 105 bytes, o maior tamanho canônico v1;
essa capacidade interna não muda o limite do formato v1. Cada operação
`Append` mantém suas tentativas/sincronização existentes. Um snapshot em
duas partes só incrementa `records_written` após confirmar ambas. Falha na
segunda parte pode preservar uma cauda incompleta; ela é contabilizada como
perda, o escritor para e o leitor relata o trecho sem publicar o snapshot.

SD indisponível, fila cheia ou bloco ocupado não bloqueiam recepção/TM:
os registros não gravados são contabilizados. ACK UART continua confirmando
RAM. `set_time` confirma aplicação em RAM; a falta de persistência é
diagnosticada separadamente. Não há retenção ilimitada nem confirmação de solo.

## 5. Operação, falhas, estatísticas e tempo

### event e fault

Corpo: `source` u8, `code` u16, `severity` u8, `description` de 64 bytes. Origem: 1 OBC, 2 Payload, 3 TT&C, 4 ADCS, 5 EPS, 6 Térmico, 7 SD, 8 I2C. Severidade: 0 info, 1 warning, 2 error, 3 critical. `event` aceita info/warning; `fault` aceita error/critical. Não transmitir valores desconhecidos dessas enumerações como se fossem válidos.

`code` é opaco, com significado definido por cada origem; catálogos dos produtores ainda precisam de aceite. Descrição contém até 64 bytes de texto, sem exigir mensagem não vazia. Na RAM são 65 bytes com NUL; no arquivo são 64 com padding zero após o fim do texto. A validação local verifica terminação/limite, não UTF-8. Os códigos dos exemplos abaixo são propostos, não ações já integradas.

```json
{"version":1,"type":"event","boot_id":1,"monotonic_ms":5000,"data":{"source":1,"code":1,"severity":0,"description":"startup"}}
{"version":1,"type":"fault","boot_id":1,"monotonic_ms":5000,"data":{"source":7,"code":1,"severity":2,"description":"sd unavailable"}}
```

### statistics

Sete u64, na ordem abaixo, cumulativos por boot OBC. Amostrar não zera contadores. A coleta integrada satura em `UINT64_MAX`, sem voltar a zero; os validadores apenas verificam o modelo e não implementam incremento. O motivo da saturação pode ser registrado como evento. Não converter esses contadores nos contadores limitados do downlink por cast silencioso.

| Campo | Definição proposta |
|---|---|
| received | Quadros candidatos completos do Payload entregues à validação, antes de rejeição/deduplicação; inclui inválidos e duplicados |
| invalid | Candidatos rejeitados por framing, integridade ou conteúdo; bytes parciais ainda aguardando não contam como candidato |
| gaps | Posições ausentes detectadas na sequência de uma sessão; retransmissão posterior não decrementa o total já detectado |
| duplicates | Candidatos válidos identificados como repetição do mesmo par sessão/sequência |
| records_written | Registros que o armazenamento confirmou escritos segundo a política COM-005; não comprova sobrevivência a corte de energia |
| storage_failures | Operações de armazenamento que retornaram falha; uma repetição pode gerar outra falha |
| records_dropped | Registros que o OBC deliberadamente deixou de encaminhar/persistir por falta de recurso; não soma automaticamente mensagens ausentes no enlace |

```json
{"version":1,"type":"statistics","boot_id":1,"monotonic_ms":5000,"data":{"received":100,"invalid":2,"gaps":3,"duplicates":1,"records_written":96,"storage_failures":1,"records_dropped":1}}
```

### boot

Corpo: `restart_count` u64 e `reset_reason` u32. `restart_count = boot_id - 1`, com as limitações de reservas confirmadas, falhas antes da reserva e apagamento/restauração de NVS descritas em [ObcTime.h](../../obc-dh/headers/ObcTime.h). `reset_reason` é o valor bruto de `esp_reset_reason_t`; zero representa motivo desconhecido. Sua aquisição pertence ao boot integrado. O modelo aceita qualquer u32 para preservar valores futuros do SDK.

```json
{"version":1,"type":"boot","boot_id":1,"monotonic_ms":0,"data":{"restart_count":0,"reset_reason":0}}
```

### time_reference

Reutilizar `obc_time::TIME_REFERENCE`. Sua recepção deve coincidir exatamente com o boot e o instante do cabeçalho. Corpo: `unix_s` u32 e `delay_ms` u32. Não repetir o `EVENT_STAMP` no corpo; ele já está no cabeçalho. Cada referência conserva a compensação utilizada, inclusive se o parâmetro global mudar em outra compilação.

```json
{"version":1,"type":"time_reference","boot_id":1,"monotonic_ms":5000,"data":{"unix_s":1760000000,"delay_ms":162}}
```

## 6. Validação local e pendências de aceite

[ObcDataCheck.cpp](../../obc-dh/tests/ObcDataCheck.cpp) verifica os exemplos, disponibilidade, limites de campos, tipos/tamanhos, datas e referências de boots. DAD-003 acrescentou [testes do codec](../../obc-dh/tests/ObcRecordCodecCheck.cpp) e [verificação do leitor C++](../../obc-dh/tests/ReadRecordsCheck.py), incluindo vetores independentes, CRC, truncamento, corrupção, recuperação e reconstrução temporal. Modelos/codecs não fazem I/O; o parser é implementado em PayloadReceiver e a primeira etapa INT-006 liga recepção, tempo e registros. Aceites e ensaios físicos continuam separados. O [guia do leitor](../../obc-dh/tools/README.md) registra uso, limitações e verificações; COM-005 conserva a responsabilidade de armazenamento físico.

Nos aceites encaminhados às integrações, Payload deve confirmar o mapeamento dos snapshots, limites/extras, semântica e idade dos campos, enquadramento e tráfego, preservando a pendência de cada atualização ADS-B intermediária. A descrição v1 de atualização individual é legado preservado, não uma exigência simultânea ao snapshot. TT&C/ADCS/EPS/Térmico devem confirmar disponibilidade, unidades, origem das leituras, estatísticas e códigos. A existência deste documento ou um build aprovado não declara compatibilidade entre setores.
