# TODO — OBC-DH

Painel unificado das tarefas em quatro grupos. Revisão: 27/10/2026. O serviço I2C e sua política estão implementados; o teste nativo passou. O firmware central ainda não inicializa esses recursos.

Cada entrega aparece uma única vez. O link abre escopo, condições, conclusão e evidências. **Pendente** = não iniciada; **Em andamento** = execução iniciada; **Bloqueada** = impedimento registrado durante a execução; **Concluída** = escopo comprovado e arquivo em `done/`. Pendente não significa automaticamente disponível.

**Para escolher trabalho:** conferir tarefas predecessoras e condições externas separadamente. Com ambas atendidas, a tarefa pode avançar em paralelo às demais disponíveis. Resolver definições externas pode começar antes da implementação. Em TES-008/TES-009, conferir cada cenário. Números identificam tarefas, não fases obrigatoriamente sequenciais.

Detalhes e histórico: [regras e rastreabilidade](../.tasks/README.md). Requisitos e validação: [REQUISITOS.md](REQUISITOS.md). Guias de aprendizado permanecem na documentação, fora deste painel.

## 1. Comunicação e armazenamento

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| COM-001 | [Serviço I2C: inicialização, executor e fila](../.tasks/01-comunicacao/done/COM-001-servico-i2c.md) | — | — | Concluída |
| COM-002 | [Prioridade dinâmica e ordenação I2C](../.tasks/01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md) | COM-001 | — | Concluída |
| COM-003 | [Recuperação do barramento I2C após falhas](../.tasks/01-comunicacao/COM-003-recuperacao-i2c.md) | COM-001 | Política de recuperação | Pendente |
| COM-004 | [Transporte UART e recepção validada do Payload](../.tasks/01-comunicacao/COM-004-uart-payload.md) | — | Contrato com Payload | Pendente |
| COM-005 | [SD: gravação, leitura, extração e recuperação](../.tasks/01-comunicacao/COM-005-armazenamento-sd.md) | — | Parâmetros do SD | Pendente |

## 2. Dados e tempo

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| DAD-001 | [Modelos de dados e formatos persistidos](../.tasks/02-dados-e-tempo/DAD-001-modelos-e-formatos.md) | — | Campos e unidades dos produtores | Pendente |
| DAD-002 | [Tempo monotônico, boots e referências absolutas](../.tasks/02-dados-e-tempo/DAD-002-tempo-e-boots.md) | — | Critérios temporais e boots | Pendente |
| DAD-003 | [Codificação e interpretação dos registros](../.tasks/02-dados-e-tempo/DAD-003-registros-e-interpretacao.md) | DAD-001, DAD-002 | — | Pendente |

## 3. Integração e operação

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| INT-001 | [Configuração base do ambiente OBC](../.tasks/03-integracao/done/INT-001-ambiente-obc.md) | — | — | Concluída |
| INT-002 | [Conciliação de EN do motor no GPIO 12](../.tasks/03-integracao/done/INT-002-pinagem-en-motor.md) | — | — | Concluída |
| INT-003 | [Inicialização central e organização da execução](../.tasks/03-integracao/INT-003-inicializacao-central.md) | INT-001 | Placa, ConOps e estados seguros | Pendente |
| INT-004 | [Integração de TT&C e umbilical](../.tasks/03-integracao/INT-004-ttc-e-umbilical.md) | INT-003 | Biblioteca TT&C e montagem | Pendente |
| INT-005 | [Integração de sensores, encoder e ADCS](../.tasks/03-integracao/INT-005-sensores-e-adcs.md) | COM-001, COM-002, INT-002, INT-003 | Inventário e limites dos setores | Pendente |
| INT-006 | [Fluxo Payload–tempo–SD–telemetria](../.tasks/03-integracao/INT-006-fluxo-de-dados.md) | COM-004, COM-005, DAD-003, INT-004 | Política de downlink | Pendente |
| INT-007 | [Comandos, modos e proteções operacionais](../.tasks/03-integracao/INT-007-comandos-modos-e-protecoes.md) | INT-004, INT-005, INT-006 | ConOps e condições de atuação | Pendente |
| INT-008 | [Supervisão, watchdog e retomada após falhas](../.tasks/03-integracao/INT-008-supervisao-e-retomada.md) | COM-003, INT-007 | Limites de recuperação | Pendente |

## 4. Testes

| ID | Entrega | Tarefas prévias | Condições externas | Estado |
|---|---|---|---|---|
| TES-001 | [Teste nativo do escalonamento I2C](../.tasks/04-testes/done/TES-001-escalonamento-i2c.md) | COM-002 | — | Concluída |
| TES-002 | [Executor I2C: concorrência, fila e erros](../.tasks/04-testes/TES-002-executor-i2c.md) | COM-001, COM-002 | Ambiente FreeRTOS de teste | Pendente |
| TES-003 | [Comunicação I2C em hardware](../.tasks/04-testes/TES-003-i2c-hardware.md) | COM-001, COM-002 | Dispositivos e montagem | Pendente |
| TES-004 | [Recepção UART: validação e limites](../.tasks/04-testes/TES-004-uart-software.md) | COM-004 | Casos do contrato UART | Pendente |
| TES-005 | [UART entre ESP32 e Raspberry Pi](../.tasks/04-testes/TES-005-uart-hardware.md) | COM-004 | Ponta Pi e montagem | Pendente |
| TES-006 | [SD: operações e falhas em hardware](../.tasks/04-testes/TES-006-sd-hardware.md) | COM-005 | Cartão e montagem | Pendente |
| TES-007 | [Formatos e tempo: interpretação e limites](../.tasks/04-testes/TES-007-dados-e-tempo.md) | DAD-003 | Critérios de dados/tempo | Pendente |
| TES-008 | [Boot, comandos, telemetria e convivência](../.tasks/04-testes/TES-008-integracao.md) | Por cenário no arquivo | Por cenário | Pendente |
| TES-009 | [Falhas, recuperação e integridade](../.tasks/04-testes/TES-009-falhas-e-recuperacao.md) | Por cenário no arquivo | Por cenário | Pendente |
| TES-010 | [Missão completa e cobertura dos requisitos](../.tasks/04-testes/TES-010-missao-e-cobertura.md) | INT-008; TES-001 a TES-009 | Referências e limites vigentes | Pendente |
