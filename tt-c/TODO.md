# Plano de implementação do TT&C

Checklist do enlace TT&C do FCP-01, levantado a partir do Design Package (`DP_Master_EN.pdf`, seções 3.1, 3.5, 3.8, 4.2.4, 5.2, 5.3, 7.1–7.4 e 8.3) comparado ao código de `tt-c/`. Atualizado em 14/10/2026.

O DP já foi enviado ao comitê e é a referência normativa: em qualquer divergência, prevalece o que está no DP, e o código deve ser ajustado a ele.

Itens marcados como concluídos estão implementados e compilam; a validação em hardware é tratada à parte na seção 5. Decisões de equipe aparecem como **[DECISÃO]**.

## Decisões registradas

- **Camada de aplicação em JSON** (DP §7.1). Contrato em [`common/protocol/ttc-json.md`](../common/protocol/ttc-json.md).
- **Transmissão sob demanda**, sem duty cycle fixo: o OBC enfileira e o TT&C transmite assim que o rádio estiver livre (DP §5.3; decisão da equipe).
- **Burn-wire fora do TT&C.** O mecanismo é do EPS e o comando (`deploy_antenna`) é executado pelo OBC; o `tt-c` não referencia o burn-wire. Para a queima ocorrer fora de TX (DP §5.2), há o interlock genérico `Ttc::HoldTransmit()`.
- **LOW-POWER mantém o rádio em RX**, sem modo sleep: a saída do LOW-POWER exige um TC de recuperação (DP §2.5).
- **Integridade pelo CRC de hardware do LoRa** (DP §7.1). CRC16 de aplicação e fragmentação foram removidos.

## Estado atual

| Área | Evidência | Trabalho restante |
|---|---|---|
| Biblioteca do enlace | `TtcProtocol`, `Ttc`, `Sx1276Radio`, `UmbilicalLink`: SF7, JSON, TX intercalado com ACK prioritário, DIO0 por interrupção, task no Core 0, umbilical, deduplicação, verificação de saúde, beacon. | Validação em bancada (seção 5) e integração com o OBC (seção 6). |
| Estação terrena | `ground-station/` (ponte ESP32) e `ground-station/app/` (Python, interface web, logs, roteiros de verificação, análise pós-passagem). | Ensaio com o hardware; fotos e descrição para o DP. |
| Firmware de teste | `examples/link-test/`: simula o OBC (TM, aeronaves, `test_tm`, beacon em safe). | — |
| Testes | `test/test_protocol` (nativo, 13 casos), `test/test_ttc` (na placa, 9 casos), `ground-station/app/tests` (pytest, 14 casos). | Rodar `test_ttc` na placa. |

---

## 1. Conformidade com o DP

- [x] `radio_spreading` = 7 (DP §5.3) e `radio_tx_timeout_ms` ajustado ao airtime em SF7.
- [x] `Ttc::Update()` transmite no máximo um quadro por chamada, sem deixar a recepção surda em rajadas (REQ-TTC-004, HLR-COMM-01).
- [x] ACKs em fila prioritária.
- [x] Camada de aplicação em JSON (DP §7.1):
  - [x] Esquema de TC, ACK/NACK, TM, aeronave, evento e beacon em `common/protocol/ttc-json.md`.
  - [x] Todo pacote ≤ 255 bytes; o pior caso da TM é verificado nos testes nativos.
  - [x] TT&C como ponte: `Ttc::Send()` recebe o JSON pronto; `Ttc::SendTelemetry()` só completa os campos de enlace.
  - [x] Envelope do TC validado no TT&C (sintaxe, `seq`, `cmd`); `args` validado pelo OBC.
  - [x] ArduinoJson 7.
  - [x] Estação terrena em JSON, com detecção de perdas pelo contador `n`.
- [x] Campos da TM exigidos pelo HLR-COMM-02 e §8.3.1: modo, energia (bateria e painéis), 5 temperaturas, taxa angular, azimute solar, aeronaves, mensagens e enlace.
- [ ] Confirmar com o OBC, o EPS, o ADCS e o controle térmico as unidades e a origem de cada campo da TM.
- [x] Task do enlace no Core 0 com prioridade alta (`Ttc::Start()`).
- [x] Interrupção do DIO0 para RX/TX Done.

