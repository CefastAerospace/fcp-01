# Organização das tarefas OBC-DH

O [TODO](../.agents/TODO.md) é o painel unificado. Os arquivos nesta pasta são a fonte do escopo, dependências, condições externas e evidências. O painel e cada tarefa devem manter o mesmo estado.

**Conclusão INT-004 — 17/11/2026:** após as verificações locais registradas,
o responsável OBC-DH informou o aceite TT&C das definições e autorizou o
encerramento. A task está em `03-integracao/done/`; referências e painel
foram atualizados. Nenhum ensaio ou requisito foi aprovado por esse aceite.

## Revisão vigente — 14/11/2026

A revisão separa entrega de software, contrato de integração e comprovação independente. OBC-DH mantém e executa regressões de desenvolvimento; o setor de testes executa os ensaios e registra o aceite. As tarefas novas são planejamento, não implementação ou evidência de execução. Nenhuma tarefa mudou de estado apenas por esta revisão.

| Tipo de entrega | Responsável | Encerramento |
|---|---|---|
| Implementação | OBC-DH | Comportamento do próprio escopo implementado, build e regressões pertinentes aprovados, versão e limitações registradas. |
| Contrato de integração | OBC-DH e setor correspondente | Interface, unidades, responsabilidades e parâmetros necessários registrados e acordados. Não transferir decisão de engenharia ao setor de testes. |
| Comprovação independente | Setor de testes, com apoio técnico | Procedimento executado, resultados comparados aos critérios e evidências preservadas. |

Uma implementação concluída não aprova requisito de missão. A TES vinculada permanece aberta até sua própria comprovação. COM/DAD/INT conservam verificações locais; não devem exigir a aprovação de uma TES que depende delas para iniciar. Uma definição elétrica ou operacional necessária ao próprio código continua sendo condição externa real, mesmo quando o ensaio físico tem task própria.

### Correspondência das entregas redistribuídas

| Origem | Entrega que permanece | Destino da entrega extraída |
|---|---|---|
| INT-003 | Boot, estados iniciais e falhas implementados | Comprovação elétrica/boot/reset continua na TES-008. |
| INT-004 | Adaptador e contrato OBC–TT&C | Comprovação RF/temporal continua na TES-008; aceite de contrato permanece nesta INT. |
| INT-005 | ADCS, encoder, aquisição de atitude/luz e FOC | [INT-009](03-integracao/INT-009-aquisicao-eps-termico.md): aquisição EPS/Térmico e seus contratos, incluindo pendências recebidas de DAD-001/COM-003. |
| INT-006 | Fluxo, registros, entregas, extração e contratos necessários | Comprovações físicas continuam nas TES-005/006/008/009. A ponta Pi continua responsabilidade Payload. |
| INT-007 | Comandos, modos e coordenação da missão | [INT-010](03-integracao/INT-010-protecoes-energia-termico.md): proteções; [INT-011](03-integracao/INT-011-burn-wire-persistencia.md): Burn Wire/estados duráveis DAD-003; [INT-012](03-integracao/done/INT-012-watchdog-reset-obc.md): `CMD_RESET_OBC`. |
| INT-008 | Supervisão de recursos e recuperação local, incluindo histórico do escritor e diagnósticos | INT-012: watchdog; [INT-013](03-integracao/INT-013-retomada-operacional.md): retorno de modos/missão e estados duráveis DAD-003 após reset. |
| TES-002 | Comprovação do executor FreeRTOS pelo setor de testes | OBC prepara e mantém a ferramenta, reaproveitando a bancada parcial de recuperação. |
| Regressões existentes | TES-001/004/007 e seus históricos preservados | [TES-011](04-testes/TES-011-regressoes-reproduziveis.md): execução nativa reproduzível; [TES-012](04-testes/TES-012-consumidor-telecomandos.md): lacunas do consumidor atual. |

Não renumerar IDs existentes. As novas tarefas recebem novos IDs do mesmo prefixo. Obrigações transferidas continuam abertas na destinatária, com referência à origem; uma divisão não encerra automaticamente nenhum dos lados. Históricos datados descrevem o escopo da época; esta correspondência e os escopos vigentes definem o trabalho atual.

### Parâmetros provisórios

Registrar cada parâmetro de desenvolvimento provisório na task proprietária: valor, unidade, finalidade, versão, responsável pela definição final e ensaio que o avaliará. Informar se ainda falta valor; esta revisão não arbitra nenhum número novo.

