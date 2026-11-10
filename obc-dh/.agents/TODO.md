# TODO — OBC-DH

**Atualização de 09/11/2026 — encerramento por responsabilidade:** COM-004,
COM-005, DAD-001, DAD-003, TES-004 e TES-007 estão concluídas no escopo próprio
com evidências registradas e arquivos em `done/`. Aceites, montagem, ponta Pi,
integração, estados duráveis e ensaios físicos continuam pendentes nas INT/TES
destinatárias, vinculadas em ambas as pontas. INT-003 permanece em andamento
por trabalho próprio do boot de voo. As revisões anteriores abaixo são histórico.

Painel unificado das tarefas em quatro grupos. Revisão: 05/11/2026. O serviço I2C e sua política estão implementados; o teste nativo passou. DAD-002 acrescenta lógica temporal e reserva de boots em NVS, com teste nativo e build ESP32 aprovados. DAD-001 tem modelos/validadores v1 e de snapshots JSON completos do Payload, com extras textuais e teste nativo/build aprovados; DAD-003 tem codec v1/v2 e leitor nativo de arquivos mistos, incluindo snapshots e extras. Permanecem pendentes o aceite dos produtores e os estados duráveis de retomada. INT-003 inicializa tempo/boots, I2C e tenta montar SD no boot de bancada, com diagnóstico USB; não inicia missão ou grava registros.

Cada entrega aparece uma única vez. O link abre escopo, condições, conclusão e evidências. **Pendente** = não iniciada; **Em andamento** = execução iniciada; **Bloqueada** = impedimento registrado durante a execução; **Concluída** = escopo comprovado e arquivo em `done/`. Pendente não significa automaticamente disponível.

**Histórico de 05/11/2026:** COM-005 tem núcleo de arquivos e adaptador SD/HSPI implementados localmente, com teste nativo de falhas e build ESP32 aprovados em 05/11/2026. Sincroniza cada bloco, repete até três vezes no mesmo offset e preserva arquivos. Permanece em andamento até confirmação de módulo/alimentação com EPS. INT-003 passou a chamar sua montagem na bancada, sem comprovação física; TES-006/TES-009 conservam os ensaios.

INT-003 tem entrega parcial de bancada, teste nativo das oito combinações de resultados e build ESP32 aprovados. Falhas não interrompem os serviços independentes; diagnóstico não provoca repetição/reset. Permanecem pendentes placa, ConOps e níveis elétricos do boot integrado; não há upload ou ensaio físico declarado.

**Histórico de 05/11/2026:** DAD-003 acrescentou persistência de snapshots/extras v2 em registros variáveis de 37–64497 bytes, sem alterar o v1. Novos vetores independentes, leitor misto, testes anteriores e build ESP32 aprovados. A pendência de codec dos snapshots foi resolvida; consolidação de DAD-001 e estados duráveis de retomada permanecem abertos. O boot de bancada não codifica ou grava snapshots.

**Para escolher trabalho:** conferir tarefas predecessoras e condições externas separadamente. Com ambas atendidas, a tarefa pode avançar em paralelo às demais disponíveis. Resolver definições externas pode começar antes da implementação. Em TES-008/TES-009, conferir cada cenário. Números identificam tarefas, não fases obrigatoriamente sequenciais.

Detalhes e histórico: [regras e rastreabilidade](../.tasks/README.md). Requisitos e validação: [REQUISITOS.md](REQUISITOS.md). Guias de aprendizado permanecem na documentação, fora deste painel.

## 1. Comunicação e armazenamento

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| COM-001 | [Serviço I2C: inicialização, executor e fila](../.tasks/01-comunicacao/done/COM-001-servico-i2c.md) | — | — | Concluída |
| COM-002 | [Prioridade dinâmica e ordenação I2C](../.tasks/01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md) | COM-001 | — | Concluída |
| COM-003 | [Recuperação do barramento I2C após falhas](../.tasks/01-comunicacao/COM-003-recuperacao-i2c.md) | COM-001 | Política de recuperação | Pendente |
| COM-004 | [Transporte UART e recepção validada do Payload](../.tasks/01-comunicacao/done/COM-004-uart-payload.md) | — | Aceite, tráfego e ponta Pi em INT-006; ensaios em TES-005/TES-009 | Concluída |
| COM-005 | [SD: gravação, leitura, extração e recuperação](../.tasks/01-comunicacao/done/COM-005-armazenamento-sd.md) | — | Montagem/EPS em INT-003/TES-006; fluxo e falhas em INT-006/INT-008/TES-009 | Concluída |

