# Arquitetura e requisitos do OBC&DH

Este documento reúne a definição de arquitetura e a especificação de requisitos fornecidas, organizadas dos fundamentos até a integração e o acabamento. A sequência corresponde às etapas de [TODO.md](TODO.md).

As decisões de hardware, interfaces e requisitos identificados por ID vêm dos PDFs. A divisão em etapas, as entregas sugeridas e os critérios adicionais de implementação são uma proposta de organização. Parâmetros ausentes e divergências permanecem como **pendências**, sem valores presumidos.

---

## Sequência de implementação

| Etapa | Resultado esperado | Base necessária |
|---|---|---|
| 1. Fundamentos e contratos | Escopo, referências e interfaces compreendidos; divergências tratadas. | Materiais de referência e alinhamento entre áreas. |
| 2. Plataforma e inicialização | ESP32 inicia sozinho e mantém as saídas nos estados acordados. | Etapa 1 e hardware definido. |
| 3. Barramentos e execução concorrente | Comunicação básica e troca de mensagens entre tarefas. | Etapa 2. |
| 4. Tempo e modelo de dados | Registros estruturados com referência de tempo. | Etapa 3 e contratos de dados da etapa 1. |
| 5. Armazenamento e registros | Dados podem ser gravados, recuperados e analisados. | Etapas 3 e 4. |
| 6. Integração dos subsistemas | Dados de missão e estado operacional disponíveis ao OBC. | Etapas 3 a 5 e ICDs. |
| 7. Telemetria e telecomandos | TM estruturada e TC validado e encaminhado. | Etapas 4 a 6. |
| 8. Modos de missão, energia e atuação | Comportamento de voo e prioridades integrados. | Etapas 6 e 7, ConOps e acordos de atuação. |
| 9. Autonomia e recuperação de falhas | Supervisão e recuperação verificadas no sistema integrado. | Etapas 2 a 8. |
| 10. Validação e acabamento | Evidências de atendimento e documentação pronta para uso. | Etapas anteriores e critérios quantitativos definidos. |

Essa ordem guia a integração. Estados seguros, validação de entradas e tratamento básico de erros devem acompanhar cada etapa desde sua implementação; não devem ser adiados para a etapa 9.

---

## 1. Fundamentos e contratos

### Objetivo e escopo

O OBC&DH é o núcleo de processamento do CubeSat. O segmento **OBC** executa o software de voo, processa os dados e organiza informações críticas. O segmento **DH** coordena o fluxo lógico de comandos e telemetria entre OBC, Payload, TT&C, EPS e ADCS.

O escopo inclui execução do software principal, coordenação e priorização dos subsistemas, modos operacionais previstos no ConOps, processamento de telecomandos, organização e armazenamento dos dados de missão e telemetria e inclusão de timestamps nos dados recebidos.

| Responsabilidade externa | Subsistema responsável |
|---|---|
| Recepção RF em 1090 MHz, decodificação e extração ADS-B | Payload. |
| Transmissão RF de TM e recepção RF de TC do solo | TT&C. |
| Determinação e controle físico de atitude, incluindo detumbling | ADCS. |
| Gerenciamento elétrico físico, baterias e distribuição de tensão | EPS. |

O OBC recebe dados já decodificados, compila informações e coordena ações lógicas. A alocação de PWM e EN do motor no ESP32 precisa ser conciliada com essa divisão de responsabilidades.

### Restrições obrigatórias

- Respeitar a arquitetura lógica padronizada do **CEFAST Aerospace**.
- Utilizar as interfaces definidas nos **ICDs** das demais áreas.
- Utilizar alimentação compatível com o **EPS**.
- Respeitar o **regulamento da competição**.

Antes de modificar a implementação, consultar essas referências, a pinagem, os requisitos, o ConOps e o padrão existente de C++. Antes de criar ferramentas de código, pesquisar soluções equivalentes já presentes no projeto ou nas dependências adotadas.

### Definições ainda necessárias