Um valor provisório pode permitir desenvolver lógica quando não redefine contrato ou segurança. Não autoriza atuação, mudança de nível elétrico, limite de proteção, aceite de interface ou aprovação de missão. Ausência de definição necessária mantém a entrega correspondente aberta. A troca posterior do valor pertence ao OBC, com regressões pertinentes; não cabe ao setor de testes ajustar firmware ou expectativas para obter aprovação. Entradas, asserts e resultados esperados históricos permanecem preservados.

## Estrutura e uso

- `01-comunicacao/`: serviço I2C, prioridade, recuperação, UART e acesso ao SD.
- `02-dados-e-tempo/`: modelos, tempo e representação/interpretação dos registros.
- `03-integracao/`: ambiente, boot, setores, fluxo de dados, modos e supervisão.
- `04-testes/`: verificações de software, interfaces físicas, integração e missão.

Cada categoria contém `done/`. Arquivos pendentes ficam na própria categoria; os concluídos ficam em `done/`, conservando ID e nome. A pasta de dados/tempo passou a registrar sua primeira conclusão com DAD-002 em 01/11/2026. Esta convenção substitui `.agents/tasks/active/` e `.agents/tasks/completed/` por decisão do responsável nesta reorganização.

Não ler todas as tarefas em cada trabalho. Consultar o painel e abrir somente a tarefa solicitada e as dependências necessárias. Referências de arquitetura, requisitos, pinagem e ferramentas permanecem em `.agents/`; não duplicar material de estudo nas tarefas.

## Dependências e estados

Uma entrega fica disponível quando as capacidades predecessoras que ela consome estão implementadas e suas condições externas resolvidas. Cada task deve identificar a capacidade e a versão quando utilizar uma entrega parcial já disponível, sem exigir encerramento de escopos independentes do mesmo ID. Preparação de contratos e lógica isolada pode anteceder a integração; isso não declara integração pronta. O responsável pela condição externa aparece no arquivo; registrar decisão, responsável, referência e unidades quando resolvê-la.

Pendente significa não iniciada, mesmo se ainda houver dependências. Em andamento significa execução iniciada. Bloqueada registra um impedimento encontrado durante a execução, com motivo e responsável. Concluída exige os critérios do escopo atendidos e evidências registradas. Não há automação de estados; atualizar painel e tarefa juntos.

[TES-008](04-testes/TES-008-integracao.md) e [TES-009](04-testes/TES-009-falhas-e-recuperacao.md) possuem cenários independentes: iniciar um cenário depende apenas dos IDs e condições daquela linha. A tarefa inteira só termina quando todos os seus cenários terminarem. Não criar dependência circular entre implementação e teste; uma tarefa de implementação conserva sua verificação local, enquanto os ensaios vinculados comprovam propriedades adicionais.

### Encerramento por responsabilidade — orientação de 09/11/2026

Uma tarefa pode encerrar quando sua própria entrega está implementada e
comprovada e só restam responsabilidades de integração, outros setores ou
ensaios com tarefas próprias. Registrar a evidência, transferir cada obrigação
com origem/destinatária e mantê-la aberta na receptora. `done/` não significa
aceite externo ou validação de missão. Código ou ensaio ainda incompleto no
próprio escopo mantém a tarefa aberta. Alterações posteriores podem gerar
correção/reabertura, com critérios de validação preservados.

Na auditoria de 09/11/2026 foram examinadas as vinte tarefas então abertas.
COM-004, COM-005, DAD-001, DAD-003, TES-004 e TES-007 foram encerradas por esse
critério, totalizando doze arquivos em `done/`. Quatorze continuam abertas;
INT-003 é parcial no próprio boot de voo e os demais ensaios/integrações ainda
têm entregas próprias não executadas. Não houve alteração de código ou testes.

## Conteúdo e encerramento

Cada arquivo contém estado/responsáveis, objetivo, entrega, pré-requisitos, critérios de conclusão, evidências e referências. Definições e condições detalham somente o trabalho correspondente. Testes integrados incluem sua tabela de cenários.

### Registro por cenário

Os checklists das TES de comprovação são roteiros para o setor de testes; seus responsáveis técnicos apoiam interfaces, montagem e diagnóstico. OBC-DH mantém ferramentas e regressões automatizadas de desenvolvimento, incluindo TES-011/012, sem que isso substitua o aceite dos ensaios. O prefixo TES não determina sozinho o executor: o cabeçalho identifica a responsabilidade. Revisões documentais não executam casos nem alteram estados por inferência.