## 2. Enlace umbilical

- [x] TC/TM pela UART0 (`UmbilicalLink`), um JSON por linha; logs nunca começam com `{`.
- [x] Interface comum `LinkTransport`; o ACK volta pelo meio de origem do TC e cada meio tem seu contador `n`.
- [x] Estação terrena conecta direto ao satélite com `--wired`.

## 3. Funcionalidades

- [x] Beacon periódico (`Ttc::EnableBeacon()`), desligado por padrão e ligado só em safe-mode.
- [x] Interlock genérico de TX para cargas de alto consumo (`Ttc::HoldTransmit()`).
- [x] `LINK_STATS.last_tc_ms` para a detecção de perda de comunicação (REQ-OBC-011).
- [x] Verificação de saúde do SX1276 a cada 1 s (RegVersion e RegOpMode), com reinicialização pelo RST; contador `rst` na TM.
- [x] Deduplicação de TCs por `seq` (8 TCs, 30 s), com reenvio do ACK anterior.
- [x] Campos `flags` e `timestamp` do TC binário eliminados na migração para JSON.
- [ ] Beacon e enlace mínimo no firmware de safe-mode em partição separada (§7.2), junto com o OBC.

## 4. Estação terrena (DP §3.8 e §7.4)

- [x] Software no notebook (`ground-station/app/`), conectado à ponte ESP32 ou ao umbilical.
- [x] Log da sessão em JSON Lines, com horário do PC, RSSI e SNR (HLR-SW-02).
- [x] Interface web com TM, gráficos, aeronaves, TCs e log, sem dependência de internet.
- [x] Análise pós-passagem (`python -m fcp_gs analyze`): perdas, latências e CSVs.
- [ ] Testar a interface com a ponte e o satélite reais.
- [ ] Descrição final do software e fotos da estação para o DP (hoje "[PENDING]"); base em `ground-station/app/README.md`.

## 5. Verificação (DP §4.2.4 e §8.3.1)

Ferramentas prontas; falta executar em bancada e anexar os relatórios de `ground-station/app/reports/`.

- [ ] Rodar `pio test -e esp32` (test_ttc) na placa.
- [ ] REQ-TTC-001: dados simulados de Payload e ADCS chegando à estação terrena (`link-test`).
- [ ] REQ-TTC-002: medir a transmissão em 921 MHz e validar o modo umbilical (`--wired`).
- [ ] REQ-TTC-003: `python -m fcp_gs tm-loss --count 1000`, perda < 1,0%.
- [ ] REQ-TTC-004 e HLR-COMM-01: `python -m fcp_gs tc-latency --count 50`, `q` ≤ 200 ms e ida e volta ≤ 2 s.
- [ ] Deduplicação: reenviar o mesmo `seq` e confirmar que o comando não é reexecutado.
- [ ] Verificação de saúde: desligar o SX1276 com o satélite rodando e confirmar a reinicialização (`rst` sobe na TM).
- [ ] REQ-NF-TTC-001: medir consumo no EPS (15 mA nominal, 120 mA de pico).
- [ ] REQ-NF-TTC-002: pesar a placa do rádio e o conjunto da antena (< 85 g).
- [ ] Burn-wire (EPS/OBC): mínimo de 5 ciclos de abertura bem-sucedidos em solo.
- [ ] Link budget (DP §5.3 "[PENDING — link budget validation]").
- [ ] Confirmar que +17 dBm respeita o limite de potência da ENACOM.

## 6. Dependências externas

- [ ] Integração com o OBC: task do OBC consumindo `Ttc::ReceiveTelecommand()`, executando os TCs e respondendo com `Ttc::SendAck()` (`obc-dh/src/main.cpp` está vazio). O `examples/link-test` serve de referência.
- [ ] Acionamento do burn-wire (mecanismo do EPS, comando do OBC) fora de TX; `Ttc::HoldTransmit()` está disponível para isso.
- [ ] O SD do OBC deve usar uma instância própria de HSPI; o rádio usa o `SPI` global (VSPI).
- [ ] Alinhar o contrato JSON com o OBC-DH (parte TT&C de P-DADOS em `obc-dh/.agents/PENDENCIAS_ARQUITETURAIS.md`).