| Pendência | Definição a obter | Etapas afetadas |
|---|---|---|
| Revisão dos requisitos | A capa indica v1.5, de 25/05/2026; o histórico registra v1.6, de 29/05/2026. Confirmar a revisão vigente. | Todas. |
| ConOps | Modos, transições, condições de entrada e saída e prioridades da missão. | 7 a 10. |
| Arquitetura lógica e ambiente C++ | Organização adotada pelo CEFAST, ferramentas, dependências e configuração de compilação. | 2 a 10. |
| ICDs e pacotes | Campos obrigatórios, unidades, enquadramento das mensagens, limites de tamanho, comandos, respostas e critérios de validação. | 3 a 10. |
| UART e aquisição | Velocidade, volume de mensagens e capacidade de absorver rajadas do Payload. | 3, 6 e 9. |
| Critérios temporais | Tempo máximo de resposta a TC, detecção e recuperação de falhas, detecção de perda de comunicação e resolução dos timestamps. | 4 e 7 a 10. |
| Tempo do Payload | A arquitetura cita timestamp no JSON; os requisitos atribuem ao OBC a associação do timestamp. Definir o significado de cada tempo e a referência usada. | 4, 6 e 7. |
| Energia | Cota do OBC, limiares, estados de baixo consumo e prioridades acordadas com EPS. | 2 e 8 a 10. |
| ADC das placas solares | A arquitetura prevê ADC; a pinagem propõe evitá-lo. Definir a interface real de medição e sua alocação. | 2, 3 e 6. |
| Encoder do ADCS | A arquitetura cita SPI, mas a pinagem não reserva sinais para o encoder. Definir interface e eventual compartilhamento. | 2, 3, 6 e 8. |
| Responsabilidade do motor | Conciliar o escopo lógico do OBC com PWM, EN e SimpleFOC previstos na pinagem. | 2, 6 e 8. |
| RTC | Confirmar ligação do DS3231 ao I2C compartilhado, endereço, inicialização, ajuste e validade do relógio. | 3 e 4. |
| Thermal Watchdog | Definir a função térmica citada na arquitetura, sua interface, condições de acionamento e recuperação. | 2, 6, 8 e 9. |
| Burn Wire | Definir condições de autorização, duração, nível ativo, estado em reset e comportamento após reinicialização. | 2, 7 a 10. |
| Placa e elétrica | Confirmar variante do DevKit, módulo, USB-C, circuito USB/UART, níveis lógicos, resistores e estados dos strapping pins. | 2 e 3. |
| Registros e estatísticas | Definir formato persistente, estatísticas relevantes, política de gravação, retenção e contagem de reinicializações. | 4, 5 e 9. |

**Entrega sugerida:** contratos mínimos registrados e cada pendência impeditiva resolvida antes da implementação que depende dela.

---

## 2. Plataforma e inicialização

### Arquitetura centralizada

O **ESP32** concentra a coordenação lógica dos subsistemas, coleta e processamento dos dados, execução de telecomandos, geração de telemetria e monitoramento operacional. O software roda sobre **FreeRTOS**, com filas para isolar a troca de mensagens entre tarefas.

O armazenamento local ocorre em cartão SD. O RTC fornece a referência de tempo sem depender de internet. A arquitetura também atribui ao OBC o acionamento do Thermal Watchdog e o gerenciamento da interface umbilical USB-C.

| Componente confirmado no PDF | Função | Justificativa de escolha |
|---|---|---|
| ESP32 DevKit V1 | Computador de bordo principal. | Baixo custo e consumo, ampla documentação e Wi-Fi/Bluetooth integrados. |
| Leitor de cartão SD | Armazenamento dos dados da missão. | Persistência e conservação local dos dados recebidos. |
| RTC DS3231 | Referência de tempo via I2C. | Contagem de tempo independente da internet para timestamps ADS-B. |

> Wi-Fi e Bluetooth são características citadas do componente, não interfaces de missão exigidas pelos documentos.

### Alimentação

Valores informados na tabela de energia da arquitetura:

