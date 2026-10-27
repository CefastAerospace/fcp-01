# Organização das tarefas OBC-DH

O [TODO](../.agents/TODO.md) é o painel unificado. Os arquivos nesta pasta são a fonte do escopo, dependências, condições externas e evidências. O painel e cada tarefa devem manter o mesmo estado.

## Estrutura e uso

- `01-comunicacao/`: serviço I2C, prioridade, recuperação, UART e acesso ao SD.
- `02-dados-e-tempo/`: modelos, tempo e representação/interpretação dos registros.
- `03-integracao/`: ambiente, boot, setores, fluxo de dados, modos e supervisão.
- `04-testes/`: verificações de software, interfaces físicas, integração e missão.

Cada categoria contém `done/`. Arquivos pendentes ficam na própria categoria; os concluídos ficam em `done/`, conservando ID e nome. A pasta de dados/tempo ainda não possui conclusões e usa apenas `.gitkeep` para preservar o diretório no Git. Esta convenção substitui `.agents/tasks/active/` e `.agents/tasks/completed/` por decisão do responsável nesta reorganização.

Não ler todas as tarefas em cada trabalho. Consultar o painel e abrir somente a tarefa solicitada e as dependências necessárias. Referências de arquitetura, requisitos, pinagem e ferramentas permanecem em `.agents/`; não duplicar material de estudo nas tarefas.

## Dependências e estados

Uma tarefa fica disponível quando todas as tarefas predecessoras estiverem concluídas e suas condições externas resolvidas. A elaboração de contratos e o levantamento de parâmetros podem avançar antes disso. O responsável pela condição externa aparece no arquivo; registrar decisão, responsável, referência e unidades quando resolvê-la.

Pendente significa não iniciada, mesmo se ainda houver dependências. Em andamento significa execução iniciada. Bloqueada registra um impedimento encontrado durante a execução, com motivo e responsável. Concluída exige os critérios do escopo atendidos e evidências registradas. Não há automação de estados; atualizar painel e tarefa juntos.

[TES-008](04-testes/TES-008-integracao.md) e [TES-009](04-testes/TES-009-falhas-e-recuperacao.md) possuem cenários independentes: iniciar um cenário depende apenas dos IDs e condições daquela linha. A tarefa inteira só termina quando todos os seus cenários terminarem. Não criar dependência circular entre implementação e teste; uma tarefa de implementação conserva sua verificação local, enquanto os ensaios vinculados comprovam propriedades adicionais.

## Conteúdo e encerramento

Cada arquivo contém estado/responsáveis, objetivo, entrega, pré-requisitos, critérios de conclusão, evidências e referências. Definições e condições detalham somente o trabalho correspondente. Testes integrados incluem sua tabela de cenários.

Ao encerrar: registrar versão, procedimento, entradas, resultados e limitações; marcar o estado; mover o arquivo para `done/`; corrigir seus links relativos e todos os links que apontam para ele; atualizar a linha no TODO. Não renumerar tarefas, nem marcar tarefas dependentes automaticamente. Se uma tarefa precisar ser reaberta, fazer o percurso inverso mantendo o histórico das evidências.

Implementação, integração e validação física são estados distintos. As cinco tarefas em `done/` nesta revisão são conclusões históricas de escopo limitado, não funcionalidades integradas. Nenhum resultado de ensaio foi inferido pela existência de código.

## Histórico e rastreabilidade

Reorganização de 27/10/2026 a partir do TODO do commit `423dacb`. O inventário de 07/10, a reorganização de 11/10 e a consolidação de interfaces de 14/10 permanecem como histórico documental. O serviço I2C foi acrescentado em `bdfa48f` (20/10); o teste nativo aprovado em 27/10 está em [TES-001](04-testes/done/TES-001-escalonamento-i2c.md). A presente migração não executa novamente os ensaios nem comprova hardware.

