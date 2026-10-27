# Execução e pendências do OBC-DH

Este checklist organiza cinco entregas da [arquitetura vigente](ARCHITECTURE.md). [REQUISITOS.md](REQUISITOS.md) preserva os 16 requisitos, critérios e cenários de validação anteriores. Pendências antes separadas foram reunidas aqui; uma decisão não comprova implementação ou teste.

## 1. Estado consolidado em 27/10/2026

Inspeção do checkout até `f75610c`, incluindo o serviço I2C acrescentado em `bdfa48f` (20/10/2026), e execução do teste nativo de escalonamento. O serviço I2C está implementado, mas `setup()`/`loop()` continuam vazios: não há inicialização ou integração operacional no firmware central. Não houve nova compilação ESP32, upload ou teste de hardware nesta revisão. As conclusões documentais anteriores permanecem como evidências de inspeção, não de funcionamento.

| Área | Evidência e reutilização | Trabalho restante |
|---|---|---|
| Ambiente OBC | `platformio.ini` configura ESP32/Arduino, diretórios e ambiente `obc`; `.clangd` ajusta o editor. O commit `bdfa48f` registra compilação do firmware e geração da base do editor. | Confirmar placa real e fixar versões validadas. Compilação ESP32 não foi repetida nesta revisão. Não há dependências de periféricos declaradas. |
| Firmware OBC | `src/main.cpp` tem `setup()`/`loop()` vazios; `headers/Pinout.h` contém constantes, inclusive UART2 em 16/17. | Inicialização, comunicação, dados, tempo, SD e missão. Constantes não comprovam montagem nem configuração. |
| Serviço I2C | `headers/I2cBus.h`, `headers/I2cScheduling.h` e `src/I2cBus.cpp`: inicialização a 400 kHz, timeout de transferência de 20 ms, tarefa exclusiva, fila limitada, mutex e prioridade com envelhecimento. | Chamar `i2c::Init()` no boot e integrar consumidores via `i2c::Execute()`; conferir montagem, parâmetros elétricos, recuperação do barramento e prazos do FOC. |
| TT&C | Biblioteca SX1276 SF7/921 MHz, JSON, CRC do rádio, filas, ACK prioritário, deduplicação, tarefa Core 0, umbilical, saúde/reset e beacon. Exemplo `examples/link-test/`, ponte `ground-station/`, app de notebook `ground-station/app/` e testes próprios. | Integrar via `Init()`/`Start()`, `ReceiveTelecommand()`, `SendAck()`, `Send()`/`SendTelemetry()` e validar fluxo de missão. Sem fragmentação/CRC16 de aplicação; referência antiga a `gs/` encerrada. |
| ADCS | Firmware de bancada em `adcs/`, com sensores, motor/encoder, missões, comandos e interface web. Bibliotecas SimpleFOC, MPU9250_WE e BH1750 declaradas. `PIN_EN = 12` já coincide com a pinagem OBC. | Integrar no mesmo ESP32, preservando algoritmos, conciliando entradas, fases, inicialização e atendimento FOC. A bancada ainda inicializa e acessa `Wire` diretamente. |
| Payload | `payload/telemetry.py` coleta TCP do dump1090, organiza `AircraftTelemetry`, associa UTC e grava JSON localmente. | Implementar a ponta UART no Raspberry Pi e acordar o contrato; log local não comprova transporte ou timestamp do OBC. |
| EPS / Térmico / comum | `eps-tc/EPS_INA219_Standard/` oferece biblioteca INA219, exemplo OBC e guia. Projeto prevê 3x INA219 e 5x LM75A; mapa compartilhado consolidado e `common/protocol/ttc-json.md` existente. | Conferir API/unidades e integrar drivers conforme contrato EPS; endereços finais, LM75A, montagem e proteção térmica ainda precisam de definição/validação. |
| Validação e referências | `tests/I2cSchedulingCheck.cpp` verifica a política I2C sem hardware; execução aprovada em 27/10/2026. Arquitetura, pinagem e requisitos transcritos existem. Ensaios próprios do enlace estão em [tt-c/ENSAIOS.md](../../tt-c/ENSAIOS.md), usando exemplo que simula o OBC. | Teste nativo não cobre executor FreeRTOS, transações físicas ou integração. Ensaios TT&C não comprovam requisitos do OBC central. PDFs, ConOps, ICDs e regulamento citados não estão presentes no repositório. |