| Componente | Alimentação | Idle / sleep | Nominal | Máximo | Fonte |
|---|---|---|---|---|---|
| ESP32 | 3,3 V | 0,01 mA | 45 mA | 240 mA | Rail de 3,3 V do EPS. |
| Leitor de SD | 3,3 V | 0,2 mA | 25 mA | 100 mA | Rail de 3,3 V do EPS. |
| RTC | 3,3 V | 0,11 mA | 0,2 mA | 1 mA | Rail de 3,3 V do EPS. |

> Esses valores são referências do PDF. Não representam, por si só, a cota energética aprovada pelo EPS nem o consumo medido da placa completa com todos os periféricos.

### Inicialização

O sistema deve iniciar automaticamente após a energização (**REQ-OBC-008**). Como organização de implementação, estabelecer primeiro os estados acordados das saídas, inicializar os recursos básicos, identificar falhas de inicialização e iniciar as tarefas no modo previsto no ConOps.

A pinagem está em [PINAGEM.md](PINAGEM.md). A inicialização deve considerar EN do motor, PWM, Burn Wire, CS e reset do rádio, inclusive durante resets inesperados. Os níveis ativos e a sequência definitiva dependem dos ICDs e do hardware.

**Entrega sugerida:** firmware mínimo que inicia sem intervenção manual e disponibiliza um estado inicial observável pela interface de bancada.

---

## 3. Barramentos e execução concorrente

### Interfaces físicas

| Interface | Uso previsto |
|---|---|
| UART0 / conexão USB da placa | Interface umbilical de bancada e solo para TC e TM básicos. |
| UART2 | JSON ADS-B do Payload e solicitações do OBC à Raspberry Pi. |
| I2C compartilhado | Sensores térmicos, EPS, ADCS e integração do RTC a confirmar. |
| VSPI | Rádio LoRa SX1276 do TT&C. |
| HSPI | Leitura e gravação do cartão SD. |
| GPIO / PWM | Reset e eventos do rádio, habilitação e fases do motor e gatilho Burn Wire, conforme acordos de responsabilidade. |

VSPI e HSPI separam rádio e armazenamento. Cada driver e recurso compartilhado ainda precisa de uma política de acesso.

### Organização da execução

As filas FreeRTOS devem permitir que aquisição, processamento, armazenamento e comunicação troquem dados sem misturar suas responsabilidades. Como orientação de implementação, definir limites e comportamento para filas cheias, mensagens inválidas, timeouts e periféricos indisponíveis.

O DIO0 do SX1276 sinaliza eventos de recepção ou transmissão. Seu tratamento deve ser integrado ao fluxo de tarefas conforme a biblioteca e o driver adotados. O reset físico do rádio está disponível para recuperação.

**Entrega sugerida:** barramentos inicializados, troca básica de mensagens entre tarefas e configuração documentada das interfaces.

---

## 4. Tempo e modelo de dados

Cada mensagem ADS-B recebida e processada deve ter um timestamp com resolução suficiente (**REQ-OBC-006**). Eventos operacionais também devem registrar horário (**REQ-OBC-010**). O DS3231 fornece a referência de tempo.

Os dados de missão e telemetria devem ter estrutura documentada e permitir reprodução da análise posterior (**REQ-OBC-005**). Definir os campos, suas unidades e a representação temporal antes de integrar armazenamento e transmissão.

| Grupo de dados | Conteúdo previsto |
|---|---|
| ADS-B | ICAO, posição / latitude e longitude, altitude e velocidade, conforme os documentos e o ICD do Payload. |
| Estado do sistema | Condição operacional do OBC e dos subsistemas. |
| Energia | Tensão, corrente e temperatura do EPS. |
| Atitude | Informações lógicas e vetores fornecidos pelo ADCS. |
| Eventos e falhas | Ocorrência e horário de eventos operacionais, falhas críticas e problemas. |
| Estatísticas | Informações relevantes da missão e quantidade de reinicializações. |

