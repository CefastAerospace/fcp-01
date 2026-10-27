# INT-001 — Configuração base do ambiente OBC

**Estado:** Concluída
**Responsáveis:** OBC-DH

## Objetivo

Disponibilizar a configuração base de compilação e organização do firmware.

## Entrega

Ambiente obc para ESP32/Arduino, diretórios de fontes/headers/testes, entrada main.cpp e configuração local do clangd. Bibliotecas e versões dos periféricos integrados serão tratadas nas tarefas consumidoras.

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Nenhuma para a configuração base já existente.

## Critérios de conclusão

Configuração base presente e documentada; não implica placa confirmada, funcionalidades integradas ou dependências de periféricos instaladas.

## Evidências

Configuração inventariada em 07/10/2026; reorganização documental em 11/10/2026 e contratos consolidados em 14/10/2026. O commit bdfa48f, de 20/10/2026, registra build e geração da base do editor. Não houve nova compilação ESP32 nesta reorganização. Versões ainda não estão fixadas e não há lib_deps de periféricos no ambiente OBC.

## Referências

- [obc-dh/platformio.ini](../../../platformio.ini)
- [obc-dh/.clangd](../../../.clangd)
- [obc-dh/src/main.cpp](../../../src/main.cpp)
- [obc-dh/.agents/FUNCIONAMENTO_PIO.md](../../../.agents/FUNCIONAMENTO_PIO.md)
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
