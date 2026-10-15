# Requisitos e referências do OBC-DH

Este documento preserva os requisitos e critérios transcritos dos PDFs e os cenários de validação já existentes. A arquitetura vigente está em [ARCHITECTURE.md](ARCHITECTURE.md); execução, pendências e evidências estão em [TODO.md](TODO.md).

IDs, descrições, prioridades, referências CubeDesign, entradas, resultados esperados e associações dos testes foram mantidos. As etapas numéricas do catálogo abaixo pertencem à organização anterior de dez etapas e são preservadas como rastreabilidade histórica. A correspondência vigente aparece em seção própria.

## Catálogo original e rastreabilidade histórica

As prioridades e referências CubeDesign foram preservadas. “Não informado” indica campo vazio no PDF, não ausência de obrigação.

### Requisitos funcionais

| ID | Descrição | Prioridade | ID CubeDesign | Etapas principais |
|---|---|---|---|---|
| REQ-OBC-001 | Operar autonomamente durante a missão, com interação por TC e geração de TM sem intervenção manual. | Alta | HLR-GEN-03 | 7, 8, 9 e 10. |
| REQ-OBC-002 | Receber TC e executar suas ações no tempo de resposta definido pela equipe. | Alta | HLR-COMM-01 | 7, 8 e 10. |
| REQ-OBC-003 | Gerar e compilar TM estruturada com status, energia, atitude e dados da missão. | Alta | HLR-COMM-02 | 4, 6, 7 e 10. |
| REQ-OBC-004 | Implementar os modos de voo previstos, consistentes com o ConOps. | Alta | HLR-SW-01 | 1, 8 e 10. |
| REQ-OBC-005 | Armazenar dados de missão e TM de forma estruturada, documentada e reprodutível para análise. | Alta | HLR-SW-02 | 4, 5 e 10. |
| REQ-OBC-006 | Associar timestamp com resolução suficiente a cada mensagem ADS-B recebida e processada. | Alta | HLR-ADS-07 | 4, 6, 7 e 10. |
| REQ-OBC-007 | Detectar falhas e realizar reinicialização automática dentro do tempo determinado. | Alta | Não informado | 2, 9 e 10. |
| REQ-OBC-008 | Inicializar automaticamente após a energização do CubeSat. | Alta | Não informado | 2 e 10. |
| REQ-OBC-009 | Registrar falhas críticas e problemas operacionais. | Média | Não informado | 4, 5, 9 e 10. |
| REQ-OBC-010 | Registrar o horário dos eventos operacionais. | Média | Não informado | 4, 5, 9 e 10. |
| REQ-OBC-011 | Identificar perda de comunicação com subsistemas. | Média | Não informado | 6, 9 e 10. |
| REQ-OBC-012 | Priorizar tarefas críticas em disponibilidade energética reduzida. | Média | Não informado | 1, 8 e 10. |

### Requisitos não funcionais

| ID | Descrição | Prioridade | ID CubeDesign | Etapas principais |
|---|---|---|---|---|
| REQ-NF-OBC-001 | Otimizar o software para respeitar a cota energética definida pelo EPS para processamento de dados. | Alta | HLR-EPS-03 | 1, 2, 8 e 10. |
| REQ-NF-OBC-002 | Armazenar estatísticas relevantes da missão para análise posterior. | Média | Não informado | 4, 5 e 10. |
| REQ-NF-OBC-003 | Registrar a quantidade de reinicializações do sistema. | Média | Não informado | 5, 9 e 10. |
| REQ-NF-OBC-004 | Garantir integridade dos dados armazenados na operação nominal e em reinicializações inesperadas. | Alta | Não informado | 5, 9 e 10. |

### Relações arquiteturais explicitadas no PDF

| Decisão | Requisitos associados na arquitetura |
|---|---|
| ESP32 central para coordenação e execução de TC | REQ-OBC-001, REQ-OBC-002, REQ-OBC-004 e REQ-OBC-008. |
| SD para dados de missão e telemetria | REQ-OBC-005, REQ-NF-OBC-002 e REQ-NF-OBC-004. |