Antes do ensaio, preencher condições, entrada/procedimento e resultado esperado com base no contrato/requisito vigente. Depois, registrar observado e evidência. Usar uma linha/ficha por caso do checklist:

| Campo | Conteúdo necessário |
|---|---|
| Caso e motivo | Task/caso, comportamento e requisito/contrato que justificam o teste. |
| Versão e ambiente | Revisão Git/artefato, roteiro, plataforma/dependências resolvidas, placa e identificação dos dispositivos. |
| Condições iniciais | Montagem, alimentação, configuração, estado temporal, arquivos e pré-requisitos do cenário. |
| Entrada e procedimento | Dados exatos, sequência de ações, método de falha e pontos de medição. |
| Esperado | Saída/status, dados que devem ser preservados, limites e referência que os define. |
| Observado | Saída real, tempos, bytes/contadores e divergências. |
| Evidência | Logs/arquivos/imagens preservados e procedimento de reprodução; não apenas caminho temporário de outro computador. |
| Resultado e limitações | Não executado, aprovado, reprovado ou impedido; responsável e condição de desbloqueio quando faltar recurso/definição. Esses rótulos são por caso, não novos estados de task. |

Marcar `[x]` somente quando o caso tiver evidência de aprovação. Rejeição prevista de entrada inválida pode ser sucesso do caso; falha inesperada não é aprovação por ser um teste de erro. Um limite ainda não acordado impede o aceite correspondente, sem justificar inventar um valor. Resultados parciais não concluem TES-008/TES-009/TES-010.

Os roteiros arquivados conservam suas entradas e expectativas. No firmware atual usar `ObcDownlinkCheck.py` e, como complemento, `ObcStationCheck.py`, conforme o guia. `ObcDataFlowCheck.py` exige a versão anterior a `request_log`/quadros `ac`; `ObcTtcCheck.py` exige a versão INT-004 anterior a `set_time`. Roteiro histórico incompatível não é regressão do firmware atual.

### Responsabilidades, correção e reexecução

- **Setor de testes:** executar os procedimentos e registrar todos os acertos, falhas e impedimentos por cenário, com versão, entradas, resultado esperado, observado e evidências. Encaminhar o relatório completo ao responsável OBC-DH, incluindo os casos aprovados. Não corrigir firmware nem alterar entradas, critérios ou resultados esperados para obter aprovação. Dúvidas ou problemas no roteiro também devem ser relatados, sem ajuste unilateral.
- **Responsável OBC-DH:** analisar os resultados, diagnosticar os problemas e realizar as correções de firmware necessárias, conforme as regras de discussão de AGENTS.md. Informar a nova versão e os casos afetados para reexecução. Questões de contrato ou de outros subsistemas devem ser tratadas com os responsáveis correspondentes.
- **OBC-DH, desenvolvimento:** preparar e manter as ferramentas de ensaio, executar regressões automatizadas pertinentes à mudança e informar firmware/roteiro compatíveis ao setor de testes. Cada comportamento novo acompanha casos nominais, recusas, fronteiras e preservação de estado pertinentes. Reutilizar testes existentes antes de acrescentar cobertura; não antecipar funcionalidades sem contrato.
- **Setor de testes, após a correção:** repetir os casos afetados e as regressões necessárias na versão indicada, conservando os critérios aprovados. Acrescentar os novos resultados sem apagar ou substituir relatórios, falhas e evidências anteriores. Aprovação de uma versão anterior não comprova o comportamento de um trecho alterado.

Executar todos os casos ou entregar o relatório não conclui automaticamente uma TES. Falhas, impedimentos e casos não executados que impeçam os critérios do próprio escopo mantêm a tarefa aberta; o arquivo não deve ser movido para `done/`. Após as correções, registrar a aprovação dos casos necessários antes do encerramento. Obrigações de outro escopo continuam nas destinatárias, conforme a regra de encerramento por responsabilidade.

Ao encerrar, somente após atender todos os critérios do próprio escopo: registrar versão, procedimento, entradas, resultados e limitações; marcar o estado; mover o arquivo para `done/`; corrigir seus links relativos e todos os links que apontam para ele; atualizar a linha no TODO. Não renumerar tarefas, nem marcar tarefas dependentes automaticamente. Se uma tarefa precisar ser reaberta, fazer o percurso inverso mantendo o histórico das evidências.