> A lista é uma base de conteúdo, não um esquema definitivo de pacote. O JSON citado para a UART do Payload não determina automaticamente o formato dos arquivos no SD ou da TM.

**Entrega sugerida:** modelo comum de registros e política de tempo, incluindo a distinção entre timestamp recebido do Payload e timestamp associado pelo OBC, quando ambos existirem.

---

## 5. Armazenamento e registros

O cartão SD armazena dados da missão e telemetria (**REQ-OBC-005**), estatísticas (**REQ-NF-OBC-002**), falhas e problemas operacionais (**REQ-OBC-009**) e horários dos eventos (**REQ-OBC-010**). O sistema também deve registrar a quantidade de reinicializações (**REQ-NF-OBC-003**).

A integridade deve ser garantida durante a operação nominal e reinicializações inesperadas (**REQ-NF-OBC-004**). O documento de arquitetura considera FAT32 e identifica corrupção por queda de tensão durante a escrita. Sua mitigação proposta é utilizar procedimentos de fechamento de arquivos e monitoramento da alimentação.

Como trabalho de implementação, definir a política de escrita, sincronização, fechamento, recuperação de registros e tratamento de indisponibilidade do SD. 

> A eficácia deve ser verificada com interrupções durante gravação; o simples fechamento de arquivos na operação normal não demonstra atendimento ao requisito em uma queda inesperada.

**Entrega sugerida:** registros legíveis e documentados, recuperação após reinicialização e extração reproduzível dos dados para análise.

---

## 6. Integração dos subsistemas

### Contratos lógicos esperados

| Subsistema | Dados recebidos / monitorados pelo OBC | Ações do OBC |
|---|---|---|
| Payload | ADS-B já decodificado: ICAO, posição, altitude e velocidade; status e temperatura conforme ICD. | Estruturar, associar tempo, armazenar e preparar os dados para TM; enviar solicitações pela UART. |
| TT&C | Telecomandos recebidos do solo. | Entregar TM formatada e compilada; validar e encaminhar os TC para execução. |
| EPS | Tensão, corrente e temperatura. | Adequar modos e atividades à autonomia disponível. |
| ADCS | Informações e vetores de atitude. | Compor TM e enviar a solicitação lógica da manobra prevista no ICD e ConOps. |
| Térmico | Temperatura pelo LM75A indicado na pinagem. | Monitorar e integrar a função Thermal Watchdog quando seu contrato estiver definido. |

> O texto dos requisitos usa “tumbling” na interface ADCS, enquanto o escopo e a pinagem mencionam “detumbling”. A nomenclatura e a ação efetiva devem ser confirmadas com ADCS.

### Fluxos da definição de arquitetura

A tabela abaixo preserva os pares de origem e destino indicados no PDF. Nas linhas de leitura, a seta representa o acesso indicado pelo documento; a resposta do sensor fornece o dado ao OBC.

| Origem → destino no PDF | Dado / operação | Interface | Observação de integração |
|---|---|---|---|
| ESP32 → ADCS | Leitura do GY-302. | I2C | Solicitação de leitura; informação retornada ao OBC. |
| ESP32 → ADCS | Leitura de tensão das placas solares. | ADC | Diverge do mapeamento sem ADC; definição pendente. |
| ADCS → ESP32 | Encoder do motor. | SPI | Não há alocação correspondente na pinagem. |
| ESP32 → TT&C | Transmissão de pacote de dados. | SPI | VSPI do LoRa, conforme pinagem. |
| Payload → ESP32 | JSON com ICAO, latitude, longitude, velocidade e timestamp. | UART serial | Transmissão em alta velocidade, sem valor definido; conciliar timestamp com REQ-OBC-006. |
| Cartão SD → ESP32 | Verificação de dados. | SPI | Leitura pelo HSPI. |
| ESP32 → cartão SD | Armazenamento de dados. | SPI | Escrita pelo HSPI. |
| RTC → ESP32 | Referência para timestamp. | I2C | DS3231. |

