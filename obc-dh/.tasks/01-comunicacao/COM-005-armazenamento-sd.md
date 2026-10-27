# COM-005 — SD: gravação, leitura, extração e recuperação

**Estado:** Pendente
**Responsáveis:** OBC-DH e EPS

## Objetivo

Oferecer armazenamento de arquivos com acesso coordenado e falhas observáveis.

## Entrega

Reutilizar SD e uma instância HSPI própria. Implementar abertura, escrita, leitura, extração, sincronização/fechamento e tratamento de cartão cheio/indisponível. Definir recuperação de arquivos incompletos e do sistema de arquivos. O formato do conteúdo pertence a [DAD-001](../02-dados-e-tempo/DAD-001-modelos-e-formatos.md)/[DAD-003](../02-dados-e-tempo/DAD-003-registros-e-interpretacao.md); não duplicar esse contrato no driver. O rádio é integrado em [INT-004](../03-integracao/INT-004-ttc-e-umbilical.md).

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Definir parâmetros do SD, alimentação, retenção e tratamento de indisponibilidade com EPS e coordenação.

## Definições e condições

- Decisão conciliada entre pinagem OBC e mapa compartilhado: LoRa/VSPI, SD/HSPI; inversão anterior é histórica. Conferir esquema, montagem e boot/reset; encerramento documental não conclui validação elétrica.
- Definir formato persistente, FAT32 citado nos PDFs, estatísticas, retenção, extração e escrita/sincronização/fechamento. Acordar SD cheio/indisponível, buffers limitados e como sinalizar perdas.
- Definir interrupções de alimentação e critérios de integridade; fechamento normal ou append não garante recuperação. Considerar registro incompleto e sistema de arquivos, não apenas conteúdo JSON.

## Critérios de conclusão

Operações e limites documentados e implementados, incluindo sinalização de perdas e recuperação. Compilar e conferir os caminhos de erro disponíveis. Integridade após interrupção de alimentação depende de [TES-006](../04-testes/TES-006-sd-hardware.md)/[TES-009](../04-testes/TES-009-falhas-e-recuperacao.md).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-REGISTROS, P-SPI.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
