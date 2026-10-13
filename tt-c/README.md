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
| `src/` + `include/` | Driver SX1276 + protocolo (lib do OBC) |
| `ground-station/` | Firmware da estação terrena de bancada (2º ESP32 + SX1276) |
| `examples/link-test/` | Teste do enlace no lado do satélite |

## Aprofundar

- `../FCP-01_Design_Package_CubeDesign_2026.md` §§2.6, 3.1, 3.5, 5.3, 7.1 + Apêndice A (REQ-TTC).
