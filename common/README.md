# Common — interfaces e protocolos compartilhados

Contratos entre subsistemas do FCP-01. Vale como referência oficial — atualizar aqui **antes** de integrar hardware novo.

| Caminho | Conteúdo |
|---|---|
| `interfaces/pinout.md` | Pinos, barramentos SPI/I2C/UART e endereços (§3.1 do Design Package) |
| `protocol/` | Spec do protocolo UART OBC ↔ Payload e dos pacotes TT&C |

[Contrato OBC v1 proposto](protocol/obc-data.md): modelos lógicos e layout persistente, ainda sujeitos ao aceite dos produtores. Os exemplos JSON não substituem a UART binária nem o contrato TT&C em uso.

[Codec e leitor OBC](../obc-dh/tools/README.md): implementação local de DAD-003, com exportação JSON Lines, identificação de corrupção e reconstrução temporal, sem alterar o contrato v1 proposto.

Regras da configuração atual: sem RTC externo, Payload por UART (não I2C), LoRa no VSPI e SD no HSPI. RTC só será reconsiderado se o método monotônico/referências LoRa não atender à validação temporal acordada. Divergências antigas estão obsoletas — ver histórico no git e [pendências de integração OBC](../obc-dh/.agents/TODO.md).
