# Common — interfaces e protocolos compartilhados

Contratos entre subsistemas do FCP-01. Vale como referência oficial — atualizar aqui **antes** de integrar hardware novo.

| Caminho | Conteúdo |
|---|---|
| `interfaces/pinout.md` | Pinos, barramentos SPI/I2C/UART e endereços (§3.1 do Design Package) |
| `protocol/` | Spec do protocolo UART OBC ↔ Payload e dos pacotes TT&C |

[Contrato OBC](protocol/obc-data.md): modelos de snapshots JSON do Payload, layouts persistentes v1 e snapshots binários v2, ainda sujeitos ao aceite dos produtores. Snapshots JSON completos na UART são a decisão de 05/11/2026; seu enquadramento UART permanece pendente; a persistência v2 já tem codec e leitor locais, sem gravação integrada no SD. O contrato TT&C foi preservado.

[Codec e leitor OBC](../obc-dh/tools/README.md): implementação local de DAD-003, com exportação JSON Lines, identificação de corrupção e reconstrução temporal, sem alterar o contrato v1 proposto.

Regras da configuração atual: sem RTC externo, Payload por UART (não I2C), LoRa no VSPI e SD no HSPI. RTC só será reconsiderado se o método monotônico/referências LoRa não atender à validação temporal acordada. Divergências antigas estão obsoletas — ver histórico no git e [pendências de integração OBC](../obc-dh/.agents/TODO.md).
