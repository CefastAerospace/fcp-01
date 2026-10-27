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
- Binário compacto com SeqID + NACK está decidido. Definir taxa nominal/rajadas, buffers/retenção, controle de fluxo e política de repetição/perdas; script atual não implementa transporte. Enquadramento e versão do enlace pertencem a esta tarefa; os modelos internos ficam em [DAD-001](../02-dados-e-tempo/DAD-001-modelos-e-formatos.md) e a bancada em [TES-005](../04-testes/TES-005-uart-hardware.md).
- Formato de voo UART decidido: binário compacto (~25–35 bytes estimados), SeqID + NACK; JSON do Payload é log de bancada. Definir campos/tipos/unidades, versão, tamanho máximo, delimitação, timeout, integridade, retenção/repetição e recuperação de mensagens incompletas/inválidas.
- Definir serialização e ordem dos bytes; memória bruta de uma struct não é representação portável. Preservar contrato TT&C e significado dos campos do Payload. JSON por linha é o enquadramento umbilical, não o transporte UART da missão.

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