O I2C anteriormente cogitado para o Payload foi substituído por UART. A proposta `0x80` e a decisão encerrada continuam no [histórico dos requisitos](../.agents/REQUISITOS.md#histórico-de-decisões-de-interface). O contrato vigente é binário compacto com SeqID/NACK; JSON do Payload é bancada. LoRa usa VSPI, SD usa HSPI e sensores/encoder usam I2C. Não há RTC externo na configuração atual.

As pendências antigas foram distribuídas sem alterar decisões ou critérios. Os IDs P-* abaixo existem apenas para rastrear sua origem; os IDs operacionais são COM/DAD/INT/TES.

| Pendência anterior | Destinos |
|---|---|
| P-PAYLOAD | [COM-004](01-comunicacao/COM-004-uart-payload.md) |
| P-BARRAMENTO | [COM-001](01-comunicacao/done/COM-001-servico-i2c.md), [COM-002](01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md), [COM-003](01-comunicacao/COM-003-recuperacao-i2c.md), [TES-001](04-testes/done/TES-001-escalonamento-i2c.md), [TES-003](04-testes/TES-003-i2c-hardware.md), [INT-005](03-integracao/INT-005-sensores-e-adcs.md) |
| P-ENDERECOS | [INT-005](03-integracao/INT-005-sensores-e-adcs.md), [DAD-002](02-dados-e-tempo/DAD-002-tempo-e-boots.md) |
| P-DADOS | [COM-004](01-comunicacao/COM-004-uart-payload.md), [DAD-001](02-dados-e-tempo/DAD-001-modelos-e-formatos.md), [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md) |
| P-TEMPO | [DAD-002](02-dados-e-tempo/DAD-002-tempo-e-boots.md) |
| P-BURN-WIRE | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) |
| P-ADCS | [INT-005](03-integracao/INT-005-sensores-e-adcs.md), [INT-002](03-integracao/done/INT-002-pinagem-en-motor.md), [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) |
| P-SPI | [COM-005](01-comunicacao/COM-005-armazenamento-sd.md), [INT-004](03-integracao/INT-004-ttc-e-umbilical.md) |
| P-EPS-ELETRICA | [INT-003](03-integracao/INT-003-inicializacao-central.md), [INT-005](03-integracao/INT-005-sensores-e-adcs.md), [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) |
| P-TERMICO | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) |
| P-MISSAO | [TES-010](04-testes/TES-010-missao-e-cobertura.md), [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-008](03-integracao/INT-008-supervisao-e-retomada.md) |
| P-REGISTROS | [COM-005](01-comunicacao/COM-005-armazenamento-sd.md), [DAD-003](02-dados-e-tempo/DAD-003-registros-e-interpretacao.md) |
| P-VALIDACAO | [TES-010](04-testes/TES-010-missao-e-cobertura.md), [TES-008](04-testes/TES-008-integracao.md) |

