# Common — interfaces e protocolos compartilhados

Contratos entre subsistemas do FCP-01. Vale como referência oficial — atualizar aqui **antes** de integrar hardware novo.

| Caminho | Conteúdo |
|---|---|
| `interfaces/pinout.md` | Pinos, barramentos SPI/I2C/UART e endereços (§3.1 do Design Package) |
| `protocol/` | Spec do protocolo UART OBC ↔ Payload e dos pacotes TT&C |

Regras da configuração atual: sem RTC externo, Payload por UART (não I2C), LoRa no VSPI e SD no HSPI. RTC só será reconsiderado se o método monotônico/referências LoRa não atender à validação temporal acordada. Divergências antigas estão obsoletas — ver histórico no git e [pendências de integração OBC](../obc-dh/.agents/TODO.md#2-pendências-e-dependências).
