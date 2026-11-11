# Contrato JSON do enlace TT&C

Formato da camada de aplicação entre o satélite e a estação terrena, conforme o DP §7.1 ("JSON-formatted telemetry packets and telecommands"). Vale para o enlace LoRa e para o umbilical (UART0/USB-C).

Implementação de referência: `tt-c/include/TtcProtocol.h` (satélite, estação terrena e testes). Qualquer mudança aqui exige gravar o satélite e a estação terrena com a mesma versão.

## Regras gerais

- **Um objeto JSON por quadro.** No LoRa, cada quadro é um objeto; no umbilical, cada linha (terminada em `\n`) é um objeto. Linhas que não começam com `{` são logs e devem ser ignoradas.
- **Tamanho máximo de 255 bytes** por quadro, o FIFO do SX1276 (DP §7.1: "strictly sized so as not to exceed the physical FIFO buffer"). Os geradores de bordo limitam o corpo a 245 bytes para comportar o contador `n`.
- **Sem fragmentação.** Mensagens maiores não são enviadas; o OBC agrega os dados para caber (DP §5.3).
- **Integridade:** CRC de hardware do LoRa. Quadros corrompidos são descartados pelo rádio; a estação terrena retransmite TCs por timeout.
- **Chaves curtas**, sem espaços. Números em unidades SI ou indicadas abaixo; `null` indica valor indisponível.
- **Campo `t`** identifica o tipo do pacote.

## Subida: telecomando (TC)

```json
{"t":"tc","seq":12,"cmd":"set_time","args":{"unix":1760000000}}
```

| Campo | Tipo | Descrição |
|---|---|---|
| `t` | `"tc"` | Tipo. |
| `seq` | inteiro 0–65535 | Identificador do TC, escolhido pela estação terrena. Retentativas usam o mesmo `seq`. |
| `cmd` | string | Nome do comando (catálogo abaixo). |
| `args` | objeto, opcional | Argumentos, interpretados pelo OBC. |

**Validação no TT&C (envelope):** JSON válido, `t` = `"tc"`, `seq` em 16 bits, `cmd` no catálogo e `args` objeto. Se o envelope falhar e o `seq` for legível, o TT&C responde com NACK; sem `seq` legível, o quadro é descartado. O conteúdo de `args` é validado pelo OBC.

**Deduplicação:** o TT&C lembra os 8 TCs mais recentes por 30 s. Um TC com `seq` repetido não é reentregue ao OBC: se já foi respondido, o mesmo ACK é reenviado; se ainda está em execução, é ignorado. A estação terrena deve iniciar `seq` em valor não repetido a cada sessão (ex.: derivado do relógio).