| Entrega anterior | Destinos atuais |
|---|---|
| Plataforma e interfaces | [COM-001](01-comunicacao/done/COM-001-servico-i2c.md) a [COM-005](01-comunicacao/COM-005-armazenamento-sd.md); [INT-001](03-integracao/done/INT-001-ambiente-obc.md) a [INT-005](03-integracao/INT-005-sensores-e-adcs.md); [TES-002](04-testes/TES-002-executor-i2c.md), [TES-003](04-testes/TES-003-i2c-hardware.md), [TES-005](04-testes/TES-005-uart-hardware.md), [TES-006](04-testes/TES-006-sd-hardware.md) e cenário de boot em [TES-008](04-testes/TES-008-integracao.md). |
| Recepção, modelos e tempo | [COM-004](01-comunicacao/COM-004-uart-payload.md); [DAD-001](02-dados-e-tempo/DAD-001-modelos-e-formatos.md) a [DAD-003](02-dados-e-tempo/DAD-003-registros-e-interpretacao.md); [TES-004](04-testes/TES-004-uart-software.md), [TES-005](04-testes/TES-005-uart-hardware.md), [TES-007](04-testes/TES-007-dados-e-tempo.md) e referência temporal em [TES-008](04-testes/TES-008-integracao.md). |
| Registros persistentes | [COM-005](01-comunicacao/COM-005-armazenamento-sd.md); [DAD-001](02-dados-e-tempo/DAD-001-modelos-e-formatos.md) a [DAD-003](02-dados-e-tempo/DAD-003-registros-e-interpretacao.md); [INT-006](03-integracao/INT-006-fluxo-de-dados.md); [TES-006](04-testes/TES-006-sd-hardware.md), [TES-007](04-testes/TES-007-dados-e-tempo.md) e [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| Integração e missão | [INT-003](03-integracao/INT-003-inicializacao-central.md) a [INT-008](03-integracao/INT-008-supervisao-e-retomada.md); [TES-008](04-testes/TES-008-integracao.md) a [TES-010](04-testes/TES-010-missao-e-cobertura.md). |
| Validação integrada | [TES-001](04-testes/done/TES-001-escalonamento-i2c.md) a [TES-010](04-testes/TES-010-missao-e-cobertura.md), mantendo os critérios originais. |

O inventário e a configuração base estão em [INT-001](03-integracao/done/INT-001-ambiente-obc.md) e nas referências de cada integração. A conciliação de EN está em [INT-002](03-integracao/done/INT-002-pinagem-en-motor.md). O histórico de atualização do checklist fica nesta seção, sem criar uma tarefa recorrente de manutenção documental.

## Cobertura dos requisitos

Esta tabela indica responsáveis pela implementação e pela comprovação. Não declara requisitos atendidos. Descrições, prioridades, associações, entradas e resultados esperados continuam em [REQUISITOS.md](../.agents/REQUISITOS.md).

| Requisito | Implementação | Evidências previstas |
|---|---|---|
| REQ-OBC-001 | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-008](03-integracao/INT-008-supervisao-e-retomada.md) | [TES-008](04-testes/TES-008-integracao.md) (teste TC original), [TES-010](04-testes/TES-010-missao-e-cobertura.md) (autonomia). |
| REQ-OBC-002 | [INT-004](03-integracao/INT-004-ttc-e-umbilical.md), [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) | [TES-008](04-testes/TES-008-integracao.md). |
| REQ-OBC-003 | [INT-005](03-integracao/INT-005-sensores-e-adcs.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md) | [TES-008](04-testes/TES-008-integracao.md) (inspeção TM). |
| REQ-OBC-004 | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) | [TES-008](04-testes/TES-008-integracao.md), [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-OBC-005 | [COM-004](01-comunicacao/COM-004-uart-payload.md), [COM-005](01-comunicacao/COM-005-armazenamento-sd.md), [DAD-001](02-dados-e-tempo/DAD-001-modelos-e-formatos.md), [DAD-003](02-dados-e-tempo/DAD-003-registros-e-interpretacao.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md) | [TES-004](04-testes/TES-004-uart-software.md) a [TES-008](04-testes/TES-008-integracao.md). |
| REQ-OBC-006 | [DAD-002](02-dados-e-tempo/DAD-002-tempo-e-boots.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md) | [TES-007](04-testes/TES-007-dados-e-tempo.md), [TES-008](04-testes/TES-008-integracao.md) (ADS-B simulado e timestamp na TM). |
| REQ-OBC-007 | [COM-003](01-comunicacao/COM-003-recuperacao-i2c.md), [INT-008](03-integracao/INT-008-supervisao-e-retomada.md) | [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-OBC-008 | [INT-003](03-integracao/INT-003-inicializacao-central.md) | [TES-008](04-testes/TES-008-integracao.md) (boot). |
| REQ-OBC-009 | [DAD-001](02-dados-e-tempo/DAD-001-modelos-e-formatos.md), [DAD-003](02-dados-e-tempo/DAD-003-registros-e-interpretacao.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md), [INT-008](03-integracao/INT-008-supervisao-e-retomada.md) | [TES-007](04-testes/TES-007-dados-e-tempo.md), [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-OBC-010 | [DAD-002](02-dados-e-tempo/DAD-002-tempo-e-boots.md), [DAD-003](02-dados-e-tempo/DAD-003-registros-e-interpretacao.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md), [INT-008](03-integracao/INT-008-supervisao-e-retomada.md) | [TES-007](04-testes/TES-007-dados-e-tempo.md) a [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-OBC-011 | [INT-005](03-integracao/INT-005-sensores-e-adcs.md), [INT-008](03-integracao/INT-008-supervisao-e-retomada.md) | [TES-005](04-testes/TES-005-uart-hardware.md), [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-OBC-012 | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) | [TES-008](04-testes/TES-008-integracao.md), [TES-010](04-testes/TES-010-missao-e-cobertura.md). |
| REQ-NF-OBC-001 | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) | [TES-008](04-testes/TES-008-integracao.md), [TES-010](04-testes/TES-010-missao-e-cobertura.md) (consumo contra cota EPS). |
| REQ-NF-OBC-002 | [DAD-001](02-dados-e-tempo/DAD-001-modelos-e-formatos.md), [DAD-003](02-dados-e-tempo/DAD-003-registros-e-interpretacao.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md) | [TES-007](04-testes/TES-007-dados-e-tempo.md), [TES-008](04-testes/TES-008-integracao.md), [TES-010](04-testes/TES-010-missao-e-cobertura.md). |
| REQ-NF-OBC-003 | [DAD-002](02-dados-e-tempo/DAD-002-tempo-e-boots.md), [DAD-003](02-dados-e-tempo/DAD-003-registros-e-interpretacao.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md), [INT-008](03-integracao/INT-008-supervisao-e-retomada.md) | [TES-007](04-testes/TES-007-dados-e-tempo.md), [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-NF-OBC-004 | [COM-005](01-comunicacao/COM-005-armazenamento-sd.md), [DAD-003](02-dados-e-tempo/DAD-003-registros-e-interpretacao.md), [INT-008](03-integracao/INT-008-supervisao-e-retomada.md) | [TES-006](04-testes/TES-006-sd-hardware.md), [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |

REQ-OBC-013 continua como proposta no Design Package, pendente de incorporação formal. [TES-010](04-testes/TES-010-missao-e-cobertura.md) acompanha a condição de missão anterior aos ensaios ambientais, sem ampliar silenciosamente o catálogo de 16 requisitos.