## 2. Dados e tempo

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| DAD-001 | [Modelos de dados e formatos persistidos](../.tasks/02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md) | — | Aceites/mapeamentos em INT-004/INT-005/INT-006 | Concluída |
| DAD-002 | [Tempo monotônico, boots e referências absolutas](../.tasks/02-dados-e-tempo/done/DAD-002-tempo-e-boots.md) | — | Contrato local aprovado; precisão física em TES-008 | Concluída |
| DAD-003 | [Codificação e interpretação dos registros](../.tasks/02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) | DAD-001, DAD-002 | Fluxo em INT-006; estados duráveis em INT-007/INT-008; ensaios em TES-009 | Concluída |

## 3. Integração e operação

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| INT-001 | [Configuração base do ambiente OBC](../.tasks/03-integracao/done/INT-001-ambiente-obc.md) | — | — | Concluída |
| INT-002 | [Conciliação de EN do motor no GPIO 12](../.tasks/03-integracao/done/INT-002-pinagem-en-motor.md) | — | — | Concluída |
| INT-003 | [Inicialização central e organização da execução](../.tasks/03-integracao/INT-003-inicializacao-central.md) | INT-001 | Bancada implementada; confirmar placa, ConOps e estados seguros de voo | Em andamento |
| INT-004 | [Integração de TT&C e umbilical](../.tasks/03-integracao/INT-004-ttc-e-umbilical.md) | INT-003 | Aceite conjunto TT&C; implementação e USB verificados | Em andamento |
| INT-005 | [Integração de sensores, encoder e ADCS](../.tasks/03-integracao/INT-005-sensores-e-adcs.md) | COM-001, COM-002, INT-002, INT-003 | Inventário e limites dos setores | Pendente |
| INT-006 | [Fluxo Payload–tempo–SD–telemetria](../.tasks/03-integracao/INT-006-fluxo-de-dados.md) | COM-004, COM-005, DAD-003, INT-004 | Etapa 1 local verificada; downlink/extração e aceites pendentes | Em andamento |
| INT-007 | [Comandos, modos e proteções operacionais](../.tasks/03-integracao/INT-007-comandos-modos-e-protecoes.md) | INT-004, INT-005, INT-006 | ConOps e condições de atuação | Pendente |
| INT-008 | [Supervisão, watchdog e retomada após falhas](../.tasks/03-integracao/INT-008-supervisao-e-retomada.md) | COM-003, INT-007 | Limites de recuperação | Pendente |

## 4. Testes

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| TES-001 | [Teste nativo do escalonamento I2C](../.tasks/04-testes/done/TES-001-escalonamento-i2c.md) | COM-002 | — | Concluída |
| TES-002 | [Executor I2C: concorrência, fila e erros](../.tasks/04-testes/TES-002-executor-i2c.md) | COM-001, COM-002 | Ambiente FreeRTOS de teste | Pendente |
| TES-003 | [Comunicação I2C em hardware](../.tasks/04-testes/TES-003-i2c-hardware.md) | COM-001, COM-002 | Dispositivos e montagem | Pendente |
| TES-004 | [Recepção UART: validação e limites](../.tasks/04-testes/done/TES-004-uart-software.md) | COM-004 | Contrato local verificado; interoperabilidade em TES-005 e excesso físico em TES-009 | Concluída |
| TES-005 | [UART entre ESP32 e Raspberry Pi](../.tasks/04-testes/TES-005-uart-hardware.md) | COM-004 | Loopback local aprovado; ponta Pi e montagem pendentes | Em andamento |
| TES-006 | [SD: operações e falhas em hardware](../.tasks/04-testes/TES-006-sd-hardware.md) | COM-005 | Cartão e montagem | Pendente |
| TES-007 | [Formatos e tempo: interpretação e limites](../.tasks/04-testes/done/TES-007-dados-e-tempo.md) | DAD-003 | Lógica e formatos verificados; precisão/retomada física em TES-008/TES-009 | Concluída |
| TES-008 | [Boot, comandos, telemetria e convivência](../.tasks/04-testes/TES-008-integracao.md) | Por cenário no arquivo | Por cenário | Pendente |
| TES-009 | [Falhas, recuperação e integridade](../.tasks/04-testes/TES-009-falhas-e-recuperacao.md) | Por cenário no arquivo | Por cenário | Pendente |
| TES-010 | [Missão completa e cobertura dos requisitos](../.tasks/04-testes/TES-010-missao-e-cobertura.md) | INT-008; TES-001 a TES-009 | Referências e limites vigentes | Pendente |
