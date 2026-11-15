# TES-011 — Execução reproduzível das regressões OBC

**Estado:** Pendente
**Responsável pela ferramenta e execução de desenvolvimento:** OBC-DH

## Objetivo

Disponibilizar um comando reproduzível para executar as regressões nativas existentes, sem alterar suas entradas ou expectativas.

## Entrega

- Inventariar os 18 arquivos C++ e 17 roteiros Python diretamente em `tests/` na revisão de 14/11/2026. Classificar executáveis nativos, geradores/verificadores, dependências entre ferramentas, roteiros USB e versões históricas; contagem de arquivos não é contagem de casos.
- Reutilizar os comandos de compilação, flags, sanitizadores e verificações existentes nos guias e tarefas. Preservar a correção controlada de ArduinoJson e as versões vigentes; não baixar bibliotecas silenciosamente ou adicionar framework.
- Criar uma entrada única de execução nativa, com seleção explícita dos conjuntos suportados, resultados por verificação e saída diferente de zero em falha ou dependência obrigatória ausente. Não classificar teste não executado como aprovado.
- Usar diretório temporário próprio para artefatos; registrar versão, comandos, ambiente e resultados em saída preservável. Não sobrescrever evidências anteriores.
- Documentar build ESP32 e roteiros USB compatíveis separadamente. O comando nativo não faz upload, não acessa serial e não executa roteiros históricos contra firmware incompatível.

## Pré-requisitos

- **Tarefas/capacidades:** verificações e comandos atuais de COM/DAD/INT e TES-001/004/007. Não exige conclusão das integrações futuras nem placa.
- **Condições externas:** compilador C++ e Python disponíveis; dependências locais efetivamente usadas identificadas. `pio test` no ambiente `obc` não substitui os executáveis nativos.

## Critérios de conclusão

Um comando reproduz os conjuntos nativos documentados; cada teste aplicável executa com suas expectativas originais. Demonstrar também propagação de falha e diagnóstico de dependência ausente em ambiente isolado, sem modificar testes de produção para provocar sucesso. Registrar comandos, revisão, resultado e limitações. Execução de desenvolvimento não aprova missão ou ensaio físico.

## Referências e comprovação

- [Guia de ferramentas](../../tools/README.md) e [guia PlatformIO](../../.agents/FUNCIONAMENTO_PIO.md).
- TES-001/004/007 conservam seus históricos. [TES-002](TES-002-executor-i2c.md) mantém a comprovação FreeRTOS, e TES-003/005/006/008/009/010 mantêm seus cenários físicos/integrados.

## Evidências

Ferramenta ainda não implementada; esta revisão apenas criou a tarefa.
