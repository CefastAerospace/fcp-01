# Contrato de dados OBC v1 — proposta aos produtores

**Estado:** implementado localmente em DAD-001; aceite de Payload, TT&C, ADCS, EPS e Térmico pendente. Este documento não substitui automaticamente os contratos em uso. Referência C++: [ObcData.h](../../obc-dh/headers/models/ObcData.h); validação: [ObcData.cpp](../../obc-dh/src/ObcData.cpp).

**Atualização de implementação — 01/11/2026:** DAD-003 acrescentou o [codec portátil](../../obc-dh/headers/ObcRecordCodec.h) e o [leitor nativo](../../obc-dh/tools/README.md), com testes locais e build ESP32 aprovados. Esta revisão documental atualiza estado e referências; versão, layouts, tamanhos e exemplos do contrato permanecem iguais aos da proposta inicial. DAD-001/DAD-003 continuam em andamento pelas pendências registradas nas tarefas; não há integração automática ao rádio ou SD.

O contrato separa dados lógicos, transporte e persistência. O Payload continua com UART binária; o enquadramento UART, confirmação/retransmissão, limites de buffers, solicitações/respostas e geração de sessão pertencem a COM-004. O SD adota os registros binários abaixo. JSON neste documento é uma representação legível, sem ser um novo protocolo no ar. TT&C conserva seu [contrato JSON](ttc-json.md), incluindo unidades, `n` atribuído pelo enlace e limites de 245/255 bytes.

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

`session_id` é opaco ao OBC: não é seu boot nem um horário Unix. O produtor deve gerar nova sessão a cada execução e antes de esgotar `seq_id`; não dar volta na sequência dentro da sessão. Retransmitir preserva sessão, sequência e conteúdo. Métodos de geração sem reutilização, retenção de sessões, identificação de gaps e deduplicação precisam ser acordados em COM-004; os modelos não implementam essas operações. A validade do último `seq_id` é aceita pelo modelo para permitir fechar a sessão; não incrementar além dele.

Exemplo de posição, máscara binária `0x27`:

```json
{"version":1,"type":"mission","boot_id":1,"monotonic_ms":5000,"data":{"session_id":42,"seq_id":12,"icao":"E4953A","msg_type":3,"lat":-19.95438,"lon":-43.6116,"alt":39000,"vel":null,"heading":null,"timestamp":"2026-09-08T00:21:44Z"}}
```

Exemplo de velocidade, máscara binária `0x18`, sem reutilizar posição anterior:

```json
{"version":1,"type":"mission","boot_id":1,"monotonic_ms":5000,"data":{"session_id":42,"seq_id":13,"icao":"E4953A","msg_type":4,"lat":null,"lon":null,"alt":null,"vel":489,"heading":37,"timestamp":null}}
```

**Migração ainda não executada:** o script atual conserva cache, usa `msg_type` textual e um `val_flags` booleano que fica verdadeiro após altitude positiva e posição. Isso não equivale à máscara desta proposta. Também não se pode deduzir disponibilidade dos zeros iniciais do script. Preservar o script/testes existentes e coordenar uma implementação explícita do produtor e do adaptador UART antes da adoção. O uso de f64 no OBC não aumenta a precisão de um formato UART quantizado: conversões e erro do enlace precisam ser definidos em COM-004. Os 105 bytes do arquivo não são o tamanho proposto para a UART.

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

Uma temperatura ausente vira `null` na posição correspondente do exemplo JSON e tem seu bit desmarcado. EPS fornece corrente em A: a integração deve converter explicitamente para mA. Não escolher aqui o significado de VIN+/VIN−, sinal da bateria, posições térmicas ou eixo ADCS. Representação de UTC desconhecido no envio TT&C continua sendo assunto de integração: `SendTelemetry()` atual usa timestamp inteiro e não foi alterado para transmitir `null`.

## 5. Operação, falhas, estatísticas e tempo

### event e fault

Corpo: `source` u8, `code` u16, `severity` u8, `description` de 64 bytes. Origem: 1 OBC, 2 Payload, 3 TT&C, 4 ADCS, 5 EPS, 6 Térmico, 7 SD, 8 I2C. Severidade: 0 info, 1 warning, 2 error, 3 critical. `event` aceita info/warning; `fault` aceita error/critical. Não transmitir valores desconhecidos dessas enumerações como se fossem válidos.

`code` é opaco, com significado definido por cada origem; catálogos dos produtores ainda precisam de aceite. Descrição contém até 64 bytes de texto, sem exigir mensagem não vazia. Na RAM são 65 bytes com NUL; no arquivo são 64 com padding zero após o fim do texto. A validação local verifica terminação/limite, não UTF-8. Os códigos dos exemplos abaixo são propostos, não ações já integradas.

```json
{"version":1,"type":"event","boot_id":1,"monotonic_ms":5000,"data":{"source":1,"code":1,"severity":0,"description":"startup"}}
{"version":1,"type":"fault","boot_id":1,"monotonic_ms":5000,"data":{"source":7,"code":1,"severity":2,"description":"sd unavailable"}}
```

### statistics

Sete u64, na ordem abaixo, cumulativos por boot OBC. Amostrar não zera contadores. Futura coleta deve saturar em `UINT64_MAX`, nunca voltar a zero; os validadores apenas verificam o modelo e não implementam incremento. O motivo da saturação pode ser registrado como evento. Não converter esses contadores nos contadores limitados do downlink por cast silencioso.

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

[ObcDataCheck.cpp](../../obc-dh/tests/ObcDataCheck.cpp) verifica os exemplos, disponibilidade, limites de campos, tipos/tamanhos, datas e referências de boots. DAD-003 acrescentou [testes do codec](../../obc-dh/tests/ObcRecordCodecCheck.cpp) e [verificação do leitor C++](../../obc-dh/tests/ReadRecordsCheck.py), incluindo vetores independentes, CRC, truncamento, corrupção, recuperação e reconstrução temporal. Não há parser JSON de entrada, agregação ou acesso físico ao SD. O [guia do leitor](../../obc-dh/tools/README.md) registra uso, limitações e verificações; COM-005 conserva a responsabilidade de armazenamento físico.

Antes de encerrar DAD-001: Payload deve aceitar sessão/sequência, atualização sem cache, campos opcionais e adaptação explícita do legado; TT&C/ADCS/EPS/Térmico devem confirmar disponibilidade, unidades, origem das leituras e códigos. Confirmar também a definição das métricas e os códigos de origem. OBC poderá manter sua implementação local enquanto isso, mas não declarar compatibilidade entre setores por compilação ou por existência deste documento.
