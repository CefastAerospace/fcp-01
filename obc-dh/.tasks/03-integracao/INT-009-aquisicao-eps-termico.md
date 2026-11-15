# INT-009 — Aquisição e integração EPS/Térmico

**Estado:** Pendente
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos contratos:** EPS e Térmico

## Objetivo

Integrar os dados de energia e temperatura ao OBC, com disponibilidade e erros explícitos.

## Origem e limites

Escopo extraído da [INT-005](INT-005-sensores-e-adcs.md) na revisão documental de 14/11/2026. Aquisição não implementa proteção térmica ou energética; as ações pertencem à [INT-010](INT-010-protecoes-energia-termico.md). A revisão não implementou drivers nem executou ensaios.

## Entrega

- Reutilizar EPS_INA219_Standard e integrar os 3x INA219 pelo executor I2C. A biblioteca INA219 não implementa aquisição/proteção dos LM75A.
- Integrar aquisição dos 5x LM75A conforme o contrato do setor, sem criar valores fictícios quando um dispositivo estiver indisponível.
- Definir com os produtores unidades, validade, localização das temperaturas, origens/códigos e publicação em registros/TM, reutilizando modelos existentes.
- Mapear erros dos drivers em `ExecuteChecked`, concluir transações antes do retorno do callback e integrar reconfiguração após recuperação do barramento.
- Conciliar medição solar por ADC nos PDFs versus sensores digitais propostos com EPS/ADCS; não reservar ADC silenciosamente. Confirmar se o BMS oferece interface de aquisição, sem presumir endereço.

## Pré-requisitos

- **Tarefas/capacidades:** COM-001/002/003, modelos DAD-001 e coordenação de boot existente da INT-003. O encerramento do boot de voo inteiro não impede preparar a aquisição contra contratos disponíveis.
- **Condições externas:** inventário, montagem, endereços finais de sete bits, grandezas/unidades, posições térmicas, periodicidade e limites dos callbacks fornecidos por EPS/Térmico. `0x40`/`0x41` e `0x48` são propostas anteriores, não mapa completo aprovado.
- Parâmetros de desenvolvimento provisórios seguem o [registro comum](../README.md#parâmetros-provisórios); não estabelecem limites de proteção.

## Verificação de desenvolvimento

OBC mantém regressões para sucesso, dispositivo indisponível, leitura incompleta, erro/timeout, validade dos campos e reconfiguração após recuperação. Reutilizar o serviço e os testes existentes, sem duplicar drivers ou algoritmos dos setores.

## Critérios de conclusão

Aquisição integrada, acesso I2C coordenado, mapeamentos documentados, build e regressões pertinentes aprovados. Contrato necessário ainda indefinido mantém a entrega correspondente aberta. Medições por dispositivo e convivência física pertencem à TES-003/TES-008; sua aprovação não é condição de encerramento desta implementação.

## Encaminhamento para comprovação

- [TES-003](../04-testes/TES-003-i2c-hardware.md): operações reais, frequência, duração e falhas por dispositivo.
- [TES-008](../04-testes/TES-008-integracao.md): dados de energia/temperatura na TM e convivência integrada.
- [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md): recuperação física e efeito sobre os consumidores.

## Evidências

Nenhuma implementação ou aprovação registrada por esta revisão. Registrar versão, contratos, parâmetros, regressões e limitações quando executar.
