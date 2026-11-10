# TES-004 — Recepção UART: validação e limites

**Estado:** Concluída
**Responsáveis:** OBC-DH e Payload

## Encerramento do escopo próprio — 09/11/2026

A verificação de software do receptor FCPU v1 foi executada e aprovada com quadros independentes, -Werror, ASan e UBSan. Os casos, entradas e resultados estão registrados nesta tarefa e nos testes citados.

Por orientação do responsável, esta entrega é encerrada e arquivada em
`done/` no limite de sua responsabilidade. Aceites externos, integração e
ensaios restantes continuam obrigatórios nas destinatárias abaixo; não são
declarados aprovados por este encerramento. Os critérios, vetores, entradas
e resultados esperados originais permanecem preservados.

Menções a “em andamento” ou “não mover para done” no histórico abaixo
registram decisões anteriores a este encerramento. Se uma integração
revelar defeito na entrega, registrar correção ou reabrir a tarefa com
rastreabilidade, sem adaptar a validação para forçar aprovação.

### Encaminhamentos registrados no encerramento — 09/11/2026

A tabela conserva as obrigações e a situação registradas no encerramento. O andamento posterior é acompanhado nas destinatárias e contextualizado na seção de situação atual abaixo.

| Destinatária | Responsabilidade ainda aberta | Situação |
|---|---|---|
| [INT-006](../../03-integracao/INT-006-fluxo-de-dados.md) | Consolidar o contrato com o produtor; qualquer adaptação acordada exige regressão explícita, sem alterar resultados esperados para acomodar falhas. | Pendente na destinatária |
| [TES-005](../TES-005-uart-hardware.md) | Executar a interoperabilidade e falhas nas duas pontas reais; os casos nativos aprovados não encerram o ensaio físico. | Pendente na destinatária |
| [TES-009](../TES-009-falhas-e-recuperacao.md) | Executar o cenário físico de excesso/desconexão UART; a saturação simulada não o substitui. | Pendente na destinatária |

## Situação atual da integração — 10/11/2026

A implementação local continua FCPU e já é iniciada pela INT-006. Em 10/11, o responsável reafirmou snapshots JSON e descartou o payload binário compacto; a TES-005 preserva a rastreabilidade dessa substituição. Esse conteúdo JSON não comprova aceite dos controles binários FCPU no fio. Os testes e vetores desta tarefa permanecem preservados; interoperabilidade e falhas físicas continuam nas destinatárias.

## Objetivo

Verificar delimitação e interpretação do fluxo de bytes recebido.

## Entrega

Injetar mensagens válidas, parciais, consecutivas, inválidas, acima do limite e rajadas. Exercitar sequência/gaps, NACK, repetição, timeout e retomada do parser conforme contrato. Preservar o cenário de excesso UART e a semântica dos campos do Payload.

## Pré-requisitos

- **Tarefas:** [COM-004](../../01-comunicacao/done/COM-004-uart-payload.md)
- **Condições externas:** Contrato UART e seus resultados esperados definidos; não exige Raspberry Pi físico.

## Critérios de conclusão

Registrar casos, entradas, resultados esperados/observados e limites de buffer, sem adaptar critérios às falhas encontradas.

## Evidências

Os relatos, comandos, hashes e resultados a seguir descrevem as revisões datadas de cada entrega. Afirmações de ausência de integração ou de estado ainda em andamento nesse histórico não substituem o encerramento e os encaminhamentos atuais acima.

Verificação de software executada e aprovada conforme os casos abaixo.
O encerramento refere-se ao contrato local FCPU v1; interoperabilidade e
ensaios físicos continuam nas tarefas destinatárias.

### Verificação local — 09/11/2026

O receptor real `PayloadReceiver` passou em `PayloadReceiverCheck.cpp`, com
quadros independentes gerados por `PayloadReceiverCheck.py` usando `struct`
e `zlib`. Contrato implementado: [FCPU v1](../../../../common/protocol/payload-uart.md).
Comando completo no [guia nativo](../../../examples/README.md#teste-nativo-sem-raspberry-pi).

GCC 16.2.1, C++11, `-Wall -Wextra -Werror`, ASan e UBSan foram mantidos. O
LeakSanitizer executou fora do sandbox por sua incompatibilidade com ptrace.
ArduinoJson 7.0.4 recebeu apenas o patch versionado e aprovado de `nextId_`.
O teste de NUL embutido revelou falha de validação; o responsável aprovou a
rejeição antes da desserialização. As entradas e resultados esperados foram
preservados e a suíte completa passou após a correção.

| Grupo | Entradas e resultado esperado | Observado |
|---|---|---|
| Conteúdo | Amostra real de sete aeronaves, u64 máximo, campos opcionais e lista vazia; modelo válido e substituição integral | Aprovado |
| Fluxo | Fragmentos de um byte, vários quadros consecutivos e candidato válido dentro de quadro com CRC inválido; consumo correto e ressincronização | Aprovado |
| Limites | 20 aeronaves/32 extras e textos no limite aceitos; excesso recusado integralmente; JSON de 65536 bytes aceito, cabeçalho de 65537 recusado | Aprovado |
| Inválidos | Tipos, ICAO, NUL, sintaxe não JSON, trailing data e profundidade excessiva; NACK sem alterar snapshot anterior | Aprovado |
| Recursos | Documento que esgota o orçamento de 98304 bytes; NO_MEMORY e preservação integral da lista | Aprovado |
| Controle | Gap sem avanço, retransmissão após ACK perdido sem republicação, sequência reutilizada incompatível e sessão nova | Aprovado |
| Tempo/reset | Fronteiras de 250/8000 ms, tempo decrescente e reinicialização da referência por boot | Aprovado |

Respostas ACK/NACK foram comparadas byte a byte com vetores externos,
incluindo cabeçalho e CRC. As regressões de modelos/codecs/leitor anteriores
também passaram, sem alteração de seus vetores/asserts. Builds normal,
loopback e autoverificação ESP32 aprovados; isso não prova troca com o Pi.

Permanecem pendentes a consolidação de COM-004 com Payload e o fechamento
formal dos casos pelo contrato aceito. Esta evidência local não reescreve
os cenários existentes nem encerra a campanha de testes por inferência.

## Referências

- Requisitos relacionados: Preparação de REQ-OBC-005 e REQ-OBC-006. Implementação não equivale a requisito validado.
- [payload/README.md](../../../../payload/README.md)
- [obc-dh/.agents/REQUISITOS.md](../../../.agents/REQUISITOS.md)
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
