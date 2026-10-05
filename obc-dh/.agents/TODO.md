# Plano de implementação do OBC&DH

Checklist do CubeSat ADS-B CubeDesign 2026, organizado na mesma sequência de [ARCHITECTURE.md](ARCHITECTURE.md). Cada etapa prepara a infraestrutura usada pelas seguintes.

Pendências de contrato devem ser resolvidas antes das tarefas que dependem delas. Estados das saídas, validação de entradas e tratamento básico de erros devem acompanhar o desenvolvimento desde o início.

---

## 1. Fundamentos e contratos

**Base necessária:** materiais de referência e alinhamento com os subsistemas.  
**Resultado:** escopo e contratos suficientes para iniciar a implementação.

- [ ] Ler a arquitetura, os requisitos, a pinagem, o ConOps, os ICDs, o regulamento, a arquitetura lógica do CEFAST Aerospace e o padrão existente de C++.
- [ ] Confirmar a revisão vigente da especificação de requisitos: capa v1.5 versus histórico v1.6.
- [ ] Inventariar funções, classes, drivers, utilitários, scripts e dependências existentes antes de propor novas ferramentas.
- [ ] Confirmar as responsabilidades de OBC, Payload, TT&C, EPS, ADCS e sistema térmico.
- [ ] Conciliar a responsabilidade física do ADCS com PWM, EN e SimpleFOC previstos na pinagem.
- [ ] Resolver a divergência entre ADC das placas solares e o mapeamento que evita ADC.
- [ ] Definir a interface e a pinagem do encoder do ADCS.
- [ ] Confirmar a integração do DS3231 ao I2C compartilhado.
- [ ] Definir a interface e o comportamento do Thermal Watchdog.
- [ ] Definir as condições de liberação, temporização e comportamento em reset do Burn Wire com Estruturas / EPS.
- [ ] Confirmar a lista de modos, transições e prioridades no ConOps. **REQ-OBC-004**.
- [ ] Definir os ICDs de dados e comandos, incluindo campos, unidades, limites, enquadramento e validação.
- [ ] Definir velocidade e volume de dados da UART do Payload, além da capacidade necessária para rajadas.
- [ ] Definir os limites de resposta a TC, detecção e recuperação de falhas e perda de comunicação. **REQ-OBC-002, REQ-OBC-007 e REQ-OBC-011**.
- [ ] Definir referência, significado e resolução dos timestamps do OBC e do Payload. **REQ-OBC-006 e REQ-OBC-010**.
- [ ] Obter a cota energética, os limiares e as prioridades de operação com EPS. **REQ-OBC-012 e REQ-NF-OBC-001**.
- [ ] Definir formato e política dos registros, estatísticas relevantes e persistência da contagem de reinicializações. **REQ-OBC-005 e REQ-NF-OBC-002 a REQ-NF-OBC-004**.

---

## 2. Plataforma e inicialização

**Base necessária:** etapa 1 e hardware definido.  
**Resultado:** ESP32 inicia automaticamente e mantém as saídas nos estados acordados.

- [ ] Configurar o ambiente C++ e as dependências conforme a arquitetura existente, reutilizando a infraestrutura disponível.
- [ ] Confirmar a variante do DevKit, o módulo ESP32, a disponibilidade dos GPIOs e o circuito da interface USB-C / UART.
- [ ] Conferir alimentação de 3,3 V, níveis lógicos, GND e interfaces elétricas com EPS e demais áreas.
- [ ] Conferir [PINAGEM.md](PINAGEM.md) com a montagem e documentar eventuais alterações acordadas.
- [ ] Validar os níveis de GPIO 5, GPIO 12 e GPIO 15 durante boot e reset com os periféricos conectados.
- [ ] Definir e aplicar os estados iniciais de CS, RST do rádio, EN, PWM e Burn Wire conforme os contratos.
- [ ] Implementar a inicialização automática após energização, com identificação de falhas nos recursos básicos. **REQ-OBC-008**.
- [ ] Verificar boot e reset sem acionamento indevido de motor ou Burn Wire.

---

## 3. Barramentos e execução concorrente

**Base necessária:** etapa 2 e parâmetros das interfaces definidos.  
**Resultado:** comunicação básica e troca controlada de mensagens entre tarefas.

- [ ] Configurar UART0 para a interface umbilical de bancada.
- [ ] Configurar UART2 para comunicação com o Payload.
- [ ] Configurar I2C em SDA = 21 / SCL = 22 e conferir dispositivos, endereços e parâmetros elétricos acordados.
- [ ] Configurar VSPI para o LoRa SX1276 e HSPI para o cartão SD, com os sinais definidos na pinagem.
- [ ] Configurar DIO0 e RST do rádio conforme o driver adotado.
- [ ] Integrar os drivers necessários, verificando primeiro as implementações e bibliotecas já disponíveis.
- [ ] Definir tarefas FreeRTOS, prioridades, buffers, filas e responsáveis pelo acesso a cada recurso.
- [ ] Implementar a troca de mensagens e o tratamento de filas cheias, timeouts, entradas inválidas e indisponibilidade de periféricos.
- [ ] Verificar comunicação básica e independência de operação entre rádio e armazenamento.

