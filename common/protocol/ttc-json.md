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
| `deploy_antenna` | — | Aciona o burn-wire (OBC). |
| `request_log` | — | Solicita dados registrados. |
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

### Aeronave

Estado agregado por aeronave (DP §5.3); o log completo fica no microSD.

```json
{"n":42,"t":"ac","ts":1760000120,"icao":"E4953A","lat":-34.57012,"lon":-58.52,"alt":30000,"vel":450,"hdg":37,"cnt":120}
```

| Campo | Unidade | Descrição |
|---|---|---|
| `ts` | s (unix) | Última mensagem da aeronave. |
| `icao` | — | Endereço ICAO (6 hexadecimais). |
| `lat`, `lon` | ° | Posição, 5 casas. |
| `alt` | ft | Altitude. |
| `vel` | kt | Velocidade. |
| `hdg` | ° | Rumo. |
| `cnt` | — | Mensagens recebidas da aeronave. |

### Evento e beacon

```json
{"n":43,"t":"ev","ts":1760000200,"ev":"low_power"}
{"n":44,"t":"bcn","ts":1760000300,"md":"safe","vb":7.2}
```

O beacon é periódico só em safe-mode (DP §7.2); a política nominal de transmissão é sob demanda.

## Estação terrena

A ponte ESP32 da estação terrena repassa cada quadro recebido ao notebook como uma linha:

```json
{"gs":{"rssi":-80,"snr":9.2,"ms":123456},"pkt":{...}}
{"gs":{...},"raw":"7b2274..."}
{"gs":{...},"err":"crc"}
```

`pkt` é o pacote recebido sem alteração; `raw` traz em hexadecimal um quadro que não é JSON válido; `err` indica quadro descartado pelo CRC do rádio. No umbilical, o notebook recebe os pacotes diretamente, sem o envelope `gs`.