As etapas indicadas no catálogo ampliam essa rastreabilidade como proposta de implementação; não alteram os IDs originais.

## Critérios de validação fornecidos

Os critérios e associações abaixo permanecem intactos. Limites ainda não fornecidos constam de [P-VALIDACAO](TODO.md#p-validacao).

| ID associado no PDF | Método | Aplicação indicada |
|---|---|---|
| REQ-OBC-001 | Teste | Injetar TC pelo barramento e medir o tempo até a execução da ação. |
| REQ-OBC-003 | Inspeção | Conferir estrutura, integridade e ordenação dos campos de status, energia e atitude na TM. |
| REQ-OBC-006 | Demonstração | Injetar ADS-B simulado e demonstrar a inclusão correta e legível do timestamp na TM resultante. |

A associação do teste de TC a **REQ-OBC-001** foi preservada. Esse teste também é pertinente a **REQ-OBC-002**, mas sozinho não demonstra a autonomia durante toda a missão exigida por REQ-OBC-001.

## Cenários de validação já previstos no checklist

As linhas abaixo foram transferidas do checklist anterior sem alteração. São cenários já planejados, não evidência de testes executados. A referência a excesso de dados UART é mantida, compatível com a decisão atual do transporte do Payload. Alterações de entradas ou resultados esperados exigem diagnóstico e discussão prévia com o responsável.

As menções às etapas 8/9 e ao catálogo em ARCHITECTURE nas linhas preservadas são referências da organização anterior. O catálogo agora está neste documento; para execução, usar a correspondência com as cinco entregas abaixo. Isso não muda o procedimento ou resultado esperado dos cenários.

- [ ] Verificar boot e reset sem acionamento indevido de motor ou Burn Wire.
- [ ] Verificar comunicação básica e independência de operação entre rádio e armazenamento.
- [ ] Verificar a resolução efetiva dos timestamps e o comportamento com relógio inválido conforme os critérios definidos.
- [ ] Implementar recuperação e verificar integridade após reinicializações e interrupções durante escrita. **REQ-NF-OBC-004**.
- [ ] Verificar extração e interpretação reproduzível dos registros; reutilizar ferramenta de análise existente quando houver.
- [ ] Verificar os parâmetros e níveis de comunicação usados para identificar subsistemas indisponíveis; integrar a supervisão na etapa 9. **REQ-OBC-011**.
- [ ] Exercitar mensagens incompletas ou inválidas, desconexões e rajadas de dados do Payload.
- [ ] Inspecionar estrutura, integridade e ordenação dos campos da TM. **REQ-OBC-003**.
- [ ] Medir a resposta dos comandos já integrados contra o limite definido; concluir os comandos de missão na etapa 8. **REQ-OBC-002**.
- [ ] Demonstrar timestamp correto e legível no fluxo ADS-B → TM. **REQ-OBC-006**.
- [ ] Medir e ajustar o consumo de processamento para respeitar a cota do EPS. **REQ-NF-OBC-001**.
- [ ] Verificar transições, prioridades energéticas e condições de atuação, incluindo recusa de comandos fora das condições permitidas.
- [ ] Verificar retomada no modo previsto pelo ConOps e estados acordados das saídas após reset, especialmente motor e Burn Wire.
- [ ] Exercitar travamento de tarefa, perda de comunicação, excesso de dados UART e interrupção de alimentação; conferir recuperação e efeitos nos registros. **REQ-OBC-007, REQ-OBC-011 e REQ-NF-OBC-004**.
- [ ] Demonstrar operação sem intervenção manual, com geração de TM e suporte a TC. **REQ-OBC-001**.
- [ ] Conferir cobertura de todos os 12 requisitos funcionais e dos 4 não funcionais do catálogo em [ARCHITECTURE.md](ARCHITECTURE.md).
- [ ] Executar o teste de injeção de TC e medição do tempo de execução associado a REQ-OBC-001 no PDF; registrar também sua relação com **REQ-OBC-002**.
- [ ] Complementar esse teste com evidência de autonomia, pois o tempo de resposta a um TC isolado não comprova toda a **REQ-OBC-001**.
- [ ] Registrar a inspeção de estrutura, integridade e ordenação dos campos da TM. **REQ-OBC-003**.
- [ ] Registrar a demonstração de inclusão correta do timestamp a partir de ADS-B simulado. **REQ-OBC-006**.
- [ ] Consolidar evidências de boot automático, modos, prioridades, consumo, perda de comunicação, recuperação, registros e integridade dos dados.

## Verificação complementar proposta

Para completar a cobertura, verificar inicialização automática, operação sem intervenção manual, transições de modo, priorização energética, registro de eventos e estatísticas, contagem de reinicializações, perda de comunicação e recuperação após falhas. Medir os tempos, a resolução temporal e o consumo contra os valores que a equipe definir.

Conferir os registros após reinicializações e interrupções durante escrita, além de exercitar mensagens incompletas ou inválidas, rajadas do Payload e indisponibilidade de periféricos. Registrar evidências junto aos requisitos atendidos.

O acabamento consiste em revisar clareza, consistência dos termos, links e exemplos reais de configuração e uso. Aplicar o padrão de C++ já existente e consolidar ferramentas redundantes identificadas, sem criar outro documento de convenções.

Para o planejamento vigente, acrescentar evidências de enquadramento UART (mensagens parciais, consecutivas e acima do limite), validade do horário antes da sincronização, referências de tempo em boots diferentes, comandos repetidos, rejeição de envio pelo TT&C e convivência entre FOC, I2C, rádio e SD. Esses acréscimos não substituem os critérios existentes. Os limites quantitativos dependem de P-DADOS, P-TEMPO, P-ADCS e P-VALIDACAO no checklist.

## Correspondência com as cinco entregas vigentes

| Entrega do TODO | Requisitos atendidos principalmente |
|---|---|
| 1. Plataforma e interfaces | REQ-OBC-008; preparação de REQ-OBC-007 e REQ-OBC-011. |
| 2. Recepção, modelos e tempo | REQ-OBC-005, REQ-OBC-006 e REQ-OBC-010. |
| 3. Registros persistentes | REQ-OBC-005, REQ-OBC-009, REQ-OBC-010, REQ-NF-OBC-002, REQ-NF-OBC-003 e REQ-NF-OBC-004. |
| 4. Integração e missão | REQ-OBC-001, REQ-OBC-002, REQ-OBC-003, REQ-OBC-004, REQ-OBC-007, REQ-OBC-011, REQ-OBC-012 e REQ-NF-OBC-001. |
| 5. Validação integrada | Comprovação dos 12 requisitos funcionais e quatro não funcionais. |

A correspondência organiza o trabalho; não altera requisitos nem permite adiar tratamento básico de falhas e validação até a última entrega.

## Referências históricas dos PDFs

As tabelas seguintes são referências de origem. Não aprovam componentes, consumo da montagem ou ligações diferentes da arquitetura vigente. Wi-Fi e Bluetooth são características citadas do ESP32, não interfaces de missão exigidas.

### Componentes

| Componente confirmado no PDF | Função | Justificativa de escolha |
|---|---|---|
| ESP32 DevKit V1 | Computador de bordo principal. | Baixo custo e consumo, ampla documentação e Wi-Fi/Bluetooth integrados. |
| Leitor de cartão SD | Armazenamento dos dados da missão. | Persistência e conservação local dos dados recebidos. |
| RTC DS3231 | Referência de tempo via I2C. | Contagem de tempo independente da internet para timestamps ADS-B. |

DS3231 permanece como referência histórica. O plano atual usa tempo monotônico do ESP32 e referências pelo LoRa; precisão e necessidade de hardware adicional dependem de P-TEMPO.

### Alimentação

| Componente | Alimentação | Idle / sleep | Nominal | Máximo | Fonte |
|---|---|---|---|---|---|
| ESP32 | 3,3 V | 0,01 mA | 45 mA | 240 mA | Rail de 3,3 V do EPS. |
| Leitor de SD | 3,3 V | 0,2 mA | 25 mA | 100 mA | Rail de 3,3 V do EPS. |
| RTC | 3,3 V | 0,11 mA | 0,2 mA | 1 mA | Rail de 3,3 V do EPS. |

Esses valores não representam a cota energética aprovada pelo EPS nem o consumo medido da placa completa. A presença de RTC e o orçamento real dependem das definições do checklist.

### Fluxos de origem

Nas linhas de leitura, a seta representa o acesso indicado pelo documento; a resposta do sensor fornece o dado ao OBC. Observações da tabela são as transcritas anteriormente; o contrato atual do Payload é UART e o ADCS executará no mesmo ESP32. Encoder, medição solar e montagem ainda dependem de confirmação.

| Origem → destino no PDF | Dado / operação | Interface | Observação de integração |
|---|---|---|---|
| ESP32 → ADCS | Leitura do GY-302. | I2C | Solicitação de leitura; informação retornada ao OBC. |
| ESP32 → ADCS | Leitura de tensão das placas solares. | ADC | Diverge do mapeamento sem ADC; definição pendente. |
| ADCS → ESP32 | Encoder do motor. | I2C | Não há alocação correspondente na pinagem. |
| ESP32 → TT&C | Transmissão de pacote de dados. | SPI | VSPI do LoRa, conforme pinagem. |
| Payload → ESP32 | JSON com ICAO, latitude, longitude, velocidade e timestamp. | I2C | Descrição de origem; formato vigente e taxa ainda não definidos. |
| Cartão SD → ESP32 | Verificação de dados. | SPI | Leitura pelo HSPI. |
| ESP32 → cartão SD | Armazenamento de dados. | SPI | Escrita pelo HSPI. |
| RTC → ESP32 | Referência para timestamp. | I2C | DS3231. |

### Riscos e alternativas

| Decisão | Alternativa considerada | Motivo da escolha | Risco | Mitigação indicada |
|---|---|---|---|---|
| ESP32 como OBC central | Raspberry Pi como computador único. | Menor consumo e simplicidade operacional. | Ponto único de falha; travamento ou falha física interrompe o sistema. | Watchdog para travamentos; armazenamento no OBC preserva os dados já recebidos se o Payload Linux falhar. |
| Cartão SD | Operação sem armazenamento. | Conservação local e redundância de coleta dos dados. | Brownout durante escrita pode corromper arquivos ou FAT32. | Fechamento de arquivos e monitoramento da alimentação. |
| UART entre Payload e OBC | SPI, CAN ou USB. | Simplicidade, baixo custo e compatibilidade com ESP32. | Excesso de JSON pode causar estouro de buffers, perdas e inconsistências. | UART em alta velocidade, buffers e filas FreeRTOS para absorver variações de taxa. |

O watchdog não recupera falha física do ESP32. O SD conserva dados já gravados, mas não existe redundância física confirmada de OBC ou armazenamento. Fechar arquivos normalmente não comprova integridade após queda durante escrita.

### Histórico de decisões de interface

Em 06/10/2026, foi registrada a intenção de I2C bidirecional para o Payload, com papéis, arbitragem e barramento ainda abertos. Essa decisão foi substituída por UART pelo responsável na revisão atual. A antiga proposta `PAYLOAD_ADDR = 0x80` não era endereço I2C de 7 bits válido: como byte de escrita, corresponderia a `0x40` (leitura `0x81`), já proposto para INA219 #1. Como o Payload usará UART, não se aplica alocar ou converter esse endereço.

## Fontes e controle de origem

| Material | Identificação no arquivo | Uso nesta consolidação |
|---|---|---|
| Definição de Arquitetura OBC.pdf | Versão 1.5, de 10/07/2026. | Estratégia, componentes, fluxos, energia, rastreabilidade e riscos. |
| Especificação de Requisitos OBC.pdf | Capa: v1.5, de 25/05/2026; histórico: revisão 1.6, de 29/05/2026. | Objetivo, escopo, requisitos, restrições, interfaces e verificação. |
| Alocação de Pinos.pdf | Sem versão ou data explícita. | Interfaces físicas e identificação de divergências. |

Consultar também ConOps, ICDs, regulamento, arquitetura lógica do CEFAST Aerospace e o padrão existente de C++. Esses materiais foram citados no contexto do projeto, mas seus conteúdos não foram fornecidos para esta consolidação.
