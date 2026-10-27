# TES-006 — SD: operações e falhas em hardware

**Estado:** Pendente
**Responsáveis:** OBC-DH e EPS

## Objetivo

Comprovar leitura, gravação e extração de arquivos na montagem real.

## Entrega

Exercitar escrita/leitura, sincronização/fechamento, extração, cartão cheio/indisponível e limites de buffer com conteúdo conhecido. Registrar perdas e efeitos sobre arquivos/FAT32. Interrupções durante escrita serão exercitadas em [TES-009](TES-009-falhas-e-recuperacao.md); a interpretação dos registros de missão está em [TES-007](TES-007-dados-e-tempo.md)/[TES-008](TES-008-integracao.md).

## Pré-requisitos

- **Tarefas:** [COM-005](../01-comunicacao/COM-005-armazenamento-sd.md)
- **Condições externas:** Cartão, montagem, alimentação e critérios de integridade definidos.

## Critérios de conclusão

Conteúdo extraído corresponde ao conteúdo de teste conforme os critérios acordados, falhas são observáveis e limites estão registrados. Não concluir integridade em queda de energia apenas por fechamento normal.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