A subida também leva a confirmação de entrega `{"t":"cf","id":[boot,seq]}`,
que não é TC ([detalhes](#entregas-identificadas-e-confirmação-do-solo)).

### Catálogo de comandos

| `cmd` | `args` | Ação (executada pelo OBC) |
|---|---|---|
| `ping` | — | Teste de enlace. |
| `set_mode` | `{"mode":"safe"}` (nomes de modo abaixo) | Muda o modo operacional. |
| `start_mission` | — | Inicia a janela de missão ADS-B. |
| `stop_mission` | — | Encerra a janela de missão. |
| `get_status` | — | Responde com ACK e uma TM. |
| `enter_safe` | — | Entra em SAFE. |
| `exit_safe` | — | Sai de SAFE / LOW-POWER (TC de recuperação, DP §2.5). |
| `request_tm` | — | Responde com ACK e uma TM. |
| `set_time` | `{"unix":1760000000}` | Acerta o relógio de bordo (s, unix). |
| `adcs_start` | — | Inicia o controle de atitude. |
| `adcs_stop` | — | Para o controle de atitude. |
| `deploy_antenna` | — | Abre a antena do TT&C pelo burn-wire (mecanismo do EPS, acionado pelo OBC). |
| `request_log` | `{"op":"list",...}` ou `{"op":"read",...}` (ver [request_log](#request_log)) | Lista ou lê arquivos fechados do SD. |
| `test_tm` | `{"count":1000}` (1–2000) | Só bancada: rajada de TMs para o teste do REQ-TTC-003. |
| `reset_obc` | — | Reinicia o OBC. |

## Descida

Todo pacote de descida recebe do TT&C o campo `n` (primeiro campo, inteiro 0–65535, com volta), atribuído no momento da transmissão. A sequência é estrita no ar e permite à estação terrena contar perdas (REQ-TTC-003). O OBC não preenche `n`.

### ACK / NACK

```json
{"n":40,"t":"ack","seq":12,"cmd":"set_time","st":"ok","q":3}
```

| Campo | Descrição |
|---|---|
| `seq`, `cmd` | Do TC respondido. |
| `st` | `ok`, `bad_json`, `invalid_cmd` (comando não suportado no estado/firmware), `invalid_param`, `state_rejected` ou `unknown_cmd`. |
| `q` | ms entre a recepção do TC pelo TT&C e a entrega ao OBC (evidência do REQ-TTC-004, ≤ 200 ms). Ausente em NACKs emitidos pelo próprio TT&C. |

ACKs têm prioridade de transmissão sobre os demais pacotes.

### Telemetria (TM)

```json
{"n":41,"t":"tm","ts":1760000123,"md":"mission","vb":7.81,"ib":420,"vs":9.1,"is":310,"tp":[21.5,22,23.1,20.9,24.3],"w":12.4,"az":37,"ac":3,"msg":1200,"rssi":-81,"snr":9.5,"rx":10,"err":0,"rst":0}
```

Conteúdo mínimo do HLR-COMM-02 / DP §8.3.1: status, energia, atitude e missão.

| Campo | Unidade | Origem | Descrição |
|---|---|---|---|
| `ts` | s (unix) | OBC | Tempo de bordo. |
| `md` | — | OBC | Modo: `bench`, `safe`, `detumb`, `sun`, `mission`, `lowpwr`. |
| `vb`, `ib` | V, mA | EPS | Tensão e corrente da bateria. |
| `vs`, `is` | V, mA | EPS | Tensão e corrente dos painéis (teste de carga). |
| `tp` | °C | TC | 5 temperaturas (LM75A). |
| `w` | °/s | ADCS | Taxa angular no eixo de controle. |
| `az` | ° | ADCS | Azimute do vetor solar. |
| `ac` | — | Payload | Aeronaves rastreadas. |
| `msg` | — | Payload | Mensagens ADS-B recebidas. |
| `rssi`, `snr` | dBm, dB | TT&C | Do último TC recebido. |
| `rx`, `err` | — | TT&C | TCs válidos e quadros rejeitados (16 bits, com volta). |
| `rst` | — | TT&C | Reinicializações do rádio. |

Os campos do TT&C são preenchidos pelo próprio TT&C (`Ttc::SendTelemetry`).

#### Disponibilidade no firmware de bancada OBC

`BuildTelemetry(tm, out, capacity, available)` e
`Ttc::SendTelemetry(tm, available)` aceitam uma máscara opcional de
`ttc::TELEMETRY_FIELD`. O padrão `TELEMETRY_ALL` conserva as chamadas
anteriores. Campos sem indicador na máscara conservam a chave e recebem
`null`; `TM_TEMPS` representa o conjunto das cinco temperaturas.
`SendTelemetry` continua preenchendo o enlace com as estatísticas reais,
respeitando a máscara também nesses campos.

A INT-004 publica `md:"bench"` e os contadores `rx`, `err` e `rst`.
UTC, energia, temperaturas, atitude e dados do payload ficam `null` até
suas integrações fornecerem dados válidos. RSSI/SNR ficam `null` até a
aplicação receber um TC de origem RF. As tentativas de recuperação do
rádio ausente incrementam `rst`; esse contador não indica que o rádio
está presente ou saudável.

`ping` recebe ACK. `get_status` e `request_tm` recebem ACK e uma TM sob
demanda; o ACK `ok` confirma que a TM foi enfileirada. Falha ao enfileirar
produz `state_rejected`. Esses três comandos aceitam ausência de `args`
ou objeto vazio; objeto não vazio recebe `invalid_param`. Demais comandos
catalogados recebem `invalid_cmd` nessa etapa. A deduplicação conserva o
comportamento existente: uma retentativa recebe o ACK anterior, sem
reexecutar o pedido de TM.

O mapeamento local usa as unidades da tabela acima. O aceite conjunto
dessas interfaces com o setor TT&C permanece registrado na INT-004.

#### Evolução na primeira etapa INT-006

O firmware integrado implementa `set_time`: `args` contém somente `unix`,
inteiro u32, inclusive zero. Valores ausentes, booleanos, negativos,
fracionários, textuais ou fora da faixa recebem `invalid_param`. O ACK
`ok` confirma aplicação em RAM; o registro da referência no SD é uma
operação separada, com falhas e descartes diagnosticados. Sem boot/fluxo
confirmado, a resposta é `state_rejected`.

A referência usa o instante `rx_ms` do TC expandido no boot monotônico de
64 bits. Compensação: 0 ms no cabo e os 162 ms existentes no RF; ambos
são estimativas, sem garantia física de precisão. Antes da referência,
`ts` fica `null`; depois, informa segundos Unix no instante da composição
da TM, inclusive zero válido. Resultado fora de u32 volta a ser indisponível.

`ac` passa a refletir a quantidade do último snapshot válido; lista vazia
produz zero e ausência de snapshot produz `null`. `msg` continua `null`,
pois `id` não fornece uma contagem ADS-B. Sensores seguem indisponíveis
até suas integrações. Não há TM periódica nova: o envio continua sob demanda.

`SendTelemetry(tm, available, queued_tm)` aceita saída opcional dos valores
efetivamente enfileirados, preservada em falha. Isso permite persistir a
mesma amostra de enlace sem segunda leitura. `request_log` e envio agregado
de aeronaves permanecem para a segunda etapa da INT-006; `request_log`
continua recebendo `invalid_cmd` neste firmware.

### Aeronave

Estado agregado por aeronave (DP §5.3); o log completo fica no microSD.

```json
{"n":42,"t":"ac","id":[3,41],"ts":null,"icao":"E4953A","lat":-34.57012,"lon":-58.52,"alt":30000,"vel":null,"hdg":null,"cnt":null}
```

| Campo | Unidade | Origem | Descrição |
|---|---|---|---|
| `id` | — | OBC | Opcional. Entrega identificada `[boot, sequence]`, inteiros de 1 a 2^64−1. |
| `ts` | s (unix) | OBC | Última mensagem da aeronave. |
| `icao` | — | Payload | Endereço ICAO (6 hexadecimais), sempre presente. |
| `lat`, `lon` | ° | Payload | Posição, 5 casas. Sempre juntas: ambas número ou ambas `null`. |
| `alt` | ft | Payload | Altitude. |
| `vel` | kt | Payload | Velocidade. |
| `hdg` | ° | Payload | Rumo. |
| `cnt` | — | Payload | Mensagens recebidas da aeronave. |

Campo indisponível conserva a chave com `null`; zero continua sendo um valor
medido. `BuildAircraft(aircraft, out, capacity, available, id)` recebe a máscara
`ttc::AIRCRAFT_FIELD` (`AC_TIMESTAMP`, `AC_POSITION`, `AC_ALTITUDE`,
`AC_VELOCITY`, `AC_HEADING`, `AC_COUNT`). Sem máscara e sem `id`, o pacote
mantém o formato anterior.

As unidades acima são as do contrato vigente. A INT-006 hoje fornece somente
latitude, longitude (graus) e altitude (pés); `ts`, `vel`, `hdg` e `cnt` saem
`null`. A origem exata de cada campo no Payload (altitude barométrica ou
geométrica, rumo verdadeiro ou de trilha, contagem por janela ou acumulada,
horário individual) continua para acordo com o Payload antes do uso
operacional desses campos.

#### Entregas identificadas e confirmação do solo

Com `id`, o pacote é uma entrega da INT-006
([obc-delivery.md](obc-delivery.md)). O ID é independente do contador `n`:
retentativas do OBC conservam ID, conteúdo e instante original, enquanto `n`
muda a cada transmissão.

A estação responde a cada recepção com uma confirmação na subida:

```json
{"t":"cf","id":[3,41]}
```

- **Aceite:** a estação só confirma depois de gravar o pacote no log da
  sessão e o ID, com `fsync`, no registro de entregas
  (`ground-station/app/state/deliveries.jsonl`). Se a gravação falhar, não
  confirma, e o OBC retransmite.
- **Repetida:** se o ID já está no registro, inclusive de uma sessão anterior,
  a estação reenvia a confirmação e não reaplica o conteúdo (aeronaves,
  rastro e mapa).
- **Persistência da deduplicação:** o registro é append-only e é carregado
  ao iniciar a estação. Uma linha cortada no fim do arquivo é ignorada. O
  modo demo usa um registro só em memória.
- **No satélite:** `cf` não é TC. Não tem `seq`, não recebe ACK nem NACK e
  não passa pela deduplicação de TCs. Com ID válido, vai para
  `Ttc::ReceiveConfirmation()`. Repetições também são repassadas, porque a
  confirmação é idempotente no OBC. `cf` malformado, com ID ausente, zero,
  negativo, fracionário, acima de 64 bits ou com chaves extras, é descartado
  e conta em `err`. Uma confirmação válida também atualiza `last_tc_ms`
  (contato com o solo).
- A estação não retransmite `cf` por conta própria. Se a confirmação se
  perder, o OBC retransmite a entrega e recebe outra.

#### Eventos de envio na biblioteca

| Chamada | Uso pelo OBC |
|---|---|
| `Ttc::SendAircraft(aircraft, available, id)` / `Ttc::SendDelivery(json, len, id)` | `true` indica que a entrega foi enfileirada. `false` é falha local imediata: ID inválido, pacote acima de 245 bytes, reservas esgotadas ou fila cheia. |
| `Ttc::ReceiveTransmitEvent(event, wait)` | Uma conclusão por entrega aceita, com o mesmo `id`. `success` indica que ao menos um meio terminou o TX; `dest`/`sent` dizem quais. Corresponde a `FinishDeliveryTransmit(id, success)`. |
| `Ttc::ReceiveConfirmation(id, wait)` | Confirmação do solo. Corresponde a `ConfirmDelivery(id)`. |

O evento de conclusão nunca se perde: `SendDelivery` reserva uma vaga na fila
de eventos (`ttc_tracked_slots` = 4) e a vaga só é liberada quando o OBC lê o
evento. Isso importa porque `Queue::BeginTransmit` do OBC não inicia nova
tentativa antes de `FinishTransmit`. Com as reservas esgotadas,
`SendDelivery` recusa a entrega até o OBC consumir os eventos.

#### Parâmetros de operação (proposta para acordo)

| Parâmetro | Valor no TT&C | Observação |
|---|---|---|
| Fila de descida comum | 16 pacotes | Compartilhada por TM, aeronaves, eventos e respostas `lg`, em FIFO. |
| Fila de ACK de TC | 4, prioritária | ACK de TC sai antes de qualquer pacote da fila comum. |
| Entregas pendentes de evento | 4 | O OBC usa 1 por vez. |
| Confirmações aguardando o OBC | 8 | Excesso é descartado e contado em `confirmations_dropped`. |
| Timeout de confirmação (OBC) | proposta ≥ 10 s | Precisa cobrir a fila comum cheia (16 × ~0,45 s em SF7) e a ida e volta (~0,4 s de aeronave + ~0,16 s de `cf` + estação). O valor final deve sair da medição na bancada. |
| Retentativas | OBC, sem limite | A mesma entrega continua até ser confirmada. A estação só reconfirma. |

Piores casos verificados nos testes nativos (`test/test_protocol`):

| Quadro | Pior caso |
|---|---|
| Aeronave com `id` de 2×20 dígitos, todos os campos e `n` | 186 B de 255 |
| `cf` com ID máximo | 59 B de 255 |
| Resposta `read` com 90 bytes, nome de 39 caracteres e `n` | 253 B de 255 |
| Resposta `list` | 3 nomes de 34 caracteres por quadro |

Falta medir quanto se preserva de cada alteração durante congestionamento
com o tráfego real.

### Evento e beacon

```json
{"n":43,"t":"ev","ts":1760000200,"ev":"low_power"}
{"n":44,"t":"bcn","ts":1760000300,"md":"safe","vb":7.2}
```

O beacon é periódico só em safe-mode (DP §7.2); a política nominal de transmissão é sob demanda.

## request_log

Lista e lê arquivos fechados do SD do OBC. O TT&C valida os argumentos
(`ttc::ParseLogRequest`) e monta as respostas (`BuildLogList`,
`BuildLogData`, `BuildLogError`); a execução cabe ao OBC.

```json
{"t":"tc","seq":20,"cmd":"request_log","args":{"op":"list","after":"boot_0000000000000003_00000001.bin"}}
{"t":"tc","seq":21,"cmd":"request_log","args":{"op":"read","name":"boot_0000000000000003_00000002.bin","off":180,"len":90,"to":"wired"}}
```

| Argumento | Operação | Descrição |
|---|---|---|
| `op` | ambas | `list` ou `read`. |
| `after` | `list` | Opcional. Cursor exclusivo: lista nomes depois deste, em ordem lexical. Ausente ou vazio começa do início. |
| `name` | `read` | Arquivo fechado. |
| `off` | `read` | Offset em bytes, u32. |
| `len` | `read` | Bytes pedidos, de 1 a 90 (`ttc::log_block_max`). |
| `to` | ambas | Opcional. Destino das respostas: `rf` ou `wired` (USB/umbilical). Sem `to`, as respostas usam o meio do TC. |

Nomes usam apenas `[A-Za-z0-9_.-]`, com até 39 caracteres, sem começar com `.`
e sem `..`. Argumento ausente, de tipo errado, fora da faixa ou chave
desconhecida recebe NACK `invalid_param`, sem resposta `lg`. Com `ok`, o OBC
enfileira uma resposta `lg` com `rq` igual ao `seq` do TC. Falha ao
enfileirar recebe `state_rejected`. Para responder pelo destino pedido, o OBC
usa `Ttc::Send(json, len, dest)`, com `dest = to` ou a origem do TC. Esse envio
vale mesmo com a cópia da descida no umbilical desligada.

Respostas:

```json
{"n":50,"t":"lg","rq":20,"op":"list","ls":[["boot_0000000000000003_00000002.bin",4096,0],["boot_0000000000000003_00000003.bin",68,1]],"end":false}
{"n":51,"t":"lg","rq":21,"op":"read","name":"boot_0000000000000003_00000002.bin","off":180,"tot":4096,"d":"RkNEMQEB..."}
{"n":52,"t":"lg","rq":22,"op":"read","name":"boot_0000000000000003_00000002.bin","off":4096,"tot":4096,"d":""}
{"n":53,"t":"lg","rq":23,"op":"read","name":"boot_0000000000000003_00000009.bin","off":0,"err":"not_found"}
```

- `ls`: itens `[nome, bytes, aberto 0|1]`, quantos couberem no quadro.
  `end:true` indica que não há mais arquivos, e `ls` vazio com `end:true`
  indica listagem vazia. O próximo cursor é o último nome recebido.
- `read`: `d` em base64. O offset seguinte é `off` + bytes decodificados.
  EOF corresponde a `off` igual a `tot` e `d` vazio. Bytes além de `tot` não
  são enviados. Diferença em relação à proposta de
  [obc-delivery.md](obc-delivery.md): o offset seguinte não é transmitido,
  porque pode ser derivado, e isso permite blocos de 90 bytes.
- `err`: `not_found` (arquivo ausente), `busy` (arquivo aberto ou SD ocupado),
  `range` (`off` maior que o tamanho), `io` (falha de leitura) ou `no_sd`
  (SD indisponível). Argumento inválido não chega aqui: recebe NACK.
- O cliente só avança depois de validar `rq`, `name`, `off` e `tot`
  constante. Em `busy` ou resposta perdida, repete o mesmo nome/offset com
  **seq novo**, porque o mesmo `seq` só recebe o ACK guardado, sem nova
  execução. A resposta `lg` pode chegar antes ou depois do ACK.
- A estação (`python -m fcp_gs logs list|get`) grava em `nome.part` e
  renomeia só no EOF consistente. A conferência dos CRCs dos registros fica
  com as ferramentas do OBC (`obc-dh/tools`), aplicadas ao arquivo
  reconstruído.

O despacho do OBC ainda responde `invalid_cmd` a `request_log`. A execução no
OBC (listagem, leitura e mapeamento de erros do SD) continua pendente na
INT-006.

## Estação terrena

A ponte ESP32 da estação terrena repassa cada quadro recebido ao notebook como uma linha:

```json
{"gs":{"rssi":-80,"snr":9.2,"ms":123456},"pkt":{...}}
{"gs":{...},"raw":"7b2274..."}
{"gs":{...},"err":"crc"}
```

`pkt` é o pacote recebido sem alteração; `raw` traz em hexadecimal um quadro que não é JSON válido; `err` indica quadro descartado pelo CRC do rádio. No umbilical, o notebook recebe os pacotes diretamente, sem o envelope `gs`.
