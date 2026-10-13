# PAYLOAD — ADS-B Air Surveillance System

Software de carga útil (*Payload*) desenvolvido pela equipe **CEFAST Aerospace** para a missão **CubeDesign 2026**.

Cadeia de voo (Design Package §6): antena fita 1090 MHz (face sul) → **RTL-SDR Blog V3** (USB) → **Raspberry Pi Zero W** (Raspberry Pi OS Lite, dump1090 MalcolmRobb, ganho 49,6, socket TCP 30003) → **UART dedicada GPIO 16/17 a 115,2 kbps** até o OBC (ESP32).

---

## Visão geral

O script `telemetry.py` consome o fluxo BaseStation do `dump1090` na porta `30003` e extrai os parâmetros de voo. Hoje loga JSON de bancada; o formato de voo (pacotes **binários compactos ~25–35 B** com **Sequence ID + NACK** via ring buffer em RAM — §6.4) está decidido e com validação pendente (§9.3-4).
Só dado processado voa para o OBC e para o solo; I/Q cru nunca sai do Pi (§6.3).

- **Janela de missão:** 10 min contínuos, até 20 aeronaves (§2.2).
- **Resiliência do Pi:** root read-only via OverlayFS, partição ext4 de dados em `/mnt/data`, watchdog Broadcom 15 s, Bluetooth desabilitado, módulos DVB bloqueados (§6.5). Detalhe de setup no `raspberry-pi-runbook.md` (não alterado por esta revisão).

---

## Parâmetros de missão (HLR-ADS-02)

| Parâmetro | Descrição |
| :--- | :--- |
| **`icao`** | Endereço ICAO da aeronave |
| **`lat` / `lon`** | Posição (latitude/longitude) |
| **`alt`** | Altitude barométrica (ft) |
| **`vel`** | Velocidade solo (kt) |
| **`heading`** | Track angle (quando disponível no dump1090) |
| **`seq_id`** | ID sequencial do pacote (detecção de gap + NACK) |
| **`timestamp`** | Associado pelo **OBC** em ms (timer interno ESP32; TC de início fixa o absoluto com correção retroativa — §6.4). O timestamp local em UTC gravado no log de bancada **não** é a referência de voo. |

---

## Log de missão e enlace

- **Bancada:** JSON Lines em `/mnt/data/mission_log.json` (exemplo abaixo) — formato de debug, não é o formato de voo.
- **Voo (OBC ↔ Payload):** binário compacto via UART (validação de bancada pendente — open issue §9.3-4).
- **Voo (satélite → solo):** TM LoRa agregada por aeronave (JSON ~150 B, §7.1); log completo fica no microSD do OBC para extração via umbilical (§5.3).

```json
{"icao":"E4953A","timestamp":"2026-09-08T00:21:44Z","lat":-19.95438,"lon":-43.6116,"alt":39000.0,"vel":489.0,"heading":37.0,"msg_type":"3","val_flags":true,"seq_id":12}
```