Implementação, integração e validação física são estados distintos. Na
reorganização de 27/10/2026 havia cinco tarefas em `done/`; DAD-002
acrescentou a sexta em 01/11/2026. A auditoria de 09/11 encerrou mais
seis escopos próprios com evidências e encaminhamentos explícitos.
Nenhum aceite externo ou ensaio físico foi inferido desses movimentos.

## Histórico e rastreabilidade

Reorganização de 27/10/2026 a partir do TODO do commit `423dacb`. O inventário de 07/10, a reorganização de 11/10 e a consolidação de interfaces de 14/10 permanecem como histórico documental. O serviço I2C foi acrescentado em `bdfa48f` (20/10); o teste nativo aprovado em 27/10 está em [TES-001](04-testes/done/TES-001-escalonamento-i2c.md). A presente migração não executa novamente os ensaios nem comprova hardware.

O I2C anteriormente cogitado para o Payload foi substituído por UART. A proposta `0x80` e a decisão encerrada continuam no [histórico dos requisitos](../.agents/REQUISITOS.md#histórico-de-decisões-de-interface). Em 05/11/2026, o responsável substituiu a decisão de binário compacto por snapshots JSON completos na UART. Modelos locais estão em DAD-001; enquadramento, integridade e compatibilização dos cenários existentes de SeqID/NACK ficam em COM-004. LoRa usa VSPI, SD usa HSPI e sensores/encoder usam I2C. Não há RTC externo na configuração atual.

As pendências antigas foram distribuídas sem alterar decisões ou critérios. Os IDs P-* abaixo existem apenas para rastrear sua origem; os IDs operacionais são COM/DAD/INT/TES.

| Pendência anterior | Destinos |
|---|---|
| P-PAYLOAD | [COM-004](01-comunicacao/done/COM-004-uart-payload.md) |
| P-BARRAMENTO | [COM-001](01-comunicacao/done/COM-001-servico-i2c.md), [COM-002](01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md), [COM-003](01-comunicacao/done/COM-003-recuperacao-i2c.md), [TES-001](04-testes/done/TES-001-escalonamento-i2c.md), [TES-003](04-testes/TES-003-i2c-hardware.md), [INT-005](03-integracao/INT-005-sensores-e-adcs.md) |
| P-ENDERECOS | [INT-005](03-integracao/INT-005-sensores-e-adcs.md), [DAD-002](02-dados-e-tempo/done/DAD-002-tempo-e-boots.md) |
| P-DADOS | [COM-004](01-comunicacao/done/COM-004-uart-payload.md), [DAD-001](02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md), [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md) |
| P-TEMPO | [DAD-002](02-dados-e-tempo/done/DAD-002-tempo-e-boots.md) |
| P-BURN-WIRE | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) |
| P-ADCS | [INT-005](03-integracao/INT-005-sensores-e-adcs.md), [INT-002](03-integracao/done/INT-002-pinagem-en-motor.md), [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) |
| P-SPI | [COM-005](01-comunicacao/done/COM-005-armazenamento-sd.md), [INT-004](03-integracao/done/INT-004-ttc-e-umbilical.md) |
| P-EPS-ELETRICA | [INT-003](03-integracao/INT-003-inicializacao-central.md), [INT-005](03-integracao/INT-005-sensores-e-adcs.md), [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) |
| P-TERMICO | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md) |
| P-MISSAO | [TES-010](04-testes/TES-010-missao-e-cobertura.md), [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-008](03-integracao/done/INT-008-supervisao-e-retomada.md) |
| P-REGISTROS | [COM-005](01-comunicacao/done/COM-005-armazenamento-sd.md), [DAD-003](02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) |
| P-VALIDACAO | [TES-010](04-testes/TES-010-missao-e-cobertura.md), [TES-008](04-testes/TES-008-integracao.md) |