### Evidências documentais concluídas

- [x] Inventariar funções, classes, drivers, utilitários, scripts e dependências existentes antes de propor novas ferramentas. Evidência: inspeção de 07/10/2026, atualizada nesta revisão para TT&C, Payload e ADCS.
- [x] Configurar o ambiente C++ e as dependências conforme a arquitetura existente, reutilizando a infraestrutura disponível. Evidência de 07/10/2026 limitada à configuração base em `platformio.ini`, `.clangd` e entrada `src/main.cpp`; não comprova compilação, placa ou periféricos.
- [x] Atualizar este checklist de acordo com as evidências e manter pendências restantes identificadas. Evidência: revisão de 07/10/2026 e reorganização documental atual, com inventário e dependências preservados e atualizados.

Consolidação de 14/10/2026: decisões de interfaces incorporadas dos documentos remotos, contrato TT&C conferido no código e orçamento de rádio recalculado em `DEFINICOES_OBC_DP.md`. Isso não encerra testes nem integração. `PENDENCIAS_ARQUITETURAIS.md` foi incorporado a este checklist e removido; referências históricas e validação permanecem em `REQUISITOS.md`.

### Evidência do teste nativo em 27/10/2026

Executado dentro de `obc-dh/`, sem modificar entradas, asserts ou resultados esperados:

```bash
g++ -std=c++11 -Wall -Wextra -Werror tests/I2cSchedulingCheck.cpp -o /tmp/fcp01-i2c-check
/tmp/fcp01-i2c-check
```

Compilação e execução encerraram com código zero. O teste cobre prioridade, promoção por conclusões, saturação, desempate por chegada, fila vazia, vencimento exato e estouro do contador em ms. Não exercita concorrência FreeRTOS, fila cheia do executor, dispositivos ou recuperação elétrica.

## 2. Pendências e dependências

Uma pendência bloqueia somente a implementação que depende dela. Modos completos não impedem desenvolver recepção UART e registros; condições desconhecidas de atuação impedem acionar motor ou Burn Wire. Estados iniciais, validação de entradas e tratamento básico de erros acompanham cada entrega.

<a id="p-i2c"></a>
### P-I2C — Histórico encerrado para o Payload

