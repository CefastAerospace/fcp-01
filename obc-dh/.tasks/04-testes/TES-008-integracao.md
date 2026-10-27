# TES-008 — Boot, comandos, telemetria e convivência

**Estado:** Pendente
**Responsáveis:** OBC-DH e setores

## Objetivo

Demonstrar o funcionamento conjunto por cenários independentes.

## Entrega

Executar cada cenário assim que suas dependências estiverem prontas, preservando os critérios de REQUISITOS.md. Registrar resultados individualmente. Não aguardar todas as integrações para iniciar boot ou TC/ACK.

## Pré-requisitos

- **Tarefas:** Por cenário, conforme tabela abaixo; não exigem conclusão conjunta para iniciar.
- **Condições externas:** Por cenário: montagem conferida, limites definidos e integrações correspondentes disponíveis.

## Definições e condições

- Preservar associação do teste TC a REQ-OBC-001, registrando relação com REQ-OBC-002 e evidência complementar de autonomia sem substituir o teste original.

## Cenários independentes

| Cenário | Pré-requisitos | Condições e verificação | Estado |
|---|---|---|---|
| Boot e reset sem atuação indevida de motor/Burn Wire | [INT-003](../03-integracao/INT-003-inicializacao-central.md) | Montagem e estados elétricos aprovados; conferir antes e depois da execução do firmware. | Pendente |
| TC/ACK e umbilical | [INT-004](../03-integracao/INT-004-ttc-e-umbilical.md) | Injetar TC e medir execução; preservar associação a REQ-OBC-001 e relação com REQ-OBC-002. Entrega ≤200 ms e resposta ≤2 s conforme TT&C. | Pendente |
| Convivência FOC, I2C, rádio e armazenamento | [INT-004](../03-integracao/INT-004-ttc-e-umbilical.md), [INT-005](../03-integracao/INT-005-sensores-e-adcs.md), [COM-005](../01-comunicacao/COM-005-armazenamento-sd.md) | Limites ADCS acordados; medir atendimento do controle sob operações de rádio e SD, sem presumir que SPI separado elimina concorrência. | Pendente |
| Fluxo ADS-B, TM e extração dos registros | [INT-006](../03-integracao/INT-006-fluxo-de-dados.md) | Injetar ADS-B simulado e demonstrar timestamp correto e legível na TM; inspecionar estrutura, integridade e ordenação de status/energia/atitude. Conferir registros e estatísticas extraídos. | Pendente |
| Referência temporal real via LoRa | [INT-006](../03-integracao/INT-006-fluxo-de-dados.md) | Medir erro/resolução e efeito da latência/quantização contra os limites temporais, incluindo período sem sincronização e resets. | Pendente |
| Modos, energia, térmico e Burn Wire | [INT-007](../03-integracao/INT-007-comandos-modos-e-protecoes.md) | Condições físicas e operacionais aprovadas. Conferir transições, recusa/duplicatas de comandos, retomada e consumo contra a cota EPS; preservar o mínimo de cinco ciclos bem-sucedidos do Burn Wire e confirmação física. | Pendente |

## Critérios de conclusão

Todos os cenários abaixo aprovados contra os critérios originais e limites acordados. Resultados parciais não encerram a tarefa; não substituem a demonstração de autonomia de [TES-010](TES-010-missao-e-cobertura.md).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-001, REQ-OBC-002, REQ-OBC-003, REQ-OBC-004, REQ-OBC-006, REQ-OBC-008, REQ-OBC-012, REQ-NF-OBC-001. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [tt-c/ENSAIOS.md](../../../tt-c/ENSAIOS.md)
- Origem no planejamento anterior: P-VALIDACAO.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