---

## 4. Tempo e modelo de dados

**Base necessária:** etapa 3 e contratos de dados definidos na etapa 1.  
**Resultado:** registros estruturados com referência de tempo documentada.

- [ ] Implementar inicialização, leitura, ajuste e identificação de validade do RTC DS3231 conforme o contrato.
- [ ] Definir estruturas comuns para ADS-B, estado do sistema, energia, atitude, eventos, falhas e estatísticas.
- [ ] Documentar campos, unidades e representação temporal. **REQ-OBC-005**.
- [ ] Implementar a associação de timestamp a cada mensagem ADS-B recebida e processada. **REQ-OBC-006**.
- [ ] Tratar o timestamp recebido do Payload conforme o significado acordado, distinguindo-o do tempo associado pelo OBC quando necessário.
- [ ] Implementar o registro do horário dos eventos operacionais. **REQ-OBC-010**.
- [ ] Verificar a resolução efetiva dos timestamps e o comportamento com relógio inválido conforme os critérios definidos.

---

## 5. Armazenamento e registros

**Base necessária:** etapas 3 e 4.  
**Resultado:** dados gravados e recuperáveis para análise posterior.

- [ ] Implementar acesso ao SD pelo HSPI e o sistema de arquivos adotado, considerando FAT32 citado na arquitetura.
- [ ] Implementar gravação e leitura de dados da missão e telemetria no formato documentado. **REQ-OBC-005**.
- [ ] Registrar falhas críticas, problemas operacionais e eventos com horário. **REQ-OBC-009 e REQ-OBC-010**.
- [ ] Registrar as estatísticas relevantes acordadas. **REQ-NF-OBC-002**.
- [ ] Implementar o registro da quantidade de reinicializações com a política de persistência definida. **REQ-NF-OBC-003**.
- [ ] Implementar a política de escrita, sincronização, fechamento e tratamento de SD indisponível.
- [ ] Implementar recuperação e verificar integridade após reinicializações e interrupções durante escrita. **REQ-NF-OBC-004**.
- [ ] Verificar extração e interpretação reproduzível dos registros; reutilizar ferramenta de análise existente quando houver.

---

## 6. Integração dos subsistemas

**Base necessária:** etapas 3 a 5 e ICDs definidos.  
**Resultado:** dados validados da missão e dos subsistemas disponíveis ao OBC.

- [ ] Implementar recepção, delimitação e validação do JSON ADS-B recebido pela UART do Payload.
- [ ] Integrar os dados ADS-B ao modelo comum, ao timestamp e ao armazenamento. **REQ-OBC-005 e REQ-OBC-006**.
- [ ] Implementar solicitações de status e temperatura à Raspberry Pi conforme o ICD.
- [ ] Integrar recepção de TC e envio de dados ao TT&C pelo LoRa.
- [ ] Integrar monitoramento de tensão, corrente e temperatura do EPS conforme as interfaces acordadas.
- [ ] Integrar os dados de atitude e sensores do ADCS; implementar encoder e medição solar somente após resolver suas interfaces.
- [ ] Integrar o LM75A e os dados térmicos previstos no contrato.
- [ ] Verificar os parâmetros e níveis de comunicação usados para identificar subsistemas indisponíveis; integrar a supervisão na etapa 9. **REQ-OBC-011**.
- [ ] Exercitar mensagens incompletas ou inválidas, desconexões e rajadas de dados do Payload.

---

## 7. Telemetria e telecomandos

**Base necessária:** etapas 4 a 6.  
**Resultado:** TM estruturada e TC validado e encaminhado.

- [ ] Compilar TM com status, energia, atitude e dados da missão. **REQ-OBC-003**.
- [ ] Formatar a TM conforme o ICD e encaminhá-la ao TT&C para transmissão RF.
- [ ] Disponibilizar TC e TM básicos pela interface umbilical conforme o contrato de bancada.
- [ ] Implementar o catálogo e a validação dos TC, incluindo parâmetros e condições de execução.
- [ ] Encaminhar comandos válidos às ações e fornecer confirmação ou rejeição conforme o ICD. **REQ-OBC-002**.
- [ ] Inspecionar estrutura, integridade e ordenação dos campos da TM. **REQ-OBC-003**.
- [ ] Medir a resposta dos comandos já integrados contra o limite definido; concluir os comandos de missão na etapa 8. **REQ-OBC-002**.
- [ ] Demonstrar timestamp correto e legível no fluxo ADS-B → TM. **REQ-OBC-006**.

