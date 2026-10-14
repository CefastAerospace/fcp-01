# Plano de implementação do TT&C

Checklist do enlace TT&C do FCP-01, levantado a partir do Design Package (`DP_Master_EN.pdf`, seções 3.1, 3.5, 3.8, 4.2.4, 5.2, 5.3, 7.1–7.4 e 8.3) comparado ao código de `tt-c/` em 14/10/2026.

O DP já foi enviado ao comitê e é a referência normativa: em qualquer divergência, prevalece o que está no DP, e o código deve ser ajustado a ele.

Os itens estão agrupados por prioridade. Decisões de equipe aparecem marcadas como **[DECISÃO]** e devem ser resolvidas antes da implementação correspondente.

## Estado atual

| Área | Evidência existente | Trabalho restante |
|---|---|---|
| Biblioteca do enlace | `TtcProtocol`, `Sx1276Radio` e `Ttc`: 921 MHz, BW 125 kHz, CRC em hardware, sync word, pinos VSPI conforme §3.1, filas TC/TX, NACK de CRC, fragmentação de dados, estatísticas do enlace, timeout de TX com reinicialização do rádio. | SF, intercalação TX/RX, umbilical, beacon, modos de energia, saúde do rádio, deduplicação de TC. |
| Estação terrena | `ground-station/`: ESP32 com CLI serial, remontagem de fragmentos e detecção de perda por `sequence_id`. | Software no notebook, logs estruturados, visualização e análise pós-passagem. |
| Firmware de teste | `examples/link-test/`: simula as respostas do OBC e envia JSON fictício de aeronaves. | Ferramentas para os critérios de verificação do DP. |
| Testes | Nenhum diretório `test/`. | Testes unitários nativos e roteiros de bancada com relatório. |

---

## 1. Conformidade com o DP (prioridade alta)

- [ ] Trocar `radio_spreading` de 9 para 7 em `TtcProtocol.h` (DP §5.3: "SF7 CONFIRMED"; REQ-TTC-003). Recalcular `radio_tx_timeout_ms` para o airtime de 255 bytes em SF7. Regravar a estação terrena com a mesma versão.
- [ ] Impedir que `Ttc::Update()` bloqueie a recepção: `FlushTransmit()` esvazia a fila inteira (até 16 quadros, ~24 s em SF9) sem consultar RX. Transmitir no máximo um quadro por `Update()` e checar RX entre eles. Atende REQ-TTC-004 (≤200 ms) e HLR-COMM-01 (≤2 s).
- [ ] Priorizar ACKs sobre TM e dados na fila de descida (fila separada ou envio na frente).
- [ ] Migrar a camada de aplicação para JSON, conforme o DP §7.1 ("JSON-formatted telemetry packets and telecommands"; TM de ~100–200 B, §5.3). Hoje TC, ACK e TM são structs binárias (`TELECOMMAND_PACKET`, `ACK_PACKET`, `TELEMETRY_PACKET`).
  - [ ] Definir o esquema JSON de TC, ACK/NACK e TM (nomes de campos, tipos e unidades) com o OBC e documentar como contrato compartilhado com a estação terrena.
  - [ ] Garantir que cada pacote JSON caiba no FIFO de 255 bytes do SX1276 (DP §7.1: "strictly sized so as not to exceed the physical FIFO buffer").
  - [ ] Tratar o TT&C como ponte de bytes (DP §7.1): o OBC serializa a TM e o TT&C apenas transfere o payload ao FIFO. Trocar `SendTelemetry(const TELEMETRY_PACKET&)` por uma API que recebe o JSON pronto.
  - [ ] Rever o CRC16 de aplicação: o DP atribui a detecção de erros ao CRC de hardware do LoRa. Decidir se o NACK `ERR_CRC` passa a ser só para JSON malformado.
  - [ ] Validar o JSON dos TCs recebidos (sintaxe, opcode e argumentos) antes de entregar ao OBC, ou repassar ao OBC a validação, conforme o REQ-TTC-004 ("forward them to the OBC").
  - [ ] Avaliar a biblioteca de JSON (ex.: ArduinoJson) quanto a uso de memória e tempo de serialização no ESP32.
  - [ ] Adaptar a estação terrena para enviar TCs e interpretar ACKs/TM em JSON.
  - [ ] Revisar o rastreamento de perdas da estação terrena, que hoje depende do `sequence_id` binário; manter um contador de sequência dentro do JSON.
- [ ] Completar o conteúdo da TM exigido pelo HLR-COMM-02 e §8.3.1 (status, energia, atitude e missão), em acordo com o OBC:
  - [ ] Modo operacional (SAFE/INIT, DETUMBLING, SUN-POINTING, MISSION, LOW-POWER, UMBILICAL).
  - [ ] Atitude: taxa angular e azimute solar estimado.
  - [ ] Missão: aeronaves rastreadas e mensagens recebidas.
  - [ ] Temperaturas do controle térmico (5× LM75A).
  - [ ] Tensão e corrente de carga solar (teste intermediário de carga avalia corrente e tensão pela TM).
