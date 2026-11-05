# COM-004 — Transporte UART e recepção validada do Payload

**Estado:** Pendente
**Responsáveis:** OBC-DH e Payload

## Objetivo

Receber mensagens completas e válidas do Payload e entregar dados utilizáveis à integração.

## Entrega

Implementar transporte, delimitação, interpretação e validação no ESP32, com buffers limitados e tratamento de perdas. Acordar e acompanhar a implementação da ponta Raspberry Pi pelo setor Payload. O receptor deve poder ser exercitado com entrada simulada antes de integrar o main.

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Fechar enquadramento e parâmetros com Payload; identificar responsável pela ponta Raspberry Pi.

## Definições e condições

- Alocação de projeto decidida: ESP32 TX 17 / RX 16, 115,2 kbps 8N1, conforme Payload e mapa compartilhado. Confirmar GPIOs/porta no Pi, variante das placas, disponibilidade de 16/17, TX/RX cruzados, GND, 3,3 V e comportamento com um lado desligado; decisão não comprova montagem.
- Habilitar a UART no Raspberry Pi e verificar console serial/Bluetooth ou outro uso da porta, preservando configuração do runbook do Payload.
- Em 05/11/2026, o responsável confirmou snapshots JSON completos diretamente pela UART, conforme [payload.json](../../payload.json), substituindo a decisão de binário compacto. O script atual do repositório não produz esse formato nem implementa transporte. Os modelos internos ficam em [DAD-001](../02-dados-e-tempo/DAD-001-modelos-e-formatos.md) e a bancada em [TES-005](../04-testes/TES-005-uart-hardware.md).
- Definir campos/tipos/unidades com o produtor, versão, tamanho máximo em bytes, delimitação, timeout, integridade, taxa nominal/rajadas, buffers/retenção, controle de fluxo e política de repetição/perdas. Os limites das structs não definem o tamanho do quadro. O exemplo compacto tem 4782 bytes para sete aeronaves: pelo menos 0,415 s em 115,2 kbps 8N1, sem pausas ou overhead adicional. Dimensionar com o máximo acordado, não apenas com a amostra.
- Não interpretar `id` como SeqID sem confirmação. Sequência/gaps, NACK e repetição constam dos cenários existentes e precisam de compatibilização explícita; esta revisão não altera entradas/resultados esperados de TES-004/TES-005. Resolver também como preservar cada mensagem ADS-B quando o produtor envia estados acumulados.
- Publicar a lista somente após o snapshot completo e válido; lista vazia limpa o estado ativo, enquanto erro preserva o estado anterior. Campos ausentes não herdam validade do snapshot anterior. Conferir ICAO da chave/atributo, strings numéricas completas, unidades e tipos de extras conforme o contrato. Excesso de capacidade exige falha explícita, sem truncamento.
- Memória bruta de uma struct não é representação portável. Preservar contrato TT&C e significado dos campos do Payload. JSON por linha é o enquadramento umbilical; o enquadramento UART do Payload permanece a definir.

## Critérios de conclusão

Contrato documentado e receptor implementado, com tratamento de mensagens parciais/consecutivas, inválidas e acima do limite. Verificar o comportamento de software. A troca física ESP32–Pi é comprovada em [TES-005](../04-testes/TES-005-uart-hardware.md).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-005, preparação de REQ-OBC-006 e REQ-OBC-011. Implementação não equivale a requisito validado.
- [payload/README.md](../../../payload/README.md)
- [payload/telemetry.py](../../../payload/telemetry.py)
- [payload/raspberry-pi-runbook.md](../../../payload/raspberry-pi-runbook.md)
- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- Origem no planejamento anterior: P-DADOS, P-PAYLOAD.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