| Entrega anterior | Destinos atuais |
|---|---|
| Plataforma e interfaces | [COM-001](01-comunicacao/done/COM-001-servico-i2c.md) a [COM-005](01-comunicacao/done/COM-005-armazenamento-sd.md); [INT-001](03-integracao/done/INT-001-ambiente-obc.md) a [INT-005](03-integracao/INT-005-sensores-e-adcs.md); [TES-002](04-testes/TES-002-executor-i2c.md), [TES-003](04-testes/TES-003-i2c-hardware.md), [TES-005](04-testes/TES-005-uart-hardware.md), [TES-006](04-testes/done/TES-006-sd-hardware.md) e cenário de boot em [TES-008](04-testes/TES-008-integracao.md). |
| Recepção, modelos e tempo | [COM-004](01-comunicacao/done/COM-004-uart-payload.md); [DAD-001](02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md) a [DAD-003](02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md); [TES-004](04-testes/done/TES-004-uart-software.md), [TES-005](04-testes/TES-005-uart-hardware.md), [TES-007](04-testes/done/TES-007-dados-e-tempo.md) e referência temporal em [TES-008](04-testes/TES-008-integracao.md). |
| Registros persistentes | [COM-005](01-comunicacao/done/COM-005-armazenamento-sd.md); [DAD-001](02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md) a [DAD-003](02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md); [INT-006](03-integracao/INT-006-fluxo-de-dados.md); [TES-006](04-testes/done/TES-006-sd-hardware.md), [TES-007](04-testes/done/TES-007-dados-e-tempo.md) e [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| Integração e missão | [INT-003](03-integracao/INT-003-inicializacao-central.md) a [INT-008](03-integracao/done/INT-008-supervisao-e-retomada.md); [TES-008](04-testes/TES-008-integracao.md) a [TES-010](04-testes/TES-010-missao-e-cobertura.md). |
| Validação integrada | [TES-001](04-testes/done/TES-001-escalonamento-i2c.md) a [TES-010](04-testes/TES-010-missao-e-cobertura.md), mantendo os critérios originais. |

O inventário e a configuração base estão em [INT-001](03-integracao/done/INT-001-ambiente-obc.md) e nas referências de cada integração. A conciliação de EN está em [INT-002](03-integracao/done/INT-002-pinagem-en-motor.md). O histórico de atualização do checklist fica nesta seção, sem criar uma tarefa recorrente de manutenção documental.

## Cobertura dos requisitos

Esta tabela indica responsáveis pela implementação e pela comprovação. A redistribuição de 14/11 inclui INT-009 a INT-013; referências anteriores permanecem como origem, sem duplicar a obrigação extraída. TES-011/012 complementam regressões de desenvolvimento, sem substituir as evidências previstas. Não declara requisitos atendidos. Descrições, prioridades, associações, entradas e resultados esperados continuam em [REQUISITOS.md](../.agents/REQUISITOS.md).

| Requisito | Implementação | Evidências previstas |
|---|---|---|
| REQ-OBC-001 | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-008](03-integracao/done/INT-008-supervisao-e-retomada.md), [INT-012](03-integracao/done/INT-012-watchdog-reset-obc.md), [INT-013](03-integracao/INT-013-retomada-operacional.md) | [TES-008](04-testes/TES-008-integracao.md) (teste TC original), [TES-010](04-testes/TES-010-missao-e-cobertura.md) (autonomia). |
| REQ-OBC-002 | [INT-004](03-integracao/done/INT-004-ttc-e-umbilical.md), [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-011](03-integracao/INT-011-burn-wire-persistencia.md), [INT-012](03-integracao/done/INT-012-watchdog-reset-obc.md) | [TES-008](04-testes/TES-008-integracao.md). |
| REQ-OBC-003 | [INT-005](03-integracao/INT-005-sensores-e-adcs.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md), [INT-009](03-integracao/INT-009-aquisicao-eps-termico.md) | [TES-008](04-testes/TES-008-integracao.md) (inspeção TM). |
| REQ-OBC-004 | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-010](03-integracao/INT-010-protecoes-energia-termico.md), [INT-013](03-integracao/INT-013-retomada-operacional.md) | [TES-008](04-testes/TES-008-integracao.md), [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-OBC-005 | [COM-004](01-comunicacao/done/COM-004-uart-payload.md), [COM-005](01-comunicacao/done/COM-005-armazenamento-sd.md), [DAD-001](02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md), [DAD-003](02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md), [INT-013](03-integracao/INT-013-retomada-operacional.md) | [TES-004](04-testes/done/TES-004-uart-software.md) a [TES-008](04-testes/TES-008-integracao.md). |
| REQ-OBC-006 | [DAD-002](02-dados-e-tempo/done/DAD-002-tempo-e-boots.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md) | [TES-007](04-testes/done/TES-007-dados-e-tempo.md), [TES-008](04-testes/TES-008-integracao.md) (ADS-B simulado e timestamp na TM). |
| REQ-OBC-007 | [COM-003](01-comunicacao/done/COM-003-recuperacao-i2c.md), [INT-008](03-integracao/done/INT-008-supervisao-e-retomada.md), [INT-012](03-integracao/done/INT-012-watchdog-reset-obc.md), [INT-013](03-integracao/INT-013-retomada-operacional.md) | [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-OBC-008 | [INT-003](03-integracao/INT-003-inicializacao-central.md), [INT-013](03-integracao/INT-013-retomada-operacional.md) | [TES-008](04-testes/TES-008-integracao.md) (boot). |
| REQ-OBC-009 | [DAD-001](02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md), [DAD-003](02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md), [INT-008](03-integracao/done/INT-008-supervisao-e-retomada.md), [INT-010](03-integracao/INT-010-protecoes-energia-termico.md), [INT-011](03-integracao/INT-011-burn-wire-persistencia.md), [INT-012](03-integracao/done/INT-012-watchdog-reset-obc.md), [INT-013](03-integracao/INT-013-retomada-operacional.md) | [TES-007](04-testes/done/TES-007-dados-e-tempo.md), [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-OBC-010 | [DAD-002](02-dados-e-tempo/done/DAD-002-tempo-e-boots.md), [DAD-003](02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md), [INT-008](03-integracao/done/INT-008-supervisao-e-retomada.md), [INT-010](03-integracao/INT-010-protecoes-energia-termico.md), [INT-011](03-integracao/INT-011-burn-wire-persistencia.md), [INT-012](03-integracao/done/INT-012-watchdog-reset-obc.md), [INT-013](03-integracao/INT-013-retomada-operacional.md) | [TES-007](04-testes/done/TES-007-dados-e-tempo.md) a [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-OBC-011 | [INT-005](03-integracao/INT-005-sensores-e-adcs.md), [INT-008](03-integracao/done/INT-008-supervisao-e-retomada.md), [INT-009](03-integracao/INT-009-aquisicao-eps-termico.md) | [TES-005](04-testes/TES-005-uart-hardware.md), [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-OBC-012 | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-010](03-integracao/INT-010-protecoes-energia-termico.md) | [TES-008](04-testes/TES-008-integracao.md), [TES-010](04-testes/TES-010-missao-e-cobertura.md). |
| REQ-NF-OBC-001 | [INT-007](03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-010](03-integracao/INT-010-protecoes-energia-termico.md) | [TES-008](04-testes/TES-008-integracao.md), [TES-010](04-testes/TES-010-missao-e-cobertura.md) (consumo contra cota EPS). |
| REQ-NF-OBC-002 | [DAD-001](02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md), [DAD-003](02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md) | [TES-007](04-testes/done/TES-007-dados-e-tempo.md), [TES-008](04-testes/TES-008-integracao.md), [TES-010](04-testes/TES-010-missao-e-cobertura.md). |
| REQ-NF-OBC-003 | [DAD-002](02-dados-e-tempo/done/DAD-002-tempo-e-boots.md), [DAD-003](02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md), [INT-006](03-integracao/INT-006-fluxo-de-dados.md), [INT-008](03-integracao/done/INT-008-supervisao-e-retomada.md), [INT-012](03-integracao/done/INT-012-watchdog-reset-obc.md), [INT-013](03-integracao/INT-013-retomada-operacional.md) | [TES-007](04-testes/done/TES-007-dados-e-tempo.md), [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |
| REQ-NF-OBC-004 | [COM-005](01-comunicacao/done/COM-005-armazenamento-sd.md), [DAD-003](02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md), [INT-008](03-integracao/done/INT-008-supervisao-e-retomada.md), [INT-011](03-integracao/INT-011-burn-wire-persistencia.md), [INT-013](03-integracao/INT-013-retomada-operacional.md) | [TES-006](04-testes/done/TES-006-sd-hardware.md), [TES-009](04-testes/TES-009-falhas-e-recuperacao.md). |

REQ-OBC-013 continua como proposta no Design Package, pendente de incorporação formal. [TES-010](04-testes/TES-010-missao-e-cobertura.md) acompanha a condição de missão anterior aos ensaios ambientais, sem ampliar silenciosamente o catálogo de 16 requisitos.
