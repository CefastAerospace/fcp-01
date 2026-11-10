# TES-003 — Comunicação I2C em hardware

**Estado:** Pendente
**Responsáveis:** OBC-DH, ADCS, EPS e Térmico

A execução e o relato dos ensaios cabem ao setor de testes; a análise e as correções de firmware cabem ao responsável OBC-DH, com apoio dos setores envolvidos. Seguir as [responsabilidades e regras de reexecução](../README.md#responsabilidades-correção-e-reexecução).

## Objetivo

Comprovar transações físicas I2C usando o serviço existente.

## Entrega

Usar uma bancada mínima que chama Init()/Execute(), sem depender do main integrado. Conferir endereços e operações dos dispositivos disponíveis, frequência real, resposta/ausência de dispositivo e timeout. Registrar configuração e limitações de cada montagem; recuperação de SDA/SCL presos pertence a [TES-009](TES-009-falhas-e-recuperacao.md).

## Pré-requisitos

- **Tarefas:** [COM-001](../01-comunicacao/done/COM-001-servico-i2c.md), [COM-002](../01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md)
- **Condições externas:** Placa e dispositivos disponíveis; inventário e montagem elétrica conferidos com os setores.

## Definições e condições

- Conferir controlador/dispositivos, SDA/SCL, compatibilidade dos 400 kHz, níveis, pull-ups, capacitância, comprimento e comportamento com periférico desligado. O Payload não participa desse barramento.

## Critérios de conclusão

Registrar versão, montagem, operações e resultados por dispositivo contra os parâmetros acordados. Dispositivo ainda não disponível mantém seu cenário pendente; a conclusão não comprova prazos FOC integrados.

## Prontidão e preparação para o setor de testes

COM-001/COM-002 estão concluídas; a preparação pode ser encaminhada. A execução física exige os dispositivos, pull-ups e montagem conferidos com os setores. ESP32 e cabos isolados não comprovam operações de sensores ou frequência real. Não aguardar INT-005 para usar uma bancada mínima do serviço, mas não declarar integração ADCS/FOC por esse ensaio.

## Checklist de execução

- [ ] **Montagem por dispositivo:** registrar placa, alimentação, GND, SDA/SCL 21/22, endereço de sete bits e configuração dos pinos de endereço. Conferir níveis, pull-ups e compatibilidade com 400 kHz antes de energizar; endereços EPS/Térmico ainda não confirmados impedem seus casos.
- [ ] **Operação nominal:** executar pelo serviço uma operação conhecida do driver de cada dispositivo, com entrada e resposta previamente especificadas pelo setor. Esperar conteúdo compatível com a referência e callback/status correspondentes; presença de ACK não comprova a correção da leitura.
- [ ] **Frequência e duração:** medir clock e duração/timeout com instrumentação adequada e comparar com os parâmetros acordados. A constante de 400 kHz e o timeout configurado de 20 ms não são medições físicas; o timeout não limita a duração total do callback.
- [ ] **Dispositivo ausente/endereço sem resposta:** usar condição de ausência aprovada; registrar código bruto do driver/Wire e status do callback. Esperar falha observável, sem leitura fictícia ou espera indefinida; um próximo pedido válido deve continuar possível se o barramento estiver livre.
- [ ] **Transação incompleta ou timeout:** aplicar o método de injeção aprovado e conferir retorno previsto pelo driver e pelo serviço, sem publicar conteúdo incompleto como leitura válida. Não transformar esse caso em ensaio de recuperação de SDA/SCL presos.
- [ ] **Periférico desligado:** somente com procedimento elétrico aprovado, observar níveis e resposta do barramento; documentar limitações de alimentação unilateral e eventual impedimento aos demais dispositivos.
- [ ] **Operação compartilhada:** executar operações conhecidas em mais de um dispositivo pelo executor e conferir ausência de transações intercaladas; conservar a distinção entre esta verificação física e concorrência/limites da TES-002.

Registrar cada dispositivo/caso separadamente no formato de [evidências](../README.md#registro-por-cenário). Dispositivo ou instrumento ausente mantém o caso pendente; recuperação ativa do barramento pertence à TES-009, após COM-003.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- [obc-dh/headers/I2cBus.h](../../headers/I2cBus.h)
- Origem no planejamento anterior: P-BARRAMENTO.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
