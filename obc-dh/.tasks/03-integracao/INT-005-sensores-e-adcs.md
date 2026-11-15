# INT-005 — Integração ADCS, encoder e FOC

**Estado:** Pendente
**Responsável pela implementação OBC:** OBC-DH
**Responsável pelos algoritmos e limites:** ADCS

## Objetivo

Integrar atitude, luminosidade, encoder e controle ADCS no ESP32 central, preservando algoritmos e acesso coordenado ao I2C.

## Redistribuição de 14/11/2026

O escopo anterior incluía aquisição EPS/Térmico. Essa entrega e seus encaminhamentos DAD-001/COM-003 passam à [INT-009](INT-009-aquisicao-eps-termico.md). Esta INT conserva os consumidores ADCS e a conciliação da medição solar com a INT-009. Nenhuma integração foi implementada ou encerrada por esta revisão.

## Entrega

Reutilizar drivers e algoritmos ADCS; encaminhar consumidores I2C pelo serviço. Integrar aquisição de atitude, encoder e luminosidade, comandos, estado e falhas no firmware central. Não duplicar setup()/loop() nem reimplementar algoritmos. Preservar atendimento FOC independente de operações demoradas de rádio/SD. Integrar inicialização e falhas ao boot existente.

## Pré-requisitos

- **Tarefas/capacidades:** COM-001/002/003, INT-002 e coordenação de boot da INT-003. A capacidade de bancada já implementada pode ser usada antes do fechamento do boot de voo; dependências elétricas para atuação permanecem obrigatórias.
- **Condições externas:** inventário/montagem ADCS, aquisição, orientação, comandos, status, condições de atuação e limites FOC/I2C acordados. EPS/Térmico fornecem seus dados na INT-009; não bloquear aquisição ADCS independente por implementação desses produtores.

## Definições e condições

- A bancada ADCS ainda usa `Wire` diretamente. Integrar todos os consumidores ADCS ao executor e coordenar sensores/encoder sem transações intercaladas. Timeout de transferência não limita um callback completo nem recupera sozinho o barramento.
- Conferir MPU9250 `0x68` com AD0 em GND; BH1750 #1 `0x23` com ADDR em GND e #2 `0x5C` com ADDR em VCC. Confirmar orientação e parâmetros de aquisição com ADCS.
- Encoder AS5600/I2C `0x36` está definido e usado no código ADCS; conferir montagem/aquisição coordenada.
- Execução no mesmo ESP32 confirmada. Integrar firmware sem duplicar entradas; conferir o papel da interface web na bancada e na missão.
- Conciliar fases 27/26/25 do ADCS com U/V/W = 25/26/27 do OBC. A revisão documental não altera código de fases.
- Definir frequência/latência FOC, aquisição e missão, prioridades/afinidade, sincronização I2C e atuação por modo. Rádio/SD não podem impedir atendimento do controle.
- Conciliar medição solar ADC nos PDFs versus sensores digitais com ADCS/EPS na INT-009; não reservar ADC silenciosamente.

## Encaminhamentos recebidos

- **DAD-001:** acordar mapeamento, unidades, disponibilidade, origens/códigos e dados ADCS. Campos de energia/temperatura e posições térmicas passam à INT-009.
- **COM-003:** acordar frequência, latência e efeito da recuperação nos consumidores/encoder ADCS. Mapear erros em `ExecuteChecked`, prazos por pedido, conclusão de transações antes do retorno e reconfiguração após recuperação. A supervisão do serviço fica na INT-008; o caso de driver sem retorno, na INT-012.

## Verificação de desenvolvimento

OBC mantém regressões para acesso exclusivo, encaminhamento de resultados/falhas, dados inválidos, disponibilidade e reconfiguração. Preservar algoritmos ADCS e verificar build integrado, sem interpretar mocks como medição FOC.

## Critérios de conclusão

Consumidores ADCS integrados e acesso a Wire coordenado, contratos necessários documentados, build e regressões pertinentes aprovados. Registrar versões, parâmetros, estados e falhas. Comprovação física de sensores/FOC e coexistência com rádio/SD pertence às TES-003/008/009, sem condicionar o encerramento desta implementação à aprovação desses ensaios.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-003, REQ-OBC-011. Implementação não equivale a requisito validado.
- [ADCS](../../../adcs/README.md), [pinagem](../../.agents/PINAGEM.md), [arquitetura](../../.agents/ARCHITECTURE.md).
- [TES-003](../04-testes/TES-003-i2c-hardware.md), [TES-008](../04-testes/TES-008-integracao.md), [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md).
- Origem no planejamento anterior: P-ADCS, P-BARRAMENTO, P-ENDERECOS, P-EPS-ELETRICA; parcela EPS/Térmico encaminhada à INT-009.
- [Painel](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
