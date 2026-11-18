# TES-012 — Regressão do consumidor de telecomandos OBC

**Estado:** Pendente
**Responsável pela ferramenta e execução de desenvolvimento:** OBC-DH

## Objetivo

Verificar a validação e os efeitos do consumidor OBC, complementando os testes do protocolo e transporte TT&C.

## Entrega

- Mapear a cobertura atual de `ObcDownlinkCheck.py`, `ObcStationCheck.py`, testes nativos de downlink e testes TT&C antes de criar casos adicionais.
- Exercitar o consumidor real: argumentos inválidos sem efeito, fluxo indisponível, falha de enfileiramento, aplicação de `set_time` e encaminhamento de `request_log`, eventos TX e confirmações. Conferir status/ACK e estado preservado.
- Reutilizar os roteiros USB atuais primeiro. Se for necessário tornar decisão/validação acessível ao teste nativo, separar somente a lógica responsável, mantendo a chamada pelo adaptador real; não testar uma segunda implementação do despacho.
- Acrescentar casos em arquivos novos ou suítes complementares, sem reescrever vetores, asserts ou expectativas históricas. Comandos ainda não implementados conservam sua rejeição na versão correspondente.
- Quando INT-007/010/011/012/013 entregarem comportamento novo, acrescentar suas regressões junto à implementação; não fabricar modos ou efeitos para completar esta tarefa.

## Pré-requisitos

- **Tarefas/capacidades:** consumidor atual da INT-004/006 e roteiros compatíveis. Não exige conclusão de ADCS, EPS ou Payload físico para testar os comandos já implementados.
- **Condições externas:** para casos USB, ESP32 e porta identificados, com firmware/roteiro compatíveis; dependências do cliente real da estação quando usadas. Falhas injetadas devem ser isoladas da operação normal.

## Critérios de conclusão

Cobertura do consumidor atual mapeada, lacunas exercitadas e regressões aprovadas com a implementação real. Registrar revisão, cenário, entradas, esperado/observado e limitações. Não declarar precisão RF, persistência física ou segurança de atuação a partir desses testes. Funcionalidades futuras não bloqueiam o encerramento da cobertura da versão atual.

## Encaminhamento para comprovação

[TES-008](TES-008-integracao.md): TC/ACK, TM e tempos integrados nos meios reais. [TES-009](TES-009-falhas-e-recuperacao.md): comportamento sob falhas/reset. Os critérios de entrega ≤200 ms e resposta ≤2 s permanecem intactos.

## Evidências

Casos adicionais ainda não implementados; esta revisão apenas criou a tarefa.