---

## 8. Modos de missão, energia e atuação

**Base necessária:** etapas 6 e 7, ConOps e contratos de atuação.  
**Resultado:** modos, prioridades e ações de missão integrados.

- [ ] Implementar os modos e as transições definidos no ConOps. **REQ-OBC-004**.
- [ ] Condicionar a execução dos TC ao modo atual e às condições acordadas para cada ação. **REQ-OBC-002 e REQ-OBC-004**.
- [ ] Implementar a priorização das tarefas críticas em disponibilidade energética reduzida. **REQ-OBC-012**.
- [ ] Aplicar as políticas de redução de consumo acordadas com EPS.
- [ ] Medir e ajustar o consumo de processamento para respeitar a cota do EPS. **REQ-NF-OBC-001**.
- [ ] Integrar solicitação de detumbling e controle do motor apenas na divisão de responsabilidades acordada com ADCS.
- [ ] Integrar o Thermal Watchdog segundo seu contrato térmico.
- [ ] Integrar o Burn Wire com autorização, temporização, estados em reset e comportamento após reinicialização definidos.
- [ ] Verificar transições, prioridades energéticas e condições de atuação, incluindo recusa de comandos fora das condições permitidas.

---

## 9. Autonomia e recuperação de falhas

**Base necessária:** etapas 2 a 8 integradas.  
**Resultado:** supervisão e recuperação demonstradas no sistema completo.

- [ ] Integrar a detecção de perda de comunicação com os subsistemas. **REQ-OBC-011**.
- [ ] Implementar a supervisão das tarefas críticas e configurar o watchdog com os limites definidos.
- [ ] Implementar detecção de falhas e reinicialização automática no tempo acordado. **REQ-OBC-007**.
- [ ] Integrar recuperação física do rádio por RST quando prevista pela política de falhas.
- [ ] Registrar falhas, horários dos eventos e quantidade de reinicializações durante a recuperação. **REQ-OBC-009, REQ-OBC-010 e REQ-NF-OBC-003**.
- [ ] Verificar retomada no modo previsto pelo ConOps e estados acordados das saídas após reset, especialmente motor e Burn Wire.
- [ ] Exercitar travamento de tarefa, perda de comunicação, excesso de dados UART e interrupção de alimentação; conferir recuperação e efeitos nos registros. **REQ-OBC-007, REQ-OBC-011 e REQ-NF-OBC-004**.
- [ ] Demonstrar operação sem intervenção manual, com geração de TM e suporte a TC. **REQ-OBC-001**.

---

## 10. Validação e acabamento

**Base necessária:** etapas anteriores e critérios quantitativos definidos.  
**Resultado:** evidências de atendimento e documentação coerente com o software.

- [ ] Conferir cobertura de todos os 12 requisitos funcionais e dos 4 não funcionais do catálogo em [ARCHITECTURE.md](ARCHITECTURE.md).
- [ ] Executar o teste de injeção de TC e medição do tempo de execução associado a REQ-OBC-001 no PDF; registrar também sua relação com **REQ-OBC-002**.
- [ ] Complementar esse teste com evidência de autonomia, pois o tempo de resposta a um TC isolado não comprova toda a **REQ-OBC-001**.
- [ ] Registrar a inspeção de estrutura, integridade e ordenação dos campos da TM. **REQ-OBC-003**.
- [ ] Registrar a demonstração de inclusão correta do timestamp a partir de ADS-B simulado. **REQ-OBC-006**.
- [ ] Consolidar evidências de boot automático, modos, prioridades, consumo, perda de comunicação, recuperação, registros e integridade dos dados.
- [ ] Conferir conformidade com ICDs, ConOps, regulamento e arquitetura lógica do CEFAST Aerospace.
- [ ] Atualizar pinagem, arquitetura e README com decisões finais, parâmetros reais, dependências e instruções de compilação e uso.
- [ ] Aplicar o padrão existente de escrita e documentação em C++, sem criar um documento redundante.
- [ ] Revisar termos, tabelas, links e exemplos; consolidar ferramentas redundantes identificadas sem remover funcionalidades necessárias.
- [ ] Atualizar este checklist de acordo com as evidências e manter pendências restantes identificadas.

---

## Referências

- [ARCHITECTURE.md](ARCHITECTURE.md) — definição das etapas, requisitos e critérios de verificação.
- [PINAGEM.md](PINAGEM.md) — interfaces físicas e restrições de integração.
- [README.md](README.md) — visão geral e regras de trabalho.
- **Definição de Arquitetura OBC.pdf**, **Especificação de Requisitos OBC.pdf** e **Alocação de Pinos.pdf** — materiais de origem.
