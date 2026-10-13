# Common — interfaces e protocolos compartilhados

Contratos entre subsistemas do FCP-01. Vale como referência oficial — atualizar aqui **antes** de integrar hardware novo.

| Caminho | Conteúdo |
|---|---|
| `interfaces/pinout.md` | Pinos, barramentos SPI/I2C/UART e endereços (§3.1 do Design Package) |
| `protocol/` | Spec do protocolo UART OBC ↔ Payload e dos pacotes TT&C |

Regras: sem RTC externo, Payload por UART (não I2C), LoRa no VSPI e SD no HSPI. Divergências antigas estão obsoletas — ver histórico no git e `../obc-dh/.agents/PENDENCIAS_ARQUITETURAIS.md`.