**Decisão:** responsável pelo OBC-DH substituiu I2C do Payload por UART na revisão atual. Foram encerradas as questões de alvo/controlador, múltiplos controladores, arbitragem, endereço e compartilhamento do Payload com sensores. Isso não conclui implementação ou teste. Histórico e proposta `0x80` estão em [REQUISITOS.md](REQUISITOS.md#histórico-de-decisões-de-interface); questões ativas seguem em P-PAYLOAD, P-BARRAMENTO e P-ENDERECOS.

<a id="p-payload"></a>
### P-PAYLOAD — UART OBC–Raspberry Pi

**Responsáveis:** OBC-DH e Payload. **Bloqueia:** UART da entrega 1 e recepção da entrega 2.

- Alocação de projeto decidida: ESP32 TX 17 / RX 16, 115,2 kbps 8N1, conforme Payload e mapa compartilhado. Confirmar GPIOs/porta no Pi, variante das placas, disponibilidade de 16/17, TX/RX cruzados, GND, 3,3 V e comportamento com um lado desligado; decisão não comprova montagem.
- Habilitar a UART no Raspberry Pi e verificar console serial/Bluetooth ou outro uso da porta, preservando configuração do runbook do Payload.
- Binário compacto com SeqID + NACK está decidido. Definir taxa nominal/rajadas, buffers/retenção, controle de fluxo e política de repetição/perdas; script atual não implementa transporte. Enquadramento/versão finais e bancada continuam abertos em P-DADOS.

<a id="p-barramento"></a>
### P-BARRAMENTO — I2C de sensores e encoder

**Responsáveis:** OBC-DH, ADCS, EPS e Térmico. **Bloqueia:** integração I2C das entregas 1 e 4.

- [x] Implementar `i2c::Init()` com SDA 21 / SCL 22, frequência de 400 kHz e timeout de 20 ms por transferência. Evidência: `headers/I2cBus.h` e `src/I2cBus.cpp`; ainda não chamado pelo boot.
- [x] Implementar atendimento exclusivo em tarefa FreeRTOS e API síncrona `i2c::Execute()`. Prioridade provisória da tarefa = 5 e pilha = 4096 bytes; callbacks devem ser finitos e curtos.
- [x] Implementar fila de oito posições estáticas, mutex dos metadados e semáforos de conclusão. A posição permanece ocupada até o chamador consumir o resultado.
- [x] Implementar cinco prioridades de solicitação (0–4), promoção por callback concluído e desempate pela ordem de chegada. Envelhecimento não altera prioridades FreeRTOS nem comprova prazos do FOC.
- [x] Implementar validação de argumentos/contexto e retornos de serviço não inicializado, fila cheia, timeout de fila e falha da operação. Espera padrão na fila = 1000 ms; operação iniciada é aguardada até terminar, preservando o contexto.
- [x] Implementar e executar o teste nativo da política em `tests/I2cSchedulingCheck.cpp`. Evidência e limites registrados na seção 1.
- Conferir controlador/dispositivos, SDA/SCL, compatibilidade dos 400 kHz, níveis, pull-ups, capacitância, comprimento e comportamento com periférico desligado. O Payload não participa desse barramento.
- Integrar inicialização no boot e todos os consumidores ao serviço; a bancada ADCS ainda usa `Wire` diretamente. Validar concorrência e falhas do executor, limites das operações e recuperação de SDA/SCL presos. Timeout de transferência não limita um callback completo nem recupera sozinho o barramento. Coordenar sensores e encoder sem interferir no atendimento FOC; limites dependem de P-ADCS.

<a id="p-enderecos"></a>
### P-ENDERECOS — Inventário dos dispositivos I2C

**Responsáveis:** OBC-DH, ADCS, EPS e Térmico. **Bloqueia:** dispositivos I2C das entregas 1 e 4.

- Conferir endereços de 7 bits e montagem: MPU9250 `0x68` com AD0 em GND; BH1750 #1 `0x23` com ADDR em GND e #2 `0x5C` com ADDR em VCC. Confirmar orientação e parâmetros de aquisição com ADCS.
- Encoder AS5600/I2C `0x36` está definido no projeto e utilizado no código ADCS; conferir montagem e aquisição coordenada. Conferir endereços finais/configuração física dos 3x INA219 e 5x LM75A com EPS/Térmico; `0x40`/`0x41` e `0x48` são propostas anteriores, não mapa completo aprovado. Confirmar se o BMS oferece interface de aquisição e seus parâmetros, sem presumir endereço I2C.
- RTC externo está fora da configuração atual; considerar sua instalação apenas se o método monotônico/LoRa falhar na validação temporal acordada. DS3231 histórico usa `0x68`, que conflita com a IMU; eventual reconsideração exige decisão elétrica sem mudar MPU9250 para `0x69` por suposição. Payload UART não recebe endereço I2C.

<a id="p-dados"></a>
### P-DADOS — Contratos, telemetria e telecomandos

**Responsáveis:** OBC-DH, Payload e TT&C. **Bloqueia:** modelos/recepção da entrega 2 e TC/TM/dados da entrega 4.

- Formato de voo UART decidido: binário compacto (~25–35 bytes estimados), SeqID + NACK; JSON do Payload é log de bancada. Definir campos/tipos/unidades, versão, tamanho máximo, delimitação, timeout, integridade, retenção/repetição e recuperação de mensagens incompletas/inválidas.
- Definir serialização e ordem dos bytes; memória bruta de uma struct não é representação portável. Preservar contrato TT&C e significado dos campos do Payload. JSON por linha é o enquadramento umbilical, não o transporte UART da missão.
- Definir status, temperatura, solicitações/respostas, significado de `val_flags`, validade de dados acumulados em instantes diferentes e comportamento de `seq_id` após reset. Manter distinção entre timestamp do Payload e recepção no OBC.
- Reutilizar `common/protocol/ttc-json.md` e `tt-c/include/TtcProtocol.h`: envelope TC no TT&C, `args`/condições no OBC e resposta `SendAck()`. A biblioteca deduplica oito TCs por 30 s; definir persistência/condições das atuações críticas após reset e fora da janela. Não presumir PUS; satélite e estação precisam da mesma versão.
- TM via `SendTelemetry()` e aeronaves agregadas via `BuildAircraft()`/`Send()`. Corpo JSON ≤245 bytes sem `n`, quadro final ≤255 bytes, sem fragmentação e CRC de hardware LoRa. ACK prioritário e TX sob demanda já estão definidos; acordar frequência/seleção e tratar falhas de geração/enfileiramento. Não substituir demonstração de timestamp na TM por outro teste; preservar o critério e discutir evidência correspondente.

<a id="p-tempo"></a>
### P-TEMPO — Referência temporal

**Responsáveis:** OBC-DH, com Payload, TT&C e coordenação. **Bloqueia:** precisão/formato temporal da entrega 2 e persistência temporal da entrega 3.

- Decisão: ms monotônicos por boot e referências absolutas pelo LoRa via `CMD_SET_TIME`/`set_time`, com reconstrução retroativa sem reescrever registros. RTC externo fica fora da configuração atual e só será reconsiderado se esse método não atender aos critérios temporais acordados; DS3231 permanece histórico.
- `args.unix` e `ts` do contrato TT&C estão em segundos Unix. Definir instante representado/recebido, conversão ms/s, resolução, erro/deriva admissível, periodicidade e validade; considerar quantização do horário transmitido e atraso no enlace. Não alterar silenciosamente o contrato ou inventar precisão.
- Definir identificação dos boots, persistência e contagem sem colisões após reset, inclusive com SD indisponível. Tratar duração, possível estouro do contador monotônico e representação dos tempos nos registros.
- Definir escolha/interpretação de referências sucessivas, ajuste inicial sem internet, análise posterior e indicação de horário desconhecido nos registros e na TM. Não inferir intervalo desligado usando apenas a referência do boot anterior.
- Definir o instante do timestamp OBC e preservar o horário produzido pelo Payload sem remover ou redefinir seu campo. O exemplo TT&C faz ajuste em memória e não comprova recuperação ou precisão.

<a id="p-burn-wire"></a>
### P-BURN-WIRE — Liberação da antena

**Responsáveis:** OBC-DH, Estruturas, EPS e TT&C. **Bloqueia:** saídas correspondentes da entrega 1 e acionamento/retomada da entrega 4.

- O mecanismo confirmado é MOSFET no GPIO 33 para queimar a retenção física das antenas. Remoto registra dois circuitos nichrome, 5 V / 4 Ω cada e orçamento de 2,5 A / 12,5 W transitório; conferir montagem, nível ativo, estados em boot/reset, duração máxima, autorização, energia e novas tentativas com Estruturas/EPS.
- Acionar fora de TX, usando `Ttc::HoldTransmit(true, wait_ms)` e confirmando seu retorno antes da atuação; definir retomada e tratamento de falhas. Esse interlock não autoriza a queima. Preservar ensaio mínimo de cinco ciclos bem-sucedidos registrado pelo TT&C, sem mudar critérios dos setores.
- A flag `IS_WIRE_BURNT` foi apenas sugerida; não é implementação aprovada nem convenção de nomes. Definir persistência, estado inicial, interrupção durante acionamento e duplicatas de comando; flag volátil perde valor após reset.
- Distinguir comando emitido de liberação física confirmada; definir sensor ou critério de sucesso. Se a antena for necessária para receber TC, acordar autorização da primeira liberação sem depender de um comando impossível de receber.

<a id="p-adcs"></a>
### P-ADCS — Integração no ESP32 central

**Responsáveis:** ADCS e OBC-DH. **Bloqueia:** saídas do motor da entrega 1 e integração/atuação da entrega 4; limita o acesso ao I2C.

- Execução no mesmo ESP32 foi confirmada. Definir integração do firmware de bancada sem duplicar `setup()`/`loop()`, preservando algoritmos, comandos e responsabilidades; conferir papel da interface web na bancada e na missão.
- [x] Conciliar EN no GPIO 12 entre `adcs/include/config.h` (`PIN_EN`) e `obc-dh/headers/Pinout.h` (`motor_en`). Evidência: inspeção do código em 27/10/2026; não comprova montagem ou estados em boot/reset.
- Conciliar fases 27/26/25 do ADCS com U/V/W = 25/26/27 do OBC. Alterações de código serão feitas em implementação separada.
- Encoder AS5600 por I2C `0x36` está definido; confirmar ligação/montagem e acesso coordenado. Referências antigas a SPI são históricas.
- Definir frequência/latência FOC, aquisição e missão, prioridades/afinidade, sincronização do I2C, comandos, status, falhas e atuação por modo. Operações de rádio/SD não podem impedir atendimento do controle.
- Definir se o alinhamento do motor no boot é permitido e em quais condições; confirmar a ação denominada “tumbling” nos requisitos versus “detumbling” nas demais referências.

<a id="p-spi"></a>
### P-SPI — Rádio e armazenamento

**Responsáveis:** OBC-DH e TT&C. **Bloqueia:** SPI da entrega 1, SD da entrega 3 e integração de rádio da entrega 4.

- Decisão conciliada entre pinagem OBC e mapa compartilhado: LoRa/VSPI, SD/HSPI; inversão anterior é histórica. Conferir esquema, montagem e boot/reset; encerramento documental não conclui validação elétrica.
- Rádio usa `SPI` global; SD exige instância própria HSPI. Parâmetros atuais TT&C: 921 MHz, SF7, BW 125 kHz, CR 4/6, preâmbulo 32, CRC do rádio e TX sob demanda. Preservar decisão TT&C; validar velocidades/coordenação com estação e não duplicar filas.

<a id="p-eps-eletrica"></a>
### P-EPS-ELETRICA — Placa, sensores e energia

**Responsáveis:** EPS, OBC-DH e ADCS. **Bloqueia:** hardware da entrega 1 e políticas energéticas/medição da entrega 4.

- Confirmar variante DevKit/módulo, conectores, USB/UART, disponibilidade dos GPIOs e flash/PSRAM. GPIOs 16/17 estão alocados à UART do Payload; conferir disponibilidade física antes da ligação.
- Conferir alimentação, GND, níveis, resistores e estados em reset de CS, RST, EN, PWM e Burn Wire. GPIOs 5, 12 e 15 são strapping pins; GPIO 35 não possui pulls internos.
- Resolver medição solar: ADC nos PDFs versus sensores digitais propostos. Definir responsável, grandezas e interface sem reservar ADC silenciosamente.
- Obter cota energética real, limiares, prioridades, baixo consumo, retorno e corte de alimentação de periféricos. Medir consumo da montagem e consumo residual do motor desabilitado; valores históricos não comprovam orçamento.

<a id="p-termico"></a>
### P-TERMICO — Thermal Watchdog

**Responsáveis:** EPS, Térmico e OBC-DH. **Bloqueia:** aquisição/proteção térmica da entrega 4.

- Projeto EPS/Térmico prevê 5x LM75A na bateria/pontos críticos, com alerta ao OBC; confirmar endereços, localização, aquisição, periodicidade, limites/retorno e responsável pela proteção. Não presumir implementação térmica a partir da biblioteca INA219 existente.
- Distinguir monitoramento de atuação térmica e watchdog de tarefas. Armazenar temperatura não comprova proteção; não presumir sinal dedicado.

<a id="p-missao"></a>
### P-MISSAO — Referências, modos e recuperação

**Responsáveis:** coordenação e setores. **Bloqueia:** somente comportamentos dependentes de ConOps/ICDs e limites correspondentes nas entregas 1, 4 e 5.

- Confirmar revisão de requisitos: capa v1.5 de 25/05/2026 versus histórico v1.6 de 29/05/2026. Obter ConOps, ICDs, arquitetura lógica CEFAST e regulamento vigentes.
- Integrar modos/transições e estado inicial registrados no README geral e no contrato TT&C, conferindo ConOps vigente. Preservar decisão TT&C de LOW-POWER com rádio em RX e beacon periódico só em safe-mode. Detalhar retomada, comandos permitidos e prioridades sem substituir decisões dos setores.
- Preservar limites TT&C de entrega TC ao OBC ≤200 ms e resposta ≤2 s; definir detecção/recuperação e critérios de saúde/tentativas por periférico/tarefa/OBC. Reutilizar estatísticas e reset do rádio existentes; ausência ADS-B não equivale à perda de comunicação.
- Considerar limites físicos: watchdog não recupera falha física do ESP32; não existe redundância física confirmada de OBC/armazenamento.

<a id="p-registros"></a>
### P-REGISTROS — Persistência e integridade

**Responsáveis:** OBC-DH, EPS e coordenação. **Bloqueia:** formato/recuperação da entrega 3 e retomada persistente da entrega 4.

- Definir formato persistente, FAT32 citado nos PDFs, estatísticas, retenção, extração e escrita/sincronização/fechamento. Acordar SD cheio/indisponível, buffers limitados e como sinalizar perdas.
- Definir contagem/identificação de boots e estados necessários à retomada, incluindo Burn Wire e referências temporais, sem reescrever dados já gravados.
- Definir interrupções de alimentação e critérios de integridade; fechamento normal ou append não garante recuperação. Considerar registro incompleto e sistema de arquivos, não apenas conteúdo JSON.

<a id="p-validacao"></a>
### P-VALIDACAO — Critérios e limites

**Responsáveis:** OBC-DH e coordenação. **Bloqueia:** comprovação quantitativa nas entregas correspondentes e encerramento da entrega 5.

- Preservar IDs, entradas, resultados esperados e critérios em [REQUISITOS.md](REQUISITOS.md). O cenário de excesso UART permanece; a proposta anterior de convertê-lo para I2C ficou sem aplicação com a decisão atual.
- Preservar associação do teste TC a REQ-OBC-001, registrando relação com REQ-OBC-002 e evidência complementar de autonomia sem substituir o teste original.
- Obter limites de resposta/recuperação, resolução/erro temporal, consumo e integridade para avaliar resultados. Não marcar testes como concluídos por decisão documental.

## 3. Cinco entregas

Os itens compostos abaixo permanecem abertos; configuração, inventário e avanço do serviço I2C estão registrados na seção 1 e em P-BARRAMENTO. A coincidência de EN no código está registrada em P-ADCS. Nenhum desses avanços conclui uma entrega integrada. Cada entrega reutiliza as soluções existentes e documenta parâmetros reais. Não é necessário resolver todas as pendências antes de iniciar trabalho independente.

### 1. Plataforma e interfaces

- [ ] Confirmar placa, alimentação e parâmetros elétricos necessários à interface em desenvolvimento; conferir pinagem e estados em boot/reset, sem atuação indevida de motor ou Burn Wire. P-EPS-ELETRICA, P-ADCS e P-BURN-WIRE.
- [ ] Inicializar automaticamente recursos e registrar falhas, com estado observável pela interface de bancada; implementar UART0 conforme contrato. REQ-OBC-008; P-MISSAO.
- [ ] Configurar UART nos dois lados e verificar troca básica com Payload; conferir I2C de sensores/encoder e parâmetros aprovados. P-PAYLOAD, P-BARRAMENTO e P-ENDERECOS.
- [ ] Inicializar SD e biblioteca TT&C com GPIOs conferidos, HSPI/VSPI, DIO0 e RST; documentar dependências/versões validadas. P-SPI.
- [ ] Compilar e validar as interfaces e boot/reset na placa; registrar evidências, mantendo falhas/limites tratados desde a inicialização.

### 2. Recepção, modelos e tempo

- [ ] Fechar contrato UART, implementar delimitação/validação e tratamento de mensagens incompletas, inválidas, rajadas e limite excedido. P-PAYLOAD e P-DADOS.
- [ ] Implementar tipos concretos em `headers/models/` conforme uso e contrato, documentando unidades/campos e sem duplicar protocolo TT&C. REQ-OBC-005.
- [ ] Implementar tempo monotônico em ms, identificação do boot, validade e referência `set_time`/`args.unix` em segundos; associar tempo aos dados/eventos e converter explicitamente para `ts` no TT&C. REQ-OBC-006 e REQ-OBC-010; P-TEMPO.
- [ ] Preservar timestamp do Payload e conferir comportamento sem sincronização, com referências sucessivas, reset e possível estouro do contador.

### 3. Registros persistentes

- [ ] Implementar escrita/leitura/extração documentadas de missão, TM, eventos, falhas, estatísticas e reinicializações. REQ-OBC-005, REQ-OBC-009, REQ-OBC-010 e REQ-NF-OBC-002/003; P-REGISTROS.
- [ ] Persistir referências de tempo e identificar boots para análise posterior sem alterar registros anteriores. P-TEMPO e P-REGISTROS.
- [ ] Implementar acesso coordenado, sincronização/fechamento, retenção e tratamento de SD cheio/indisponível, com limites de buffer e registro/sinalização de perdas.
- [ ] Implementar recuperação e demonstrar integridade após reset e interrupção durante escrita, incluindo registros incompletos e sistema de arquivos. REQ-NF-OBC-004; critérios anteriores permanecem em REQUISITOS.

### 4. Integração e missão

- [ ] Integrar ADCS ao firmware central preservando algoritmos; conciliar pinagem/inicialização, isolar atendimento FOC de rádio/SD e coordenar I2C. P-ADCS e P-BARRAMENTO.
- [ ] Integrar biblioteca INA219 existente, dados de EPS, atitude/sensores e térmico conforme contratos; implementar encoder/medição solar conforme decisões e pendências de montagem. P-ENDERECOS, P-EPS-ELETRICA e P-TERMICO.
- [ ] Integrar `Ttc::Init()`/`Start()` sem `Update()` concorrente; consumir TC com `ReceiveTelecommand()` e responder com `SendAck()`, preservando limites do setor.
- [ ] Compor TM via `SendTelemetry()` e estados ADS-B via `BuildAircraft()`/`Send()`, sem fragmentação; acordar seleção/frequência, validade temporal e falha de geração/enfileiramento; disponibilizar TC/TM na bancada. REQ-OBC-003; P-DADOS.
- [ ] Validar e executar TC conforme parâmetros/modo/condições, fornecer ACK/rejeição e tratar duplicatas sem repetir atuação indevida. REQ-OBC-002; P-DADOS e P-MISSAO.
- [ ] Implementar modos, transições, prioridades energéticas e ações acordadas, incluindo detumbling, Thermal Watchdog e Burn Wire; conferir retomada e estados persistentes. REQ-OBC-004/012 e REQ-NF-OBC-001.
- [ ] Integrar detecção de perda de comunicação, supervisão das tarefas críticas, watchdog e recuperação por periférico/tarefa/OBC, com registros de falhas/horários/reinicializações. REQ-OBC-007/009/010/011 e REQ-NF-OBC-003.
- [ ] Demonstrar operação sem intervenção manual, TC/TM e convivência ADCS/UART/I2C/SD/rádio contra os limites acordados. REQ-OBC-001; P-VALIDACAO.

### 5. Validação integrada

- [ ] Executar e registrar os critérios e cenários preservados em [REQUISITOS.md](REQUISITOS.md), cobrindo os 12 requisitos funcionais e quatro não funcionais.
- [ ] Reunir evidências de teste TC (associação original REQ-OBC-001), inspeção TM (REQ-OBC-003) e ADS-B simulado com timestamp na TM (REQ-OBC-006), além das verificações complementares propostas.
- [ ] Consolidar boot, modos, prioridades, consumo, perda de comunicação, excesso UART, travamento, interrupção de alimentação, recuperação e integridade contra limites definidos. P-VALIDACAO.
- [ ] Conferir ICDs, ConOps, arquitetura CEFAST e regulamento; atualizar decisões, pinagem, parâmetros, versões e instruções conforme evidências reais.

## 4. Evidências e encerramento

Para encerrar uma pendência, registrar decisão, responsável, referência e parâmetros com unidades; atualizar arquitetura/pinagem e dependências sem confundir definição com implementação. Para concluir uma entrega, vincular versão do firmware, procedimento, entradas, resultados e limitações às evidências dos requisitos. Itens compostos permanecem abertos enquanto qualquer parte não estiver comprovada.

Não há diretório `.agents/tasks/` neste módulo no checkout inspecionado. As marcações desta revisão estão neste checklist; não há arquivos de tarefas a mover de `active/` para `completed/`.
