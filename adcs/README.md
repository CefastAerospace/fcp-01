# ADCS — Attitude Determination and Control

Controle de atitude de 1 eixo do FCP-01: detumbling (60 → <5 rpm) e apontamento solar por sensores de luz (§3.6). Os algoritmos rodam no ESP32 do OBC; esta pasta guarda a bancada e os drivers.

## Cadeia

Motor **GM2804** + driver **SimpleFOC Mini** + roda de reação em anel de aço (~100 g, 80 mm, usinada na equipe) → absorve o momento da taxa inicial com ~51% de margem (§3.6).

| Recurso | Ligação | Observação |
|---|---|---|
| Fases U/V/W | GPIO 25 / 26 / 27 (PWM) | FOC com malha de velocidade + ângulo |
| EN do driver | GPIO 12 | Desliga fora do detumbling (zera idle) |
| Encoder | AS5600, I2C `0x36` | Feedback de posição do rotor |
| Taxa angular | MPU-9250, I2C `0x68` (±250 a ±2000 °/s) | Lei de detumbling a 20 Hz |
| Sol grosseiro | 2x BH1750 `0x23` / `0x5C` + baffles | Par diferencial p/ azimute |

## Contratos com o OBC

- Recebe comando lógico de manobra; devolve vetores/atitude e métricas de erro p/ TM (REQ-ADCS-005).
- Critérios: detumbling <10 s (primário) / <25 s / <60 s; erro de azimute ≤5° / ≤15° / ≤30° (§2.6).
- Consumo alocado ≤2,5 W (REQ-NF-ADCS-001).

## Aprofundar

- `../FCP-01_Design_Package_CubeDesign_2026.md` §§2.6, 3.1, 3.6, 8.3.3 + Apêndice A (REQ-ADCS).
- `../obc-dh/.agents/PENDENCIAS_ARQUITETURAIS.md` (P-ADCS: fases, encoder, limites).
