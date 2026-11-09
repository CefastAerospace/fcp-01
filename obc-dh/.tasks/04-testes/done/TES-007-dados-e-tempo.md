# TES-007 — Formatos e tempo: interpretação e limites

**Estado:** Concluída
**Responsáveis:** OBC-DH

## Encerramento do escopo próprio — 09/11/2026

A verificação portátil de formatos e tempo foi executada e aprovada com modelos, codecs, leitor real e simulação temporal. Precisão física, persistência NVS real e operação integrada possuem tarefas próprias.

Por orientação do responsável, esta entrega é encerrada e arquivada em
`done/` no limite de sua responsabilidade. Aceites externos, integração e
ensaios restantes continuam obrigatórios nas destinatárias abaixo; não são
declarados aprovados por este encerramento. Os critérios, vetores, entradas
e resultados esperados originais permanecem preservados.

Menções a “em andamento” ou “não mover para done” no histórico abaixo
registram decisões anteriores a este encerramento. Se uma integração
revelar defeito na entrega, registrar correção ou reabrir a tarefa com
rastreabilidade, sem adaptar a validação para forçar aprovação.

| Destinatária | Responsabilidade ainda aberta | Situação |
|---|---|---|
| [TES-008](../TES-008-integracao.md) | Medir precisão/resolução temporal real pelo LoRa e demonstrar timestamp no fluxo ADS-B–TM, além da coexistência dos recursos. | Pendente na destinatária |
| [TES-009](../TES-009-falhas-e-recuperacao.md) | Comprovar boots, persistência temporal e integridade de registros/estados em resets e interrupções reais, preservando os cenários existentes. | Pendente na destinatária |

## Objetivo

Comprovar codificação, interpretação e validade temporal sem depender de periféricos.

## Entrega

Exercitar registros válidos/incompletos/inválidos, unidades, tamanhos e interpretação reproduzível. Conferir ausência de sincronização, referências sucessivas, conversão ms/s, reset, identificação de boots, contagens e estouro do contador. Preservar timestamp do Payload e não inferir tempo desligado pela referência de outro boot.

## Pré-requisitos

- **Tarefas:** [DAD-003](../../02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md)
- **Condições externas:** Contratos e critérios temporais definidos; permite entradas simuladas.

## Critérios de conclusão

Casos e resultados reproduzíveis respeitam os contratos. Tempo simulado verifica lógica; precisão real do horário recebido pelo rádio exige [TES-008](../TES-008-integracao.md).

## Evidências

Verificação portátil executada e aprovada; detalhes e limites abaixo.
Não houve execução dos cenários físicos por esta evidência.

### Evidências consolidadas — 09/11/2026

Os executáveis reais e os vetores independentes já registrados em DAD-001,
DAD-002 e DAD-003 cobrem o escopo desta tarefa. Modelos, codecs v1/v2 e leitor
foram recompilados e aprovados nesta sessão; ObcTimeCheck foi repetido na
auditoria. Todos usaram C++11, -Wall -Wextra -Werror e UBSan, sem alterar os
arquivos de teste, entradas, asserts ou resultados esperados.

| Casos | Evidência executada | Observado |
|---|---|---|
| Tipos, unidades, disponibilidade, tamanhos, datas e limites | ObcDataCheck.cpp e PayloadSnapshotCheck.cpp | Aprovado |
| Registros válidos, todos os prefixos/capacidades dos vetores curtos, CRC, conteúdo inválido e saídas preservadas | ObcRecordCodecCheck.cpp e SnapshotCodecCheck.cpp, com vetores struct/zlib | Aprovado |
| Arquivos v1/v2/mistos, offsets, caudas truncadas, recuperação, origem preservada e interpretação JSON | ReadRecordsCheck.py e SnapshotRecordsCheck.py executando o leitor C++ | Aprovado |
| Sem sincronização, referências sucessivas/retroativas, ms/s, limites e referência de outro boot | ObcTimeCheck.cpp e testes do leitor | Aprovado |
| Boot inicial, reserva/commit simulados, falha de persistência, contagem e esgotamento u64 | ObcTimeCheck.cpp | Aprovado |

Resultados do leitor/codec observados: `Leitor C++: layouts, recuperação,
JSON, referências e preservação da origem aprovados.`, `Snapshot v2: vetores
independentes, limites e saidas preservadas aprovados.` e `Snapshot v2: leitor
misto, referencias, recuperacao e preservacao da origem aprovados.` O teste
temporal e os modelos terminaram com código zero, sem diagnóstico de UBSan.

Procedimentos dos codecs/leitor: [tools/README.md](../../../tools/README.md).
Repetição temporal dentro de obc-dh/:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcTime.cpp tests/ObcTimeCheck.cpp -o /tmp/fcp01-time-check
/tmp/fcp01-time-check
```

Tempo simulado valida lógica. Não comprova precisão real LoRa, NVS sob queda
de energia, integridade física do SD ou atuação; esses itens continuam em
TES-008/TES-009 conforme os encaminhamentos e os critérios originais.

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-OBC-006, REQ-OBC-010, REQ-NF-OBC-002, REQ-NF-OBC-003. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../../.agents/REQUISITOS.md)
- [common/protocol/ttc-json.md](../../../../common/protocol/ttc-json.md)
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