- [ ] Executar a task do enlace no Core 0 com prioridade alta (DP §7.1 e §7.2). Hoje `link-test` usa o Core 1 com prioridade 2. Documentar no `Ttc.h` ou expor uma função que cria a task.
- [ ] Usar a interrupção do DIO0 (RX/TX Done, DP §3.1) para acordar a task por notificação, em vez de polling a cada 10 ms.

## 2. Enlace umbilical (prioridade alta)

- [ ] Implementar TC/TM pela UART0 (USB-C), pontuado separadamente na competição (TC 0,5 + TM 0,5) e exigido na verificação do REQ-TTC-002.
- [ ] Abstrair o transporte para que o mesmo quadro TC/ACK/TM trafegue por LoRa ou por serial (`LoraRadio` já é uma interface).
- [ ] Adaptar a estação terrena para operar pelo cabo, no mesmo formato.

## 3. Funcionalidades ausentes

- [ ] Beacon autônomo periódico (DP §4.2.4 "tracking beacon"; §7.2 safe-mode com beacon).
- [ ] Burn-wire da antena (GPIO 33, `CMD_DEPLOY_ANTENNA`):
  - [ ] **[DECISÃO]** Definir o responsável pelo driver (TT&C, OBC ou Estruturas/EPS); ver `obc-dh/.agents/TODO.md` e `obc-dh/headers/Pinout.h`.
  - [ ] Acionar somente fora de TX (transiente de 2,5 A / 12,5 W em 5 V).
  - [ ] Limitar o tempo de queima (aguarda validação em bancada).
  - [ ] Permitir múltiplas tentativas espaçadas por minutos.
  - [ ] Garantir saída desligada no boot e após reset.
- [ ] API de modos de energia do rádio (sleep/standby) para o modo LOW-POWER (§2.5) e o REQ-NF-TTC-001 (15 mA nominal, 120 mA de pico).
- [ ] Registrar em `LINK_STATS` o instante do último TC válido, para a detecção de perda de comunicação (REQ-OBC-011).
- [ ] Verificar periodicamente a saúde do SX1276 (registrador de versão e modo de operação) e reinicializar via RST se travado; `IsHealthy()` hoje só retorna um flag.
- [ ] Deduplicar TCs por `sequence_id` para não reexecutar comandos retransmitidos pela estação terrena; reenviar o ACK anterior.
- [ ] Definir o uso dos campos `flags` e `timestamp` do TC, ou removê-los.
- [ ] Beacon e enlace mínimo no firmware de safe-mode em partição separada (§7.2), junto com o OBC.

## 4. Estação terrena (DP §3.8 e §7.4)

- [ ] Software no notebook (ex.: Python) conectado ao ESP32 da estação terrena pela serial.
- [ ] Log estruturado de TM, ACKs e dados em arquivo (CSV/JSON), com horário de recepção, RSSI e SNR (HLR-SW-02).
- [ ] Visualização da TM durante a passagem.
- [ ] Análise pós-passagem: perda de pacotes, latência e dados da missão.
- [ ] Descrição final do software e fotos da estação para o DP (hoje "[PENDING]").

## 5. Verificação (DP §4.2.4 e §8.3.1)

- [ ] REQ-TTC-001: injetar dados simulados de Payload e ADCS e validar a recepção.
- [ ] REQ-TTC-002: medir a transmissão em 921 MHz e validar o modo umbilical.
- [ ] REQ-TTC-003: transmitir 1000 pacotes de TM e comprovar perda < 1,0%. A estação terrena precisa totalizar as perdas e gerar relatório.
- [ ] REQ-TTC-004: enviar 50 TCs distintos e comprovar entrega ao OBC em ≤ 200 ms. Instrumentar a latência a bordo, da recepção até a entrega na fila.
- [ ] HLR-COMM-01 / REQ-OBC-002: comprovar ≤ 2 s ponta a ponta até a confirmação de execução. Modo em lote na estação terrena com estatísticas.
- [ ] REQ-NF-TTC-001: medir consumo no EPS (15 mA nominal, 120 mA de pico).
- [ ] REQ-NF-TTC-002: pesar a placa do rádio e o conjunto da antena (< 85 g).
- [ ] Burn-wire: mínimo de 5 ciclos de abertura bem-sucedidos em solo.
- [ ] Testes unitários nativos (`pio test -e native`) de CRC, enquadramento e fragmentação, com um `LoraRadio` falso.
- [ ] Link budget (DP §5.3 "[PENDING — link budget validation]").
- [ ] Confirmar que +17 dBm respeita o limite de potência da ENACOM.

## 6. Dependências externas

- [ ] Integração com o OBC: task real, filas e execução/validação dos TCs (`obc-dh/src/main.cpp` está vazio).
- [ ] O SD do OBC deve usar uma instância própria de HSPI; o rádio usa o `SPI` global (VSPI).
- [ ] Alinhar com o OBC o preenchimento dos campos da TM (seção 1).

## Ordem sugerida

1. SF7 e intercalação TX/RX (seção 1, dois primeiros itens).
2. Enlace umbilical (seção 2).
3. Ferramentas de verificação (seção 5) e estação terrena no notebook (seção 4).
4. Burn-wire, após definir o responsável.
5. Migração para JSON e conteúdo da TM, após fechar o esquema com o OBC.
6. Demais itens da seção 3.