Os requisitos complementam esse fluxo com TC do TT&C ao OBC e monitoramento do EPS. A pinagem acrescenta MPU-6050, INA219, BMS, sinais do motor, Burn Wire e a interface umbilical. Esses elementos devem ser integrados respeitando os contratos entre áreas.

**Entrega sugerida:** entradas validadas e dados de cada subsistema disponíveis no modelo comum, com limites de comunicação definidos.

---

## 7. Telemetria e telecomandos

### Telemetria

O OBC deve gerar e compilar pacotes estruturados com status do sistema, energia, atitude e dados da missão (**REQ-OBC-003**). O TT&C recebe esses pacotes já formatados para transmissão RF. A interface umbilical também é prevista para TC e TM básicos em bancada.

O formato, a ordenação dos campos, o tamanho máximo e a frequência de envio dependem dos ICDs e dos limites do enlace. Não foram definidos nos PDFs.

### Telecomandos

O OBC recebe os TC encaminhados pelo TT&C, valida-os e executa as ações em um tempo de resposta definido pela equipe (**REQ-OBC-002**). Como organização de implementação, utilizar um fluxo comum de validação e encaminhamento para as interfaces autorizadas, respeitando o modo atual e as condições de cada comando.

O catálogo de comandos, os parâmetros e a forma de confirmação ou rejeição precisam ser definidos. A integração dos comandos que dependem dos modos de missão e das atuações físicas é concluída na etapa 8.

**Entrega sugerida:** TM inspecionável e comandos válidos encaminhados às ações, com resposta e tempo de execução verificáveis.

---

## 8. Modos de missão, energia e atuação

### Modos operacionais

Os modos de voo devem ser consistentes com o **ConOps** (**REQ-OBC-004**). Os documentos citam detumbling e modos de baixo consumo, mas não fornecem a lista completa de estados ou suas transições. Essa lista deve ser obtida do ConOps, sem criar modos por suposição.

### Prioridades e energia

Quando a disponibilidade energética estiver reduzida, o sistema deve priorizar tarefas críticas (**REQ-OBC-012**). O software também deve ser otimizado para não ultrapassar a cota energética estipulada pelo EPS para processamento de dados (**REQ-NF-OBC-001**).

A expressão “baixo consumo energético disponível” do requisito é interpretada aqui como disponibilidade reduzida de energia. Limiares, tarefas críticas e políticas de redução de atividade devem ser acordados com EPS e ConOps e verificados na integração.

### Atuações previstas

| Atuação | Interface prevista | Condição ainda necessária |
|---|---|---|
| Manobra de atitude | Solicitação lógica ao ADCS; PWM / EN previstos na pinagem. | Conciliar responsabilidade e definir condições de habilitação e execução. |
| Thermal Watchdog | Acionamento pelo OBC citado na arquitetura. | Definir interface, limites térmicos e ação de recuperação. |
| Burn Wire | GPIO 33 para MOSFET de liberação da antena. | Definir autorização, duração, estado em reset e comportamento após reinicialização. |

**Entrega sugerida:** transições de modo verificáveis, comandos condicionados ao estado operacional e atuação dentro dos limites acordados.

---

## 9. Autonomia e recuperação de falhas

O sistema deve operar autonomamente durante a missão, mantendo suporte a TC e geração de TM sem intervenção manual (**REQ-OBC-001**). Deve detectar falhas e realizar reinicialização automática dentro de um tempo determinado (**REQ-OBC-007**) e identificar perda de comunicação com subsistemas (**REQ-OBC-011**).

O watchdog do FreeRTOS é a mitigação indicada para travamentos. Sua supervisão precisa representar a saúde das tarefas críticas, com condições e tempos de recuperação definidos. 

> O Thermal Watchdog é uma função distinta, cujo contrato ainda está pendente.

Falhas críticas, eventos e reinicializações devem alimentar os registros e estatísticas das etapas 4 e 5. A retomada de operação deve respeitar os estados das saídas e os modos previstos, especialmente para motor e Burn Wire.

