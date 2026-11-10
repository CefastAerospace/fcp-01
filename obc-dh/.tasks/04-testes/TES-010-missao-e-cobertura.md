# TES-010 — Missão completa e cobertura dos requisitos

**Estado:** Pendente
**Responsáveis:** OBC-DH e coordenação

A execução dos novos ensaios será encaminhada ao setor de testes; os responsáveis técnicos acima apoiam interfaces, condições e diagnóstico.

## Objetivo

Demonstrar a missão e consolidar evidências dos 12 requisitos funcionais e quatro não funcionais.

## Entrega

Executar a missão contínua de dez minutos com registros, TC/TM e operação autônoma. Consolidar consumo, precisão temporal, perda de comunicação, excesso UART, travamentos, interrupções de energia, recuperação e integridade. Conferir ICDs, ConOps, arquitetura CEFAST e regulamento; atualizar parâmetros, versões, pinagem e instruções conforme evidências. Relacionar cada requisito às implementações e aos ensaios, sem considerar implementação como requisito validado.

## Pré-requisitos

- **Tarefas:** [INT-008](../03-integracao/INT-008-supervisao-e-retomada.md), [TES-001](done/TES-001-escalonamento-i2c.md), [TES-002](TES-002-executor-i2c.md), [TES-003](TES-003-i2c-hardware.md), [TES-004](done/TES-004-uart-software.md), [TES-005](TES-005-uart-hardware.md), [TES-006](TES-006-sd-hardware.md), [TES-007](done/TES-007-dados-e-tempo.md), [TES-008](TES-008-integracao.md), [TES-009](TES-009-falhas-e-recuperacao.md)
- **Condições externas:** Obter referências vigentes e limites de missão com coordenação e setores.

## Definições e condições

- Confirmar revisão de requisitos: capa v1.5 de 25/05/2026 versus histórico v1.6 de 29/05/2026. Obter ConOps, ICDs, arquitetura lógica CEFAST e regulamento vigentes.
- Preservar IDs, entradas, resultados esperados e critérios em [REQUISITOS.md](../../.agents/REQUISITOS.md). O cenário de excesso UART permanece; a proposta anterior de convertê-lo para I2C ficou sem aplicação com a decisão atual.
- Obter limites de resposta/recuperação, resolução/erro temporal, consumo e integridade para avaliar resultados. Não marcar testes como concluídos por decisão documental.

## Critérios de conclusão

Todos os requisitos do catálogo possuem evidências suficientes contra seus critérios e limites, com falhas e restrições resolvidas pelos responsáveis. Preservar os ensaios originais e complementar autonomia além da resposta a TC. REQ-OBC-013 permanece proposta do DP, pendente de incorporação formal; acompanhar missão antes dos ensaios ambientais e Fit-Check sem alterar os 16 requisitos de origem.

## Prontidão e preparação para o setor de testes

A missão completa ainda não está liberada: INT-008 e ensaios predecessores permanecem abertos. Pode-se preparar procedimento, rastreabilidade e reunir evidências já existentes sem aguardar hardware. Confirmar versões de ConOps/ICDs/regulamento, limites de resposta/recuperação, precisão temporal, cota EPS e critérios de integridade antes de executar.

## Checklist da missão

- [ ] **Configuração de referência:** registrar revisão Git, artefatos, dependências efetivamente resolvidas, montagem e versões dos documentos aprovados. Não inferir plataforma fixada a partir de um build antigo.
- [ ] **Liberação:** conferir as tarefas predecessoras e cada cenário TES-008/TES-009, com limitações resolvidas pelos responsáveis. Resultados de software ou bancada simulada não aprovam ensaios físicos por inferência.
- [ ] **Procedimento de 600 s:** executar a missão contínua de dez minutos no perfil acordado, com registros, TC/TM e operação autônoma. Definir coleta e critérios antes da execução; pedidos manuais isolados de TM não demonstram autonomia da missão.
- [ ] **Análise posterior:** extrair os registros de forma reproduzível e correlacionar dados, boots, referências, falhas, perdas e efeitos das atuações. Conferir que todos os critérios existentes e limites acordados foram avaliados.
- [ ] **Condição pré-ambiental:** registrar demonstração de missão anterior aos ensaios ambientais/Fit-Check conforme referências vigentes. REQ-OBC-013 permanece proposta, sem ampliar por esta revisão o catálogo original.

## Checklist de cobertura dos requisitos

Cada item exige referência à evidência e comparação com o critério de origem; não basta indicar a implementação. Usar a [matriz de rastreabilidade](../README.md#cobertura-dos-requisitos), preservando associações, entradas e resultados de REQUISITOS.md.

- [ ] **REQ-OBC-001:** preservar o teste de TC/tempo de execução associado no PDF e acrescentar demonstração de autonomia durante a missão.
- [ ] **REQ-OBC-002:** medir recebimento/execução e resposta dos comandos contra limites acordados.
- [ ] **REQ-OBC-003:** inspecionar estrutura, integridade e ordenação de status, energia e atitude da TM.
- [ ] **REQ-OBC-004:** comprovar modos e transições previstos no ConOps.
- [ ] **REQ-OBC-005:** comprovar armazenamento estruturado e extração/interpretação reproduzíveis.
- [ ] **REQ-OBC-006:** demonstrar timestamp correto e legível na TM a partir de ADS-B simulado; manter explícita a pendência de cada mensagem quando aplicável.
- [ ] **REQ-OBC-007:** comprovar detecção de falha e reinicialização automática no prazo determinado.
- [ ] **REQ-OBC-008:** comprovar inicialização automática após energização e comportamento de reset.
- [ ] **REQ-OBC-009:** comprovar registro de falhas críticas e problemas operacionais.
- [ ] **REQ-OBC-010:** comprovar horário/validade temporal dos eventos sem UTC inventado.
- [ ] **REQ-OBC-011:** comprovar identificação de perda de comunicação com os critérios dos subsistemas.
- [ ] **REQ-OBC-012:** comprovar prioridades de tarefas críticas em disponibilidade energética reduzida.
- [ ] **REQ-NF-OBC-001:** comparar consumo medido com a cota EPS aprovada.
- [ ] **REQ-NF-OBC-002:** comprovar estatísticas relevantes, suas unidades, contagens e extração.
- [ ] **REQ-NF-OBC-003:** comprovar quantidade de reinicializações, considerando limites da reserva NVS.
- [ ] **REQ-NF-OBC-004:** comprovar integridade em operação nominal e reinicializações inesperadas, incluindo interrupção durante escrita.

Usar o formato de [registro por cenário](../README.md#registro-por-cenário). Falha ou ausência de evidência impede aprovação do requisito correspondente; não alterar resultado esperado para completar a cobertura.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-001 a REQ-OBC-012; REQ-NF-OBC-001 a REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- [tt-c/ENSAIOS.md](../../../tt-c/ENSAIOS.md)
- Origem no planejamento anterior: P-MISSAO, P-VALIDACAO.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
