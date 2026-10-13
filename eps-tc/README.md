# EPS-TC — Energia e Thermal Control

Subsistema de potência + monitoramento térmico do FCP-01 (§3.3, §4.2.7). Esta pasta guarda os drivers e a lógica de energia (lib consumida pelo `obc-dh/`).

## Cadeia

- **Armazenamento:** 2 células Li-ion 21700 em série (7,4 V, 37 Wh) + BMS HW-391 + balancer 2S.
- **Rails:** 3,3 V (OBC/sensores), 5 V (payload), 10 V (motor ADCS).
- **Geração:** 2 painéis 1,5 W via MPPT CN3722 (só precisa carregar — não alimentar cargas).
- **Operação:** RBF P10, chave de bateria, umbilical externo 9 V (bateria removida no TVAC).

## Contratos com o OBC (I2C 21/22)

- **3x INA219:** tensão/corrente/potência da bateria, rail principal e painéis (REQ-EPS-005/007).
- **5x LM75A:** temperaturas da bateria e pontos críticos (REQ-TC-001/004/005 + alerta REQ-TC-006).
- **LOW-POWER:** watchdog de energia/temperatura recolhe o satélite a tarefas críticas até recuperar (§2.5).

Endereços I2C finais e limites/períodos ainda estão em definição — ver `../common/interfaces/pinout.md` e `../obc-dh/.agents/PENDENCIAS_ARQUITETURAIS.md` (P-ENDERECOS, P-TERMICO).

## Aprofundar

- `../FCP-01_Design_Package_CubeDesign_2026.md` §§2.5, 3.1, 3.3, 8.3.2 + Apêndice A (REQ-EPS, REQ-TC).
