# TT&C — Telemetry, Tracking and Command

Enlace com o solo do FCP-01: firmware do rádio no OBC + estação terrena espelhada da equipe (§§3.5, 3.8, 7.1).

## Cadeia

- **Rádio:** LoRa SX1276 no VSPI (23/19/18, CS 5, DIO0 4, RST 14), **921 MHz SF7**, JSON ~150 B.
- **Antena:** fita de aço ¾-onda (face norte) + burn-wire nichrome no GPIO 33 — **sem servo**.
- **Backup:** umbilical USB-C (UART0) p/ TC/TM em bancada.

## Contratos

- OBC enfileira TM (`filaTM`) e o TT&C transmite quando o rádio libera; TC recebido é validado e executado em ≤2 s (§§2.6, 7.2).
- Downlink da missão é agregado por aeronave; log completo fica no SD (§5.3).
- Deploy da antena por telecomando (≥10 cm), fora de atividade TX, com ciclos de queima validados em solo (§8.3.2).

## Subpastas

| Pasta | Conteúdo |
|---|---|
| `src/` + `include/` | Driver SX1276, enlace umbilical e módulo `Ttc` (lib do OBC) |
| `ground-station/` | Firmware da ponte da estação terrena (2º ESP32 + SX1276) |
| `ground-station/app/` | Software do notebook: interface web, logs, testes de verificação |
| `examples/link-test/` | Teste do enlace no lado do satélite, simulando o OBC |
| `test/` | Testes do protocolo (nativo) e do `Ttc` (na placa) |

Contrato JSON: [`../common/protocol/ttc-json.md`](../common/protocol/ttc-json.md). Pendências: [`TODO.md`](TODO.md).

## Diagnóstico do proprietário

`Ttc::GetDiagnostics()` retorna inicialização, tarefa criada, execução de
`Update`, contador de conclusões, última disponibilidade observada do rádio,
contatos RF/cabo e margem histórica da pilha da tarefa. A cópia usa o lock
existente; a varredura da pilha ocorre fora dele, sem acesso ao rádio/UART.
Contato válido possui indicador próprio, inclusive para millis zero.
`Update` manual continua disponível sem indicar tarefa criada.

Essa observabilidade permite ao OBC distinguir silêncio, indisponibilidade
do rádio e falta de progresso. Não muda protocolo, filas, prioridades ou
política de recuperação do SX1276. `radio_resets` conserva o significado
do contador existente de tentativas da verificação periódica; não comprova
saúde ou presença do rádio. Teste adicional: `pio test -e esp32-diagnostics`.
Procedimentos integrados no [guia OBC](../obc-dh/tools/README.md#supervisão-de-recursos-ttc-e-memória--15112026).

## Aprofundar

- `../FCP-01_Design_Package_CubeDesign_2026.md` §§2.6, 3.1, 3.5, 5.3, 7.1 + Apêndice A (REQ-TTC).