### Riscos e alternativas considerados nos PDFs

| Decisão | Alternativa considerada | Motivo da escolha | Risco | Mitigação indicada |
|---|---|---|---|---|
| ESP32 como OBC central | Raspberry Pi como computador único. | Menor consumo e simplicidade operacional. | Ponto único de falha; travamento ou falha física interrompe o sistema. | Watchdog para travamentos; armazenamento no OBC preserva os dados já recebidos se o Payload Linux falhar. |
| Cartão SD | Operação sem armazenamento. | Conservação local e redundância de coleta dos dados. | Brownout durante escrita pode corromper arquivos ou FAT32. | Fechamento de arquivos e monitoramento da alimentação. |
| UART entre Payload e OBC | SPI, CAN ou USB. | Simplicidade, baixo custo e compatibilidade com ESP32. | Excesso de JSON pode causar estouro de buffers, perdas e inconsistências. | UART em alta velocidade, buffers e filas FreeRTOS para absorver variações de taxa. |

> O watchdog não recupera uma falha física do ESP32. O SD conserva dados já gravados, mas a arquitetura fornecida não define redundância física do OBC ou do armazenamento. Essas limitações permanecem mesmo após implementar as mitigações.

**Entrega sugerida:** perda de comunicação, travamentos e interrupções de alimentação exercitados, com recuperação e efeitos nos dados documentados.

---

## 10. Validação e acabamento

### Critérios fornecidos nos requisitos

| ID associado no PDF | Método | Aplicação indicada |
|---|---|---|
| REQ-OBC-001 | Teste | Injetar TC pelo barramento e medir o tempo até a execução da ação. |
| REQ-OBC-003 | Inspeção | Conferir estrutura, integridade e ordenação dos campos de status, energia e atitude na TM. |
| REQ-OBC-006 | Demonstração | Injetar ADS-B simulado e demonstrar a inclusão correta e legível do timestamp na TM resultante. |

A associação do teste de TC a **REQ-OBC-001** foi preservada. Esse teste também é pertinente a **REQ-OBC-002**, mas sozinho não demonstra a autonomia durante toda a missão exigida por REQ-OBC-001.

### Verificação complementar proposta

Para completar a cobertura, verificar inicialização automática, operação sem intervenção manual, transições de modo, priorização energética, registro de eventos e estatísticas, contagem de reinicializações, perda de comunicação e recuperação após falhas. Medir os tempos, a resolução temporal e o consumo contra os valores que a equipe definir.

Conferir os registros após reinicializações e interrupções durante escrita, além de exercitar mensagens incompletas ou inválidas, rajadas do Payload e indisponibilidade de periféricos. Registrar evidências junto aos requisitos atendidos.

O acabamento consiste em revisar clareza, consistência dos termos, links e exemplos reais de configuração e uso. Aplicar o padrão de C++ já existente e consolidar ferramentas redundantes identificadas, sem criar outro documento de convenções.

**Entrega sugerida:** checklist atualizado, evidências de validação e documentação coerente com a implementação real.

---

## Catálogo e rastreabilidade dos requisitos

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

---

## Referências e controle de origem

| Material | Identificação no arquivo | Uso nesta consolidação |
|---|---|---|
| Definição de Arquitetura OBC.pdf | Versão 1.5, de 10/07/2026. | Estratégia, componentes, fluxos, energia, rastreabilidade e riscos. |
| Especificação de Requisitos OBC.pdf | Capa: v1.5, de 25/05/2026; histórico: revisão 1.6, de 29/05/2026. | Objetivo, escopo, requisitos, restrições, interfaces e verificação. |
| Alocação de Pinos.pdf | Sem versão ou data explícita. | Interfaces físicas e identificação de divergências. |

Consultar também ConOps, ICDs, regulamento, arquitetura lógica do CEFAST Aerospace e o padrão existente de C++. Esses materiais foram citados no contexto do projeto, mas seus conteúdos não foram fornecidos para esta consolidação.
