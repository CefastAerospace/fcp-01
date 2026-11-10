# COM-001 — Serviço I2C: inicialização, executor e fila

**Estado:** Concluída
**Responsáveis:** OBC-DH

## Situação atual da integração — 10/11/2026

O serviço já é chamado por `ObcStartup` no firmware atual. As menções abaixo a `main` vazio e serviço fora do boot são evidências do estado de 20–27/10, não pendências atuais. Sensores/ADCS continuam na INT-005; concorrência FreeRTOS e transações físicas permanecem nas TES-002/TES-003. [Integração atual](../../03-integracao/INT-003-inicializacao-central.md).

## Objetivo

Disponibilizar acesso serializado ao I2C por uma tarefa exclusiva.

## Entrega

Inicializar Wire e oferecer i2c::Execute() com fila limitada, sincronização e resultados explícitos. O escopo concluído é a implementação do serviço, sem ligação ao boot ou aos sensores.

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Nenhuma para o escopo implementado.

## Definições e condições

- Implementar `i2c::Init()` com SDA 21 / SCL 22, frequência de 400 kHz e timeout de 20 ms por transferência. Evidência: `headers/I2cBus.h` e `src/I2cBus.cpp`; ainda não chamado pelo boot.
- Implementar atendimento exclusivo em tarefa FreeRTOS e API síncrona `i2c::Execute()`. Prioridade provisória da tarefa = 5 e pilha = 4096 bytes; callbacks devem ser finitos e curtos.
- Implementar fila de oito posições estáticas, mutex dos metadados e semáforos de conclusão. A posição permanece ocupada até o chamador consumir o resultado.
- Implementar validação de argumentos/contexto e retornos de serviço não inicializado, fila cheia, timeout de fila e falha da operação. Espera padrão na fila = 1000 ms; operação iniciada é aguardada até terminar, preservando o contexto.

## Critérios de conclusão

API e implementação disponíveis, com limites e restrições de uso documentados. Concorrência e comunicação física têm ensaios próprios.

## Evidências

Os relatos, comandos, hashes e resultados a seguir descrevem as revisões datadas de cada entrega. Afirmações de ausência de integração ou de estado ainda em andamento nesse histórico não substituem o encerramento e os encaminhamentos atuais acima.

Implementação no commit bdfa48f, de 20/10/2026, conferida na revisão de 27/10/2026. O commit registra compilação ESP32 e atualização da base do editor; esta reorganização não repete o build. setup()/loop() continuam vazios. Não há evidência de execução do serviço em hardware.

## Referências

- [obc-dh/headers/I2cBus.h](../../../headers/I2cBus.h)
- [obc-dh/src/I2cBus.cpp](../../../src/I2cBus.cpp)
- Origem no planejamento anterior: P-BARRAMENTO.
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
