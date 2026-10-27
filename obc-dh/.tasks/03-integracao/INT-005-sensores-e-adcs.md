# INT-005 — Integração de sensores, encoder e ADCS

**Estado:** Pendente
**Responsáveis:** OBC-DH, ADCS, EPS e Térmico

## Objetivo

Integrar aquisição e controle no ESP32 central, preservando algoritmos e acesso coordenado ao I2C.

## Entrega

Reutilizar drivers dos setores, incluindo EPS_INA219_Standard, e encaminhar consumidores I2C pelo serviço. Integrar LM75A conforme contrato próprio; biblioteca INA219 não implementa proteção térmica. Conferir aquisição de energia, atitude, encoder e medição solar. Não duplicar setup()/loop() ou reimplementar algoritmos. Preservar atendimento FOC independente de operações demoradas de rádio/SD.

## Pré-requisitos

- **Tarefas:** [COM-001](../01-comunicacao/done/COM-001-servico-i2c.md), [COM-002](../01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md), [INT-002](done/INT-002-pinagem-en-motor.md), [INT-003](INT-003-inicializacao-central.md)
- **Condições externas:** Confirmar inventário, montagem, aquisição e limites FOC com ADCS, EPS e Térmico.

## Definições e condições

- Integrar inicialização no boot e todos os consumidores ao serviço; a bancada ADCS ainda usa `Wire` diretamente. Validar concorrência e falhas do executor, limites das operações e recuperação de SDA/SCL presos. Timeout de transferência não limita um callback completo nem recupera sozinho o barramento. Coordenar sensores e encoder sem interferir no atendimento FOC; limites dependem de [INT-005](INT-005-sensores-e-adcs.md).
- Conferir endereços de 7 bits e montagem: MPU9250 `0x68` com AD0 em GND; BH1750 #1 `0x23` com ADDR em GND e #2 `0x5C` com ADDR em VCC. Confirmar orientação e parâmetros de aquisição com ADCS.
- Encoder AS5600/I2C `0x36` está definido no projeto e utilizado no código ADCS; conferir montagem e aquisição coordenada. Conferir endereços finais/configuração física dos 3x INA219 e 5x LM75A com EPS/Térmico; `0x40`/`0x41` e `0x48` são propostas anteriores, não mapa completo aprovado. Confirmar se o BMS oferece interface de aquisição e seus parâmetros, sem presumir endereço I2C.
- Execução no mesmo ESP32 foi confirmada. Definir integração do firmware de bancada sem duplicar `setup()`/`loop()`, preservando algoritmos, comandos e responsabilidades; conferir papel da interface web na bancada e na missão.
- Conciliar fases 27/26/25 do ADCS com U/V/W = 25/26/27 do OBC. Alterações de código serão feitas em implementação separada.
- Encoder AS5600 por I2C `0x36` está definido; confirmar ligação/montagem e acesso coordenado. Referências antigas a SPI são históricas.
- Definir frequência/latência FOC, aquisição e missão, prioridades/afinidade, sincronização do I2C, comandos, status, falhas e atuação por modo. Operações de rádio/SD não podem impedir atendimento do controle.
- Resolver medição solar: ADC nos PDFs versus sensores digitais propostos. Definir responsável, grandezas e interface sem reservar ADC silenciosamente.

## Critérios de conclusão

Consumidores integrados e acesso a Wire coordenado, com build e verificações locais. Documentar versões, parâmetros, estados e falhas. Medições físicas e coexistência com rádio/SD pertencem a [TES-003](../04-testes/TES-003-i2c-hardware.md)/[TES-008](../04-testes/TES-008-integracao.md).

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-003, REQ-OBC-011. Implementação não equivale a requisito validado.
- [adcs/src/adcs.cpp](../../../adcs/src/adcs.cpp)
- [adcs/src/sensors.cpp](../../../adcs/src/sensors.cpp)
- [adcs/src/motor_control.cpp](../../../adcs/src/motor_control.cpp)
- [eps-tc/EPS_INA219_Standard/README.md](../../../eps-tc/EPS_INA219_Standard/README.md)
- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- Origem no planejamento anterior: P-ADCS, P-BARRAMENTO, P-ENDERECOS, P-EPS-ELETRICA.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
