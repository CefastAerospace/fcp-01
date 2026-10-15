# Arquitetura vigente do OBC-DH

Este documento descreve decisões e comportamento planejado. [REQUISITOS.md](REQUISITOS.md) preserva obrigações, validação e referências históricas; [TODO.md](TODO.md) concentra estado do código, trabalho e pendências. Decisão documentada não comprova implementação ou teste.

## 1. Escopo e responsabilidades

O ESP32 executará o firmware integrado sobre Arduino/FreeRTOS. O OBC-DH organiza execução, circulação de dados, telecomandos, telemetria e registros. O Payload, em Raspberry Pi Zero W, entrega ADS-B já decodificado por UART. O ADCS executará no mesmo ESP32 do OBC, conforme confirmado pelo responsável nesta revisão.

| Área | Responsabilidade na integração |
|---|---|
| OBC-DH | Inicialização, integração dos módulos, referência temporal, registros, composição de TM, validação lógica de TC, modos e supervisão. |
| Payload | Recepção em 1090 MHz, decodificação ADS-B e produção dos dados; implementação da ponta UART no Raspberry Pi. |
| TT&C | Enlace RF, enquadramento, integridade e transporte de TC/TM/dados pela biblioteca existente. |
| ADCS | Algoritmos, aquisição dos sensores e controle físico do motor; fornecer comandos, estado e limites de execução para integração. |
| EPS / Térmico | Gestão elétrica física e contrato de aquisição/proteção térmica; fornecer dados, orçamento e condições operacionais. |
| Estruturas / EPS / TT&C | Condições físicas, energéticas e operacionais da liberação da antena por Burn Wire. |

Respeitar ICDs, ConOps, arquitetura lógica CEFAST Aerospace, alimentação do EPS e regulamento. Referências ausentes e parâmetros desconhecidos constam do checklist. Não reimplementar algoritmos de outros setores nem criar módulos vazios antecipadamente.

## 2. Decisões vigentes e interfaces

As decisões de 06/10/2026 sobre IMU, luminosidade, EN e Burn Wire permanecem. A consolidação de 14/10/2026 incorpora as decisões de interfaces dos demais setores: Payload por UART binária, ADCS no ESP32 central, LoRa/VSPI e SD/HSPI. Tempo usa ms monotônicos por boot e referências recebidas pelo LoRa; RTC externo não integra a configuração atual e só será reconsiderado se esse método não atender à validação temporal acordada. Parâmetros de precisão e implementação continuam abertos.

| Interface / recurso | Uso e decisão | Definições ainda necessárias |
|---|---|---|
| UART0 / USB | TC/TM JSON de bancada, um objeto por linha, pela biblioteca `UmbilicalLink`. | Conferir circuito da placa e integração no OBC. |
| UART do Payload | Troca bidirecional com Raspberry Pi; TX 17 / RX 16, 115,2 kbps 8N1, binário compacto com SeqID + NACK. | Porta no Pi, montagem, enquadramento/versão final e limites de tráfego. |
| I2C | Sensores de ADCS, EPS e Térmico, incluindo encoder conforme montagem. | Inventário, parâmetros elétricos, frequência e acesso coordenado. |
| VSPI | Rádio LoRa SX1276, 921 MHz SF7, `SPI` global. | Conferência elétrica, montagem e validação de bancada. |
| HSPI | Cartão SD, conforme pinagem local. | Conferência da montagem, velocidade e acesso aos arquivos. |
| GPIO / PWM | Rádio, EN/fases do motor e Burn Wire. | Níveis ativos, estados em boot/reset e condições de atuação. |

A referência dos GPIOs e endereços está em [PINAGEM.md](PINAGEM.md). GPIOs 16/17 são a alocação de projeto já presente em `Pinout.h`; disponibilidade no módulo e montagem precisam de conferência. UART exige TX/RX cruzados, GND comum e níveis de 3,3 V; a ponta Raspberry Pi precisa de UART habilitada e verificação de conflito com console serial e outras funções da porta escolhida. O mapa SPI invertido anterior permanece apenas no histórico.

Reutilizar `Wire`, `HardwareSerial`, `SPI` e `SD` do framework, com instância própria de HSPI para o SD. Integrar a biblioteca [TT&C](../../tt-c/include/Ttc.h), seu [protocolo](../../tt-c/include/TtcProtocol.h) e o [contrato JSON compartilhado](../../common/protocol/ttc-json.md): filas, ACK/NACK prioritário, deduplicação e `CMD_SET_TIME`/`set_time`. CRC é do hardware LoRa; não há CRC16 de aplicação nem fragmentação. Manter satélite e estação na mesma versão, sem protocolo paralelo ou PUS presumido.

## 3. Fluxo dos dados e modelos

O fluxo de missão será: recepção UART, delimitação, validação, associação de tempo, gravação e seleção para envio pelo TT&C. Dados de energia, atitude, estado, eventos, falhas e estatísticas também alimentam registros e TM conforme os contratos.

UART é um fluxo de bytes: uma leitura pode conter parte de uma mensagem ou várias mensagens. O contrato de voo do Payload é binário compacto (~25–35 bytes estimados), com SeqID + NACK; JSON local permanece formato de bancada. Definir serialização, delimitação, tamanho máximo, timeout e recuperação após entrada inválida antes da integração, sem transmitir memória bruta de structs. Taxas, rajadas, buffers/retenção e tratamento de perdas estão em P-PAYLOAD/P-DADOS no checklist. JSON por linha aplica-se ao umbilical TT&C, não ao enlace Payload.

| Referência atual do Payload | Significado a preservar |
|---|---|
| `icao`, `lat`, `lon` | Identificador ICAO e posição geográfica. |
| `alt`, `vel`, `heading` | Altitude em pés, velocidade em nós e direção em graus. |
| `timestamp` | UTC em ISO 8601 do log de bancada; preservar seu significado, separado do timestamp de voo associado pelo OBC em ms. |
| `msg_type`, `val_flags`, `seq_id` | Tipo, validade mínima e sequência atuais; semântica de dados acumulados e reset ainda precisa de contrato. |

Essa referência vem de [payload/README.md](../../payload/README.md) e do script existente. Não remover campos, converter unidades ou redefinir timestamps silenciosamente. Registrar tipos, limites, campos opcionais e versão do contrato. Fixar tamanho máximo em bytes, pois o comprimento do JSON varia.

Prever futuramente `headers/models/` para tipos específicos efetivamente utilizados: dados do Payload, estado/energia/atitude e registros de eventos/tempo. A pasta será criada quando esses tipos forem implementados. Python e C++ terão representações locais do contrato acordado; não é necessário gerar código ou criar um adaptador universal. Campo opcional novo pode ser compatível se o consumidor ignorar campos desconhecidos; mudança de nome, tipo, unidade ou significado exige coordenação ou nova versão.

O SD conserva registros completos; downlink agrega estado por aeronave, conforme decisão de Payload/TT&C. `Ttc::Send()` recebe JSON sem `n`, de até 245 bytes; o TT&C insere o contador na transmissão e limita o quadro final a 255 bytes. Usar `ttc::BuildAircraft()` e `Ttc::SendTelemetry()` para os formatos existentes. Mensagens maiores são rejeitadas, sem fragmentação. Definir seleção/frequência, agrupamento e tratamento de falhas de serialização ou enfileiramento. ACKs já têm prioridade; transmissão nominal é sob demanda. Enfileirar ou transmitir não comprova recepção no solo.

## 4. Organização da execução

Separar o atendimento frequente do controle FOC das operações demoradas do rádio e do SD. O código ADCS existente não admite operações bloqueantes no atendimento do motor. Após `Ttc::Init()`, `Ttc::Start()` cria a tarefa do enlace no Core 0, prioridade padrão 10; só essa tarefa acessa o rádio e executa `Update()`. Não chamar `Update()` em paralelo com `Start()`. A transmissão por quadro espera TX Done com timeout de 1 s, sem bloquear a tarefa FOC.

OBC consome TCs com `ReceiveTelecommand()` e responde com `SendAck()`; `Send()` e `SendTelemetry()` enfileiram a descida. DIO0 apenas notifica a tarefa na ISR, sem acesso SPI. Quantidade, prioridades e afinidade das demais tarefas dependem de P-ADCS/P-MISSAO e das medições; preservar a decisão de execução do TT&C, sem impor o antigo mapa completo de tarefas por núcleo.

Estabelecer responsável pelo acesso a cada recurso. Coordenar sensores/encoder no I2C sem inicializações concorrentes ou transações intercaladas. Definir limites de espera e recuperação de SDA/SCL presos. Separar barramentos SPI não resolve concorrência sobre um driver ou arquivo. Reutilizar filas do TT&C; acrescentar filas/buffers limitados somente onde forem necessários para absorver rajadas ou isolar escrita.

Integrar o ADCS como módulo do firmware central: seus `setup()`/`loop()` atuais são da bancada e não podem coexistir como uma segunda entrada do OBC. A integração preservará os algoritmos e conciliará EN, fases, encoder e alinhamento no boot conforme o checklist. Não há aprovação automática das frequências, prioridades ou estados apenas por existirem no código de bancada.

## 5. Tempo e persistência

O OBC associará ms desde o boot aos dados recebidos e aos eventos, com referências absolutas recebidas pelo LoRa. A correção retroativa é a reconstrução do horário a partir dessas referências, sem reescrever registros já gravados. DS3231 permanece em [REQUISITOS.md](REQUISITOS.md) como referência histórica e compra registrada. RTC externo só será reconsiderado se o método monotônico/LoRa não atender aos critérios de P-TEMPO; não existe instalação aprovada nesta configuração.

| Registro planejado | Informação mínima |
|---|---|
| Dado / evento | Identificador do boot, tempo monotônico local e conteúdo/origem. |
| Sincronização | Identificador do boot, instante monotônico local da recepção e referência absoluta recebida, com validade e significado definidos. |
| Reinicialização | Identificação do novo boot e dados acordados para contagem e retomada. |

No mesmo boot, a reconstrução básica é `horário do evento ≈ horário da referência + (tempo do evento − tempo da referência)`. Referências sucessivas podem permitir estimar deriva; método, unidades, resolução e representação persistente ainda serão definidos. A precisão depende do instante representado pelo horário enviado, do atraso no enlace e da deriva local.

Antes da primeira referência válida, conservar tempo relativo e indicar horário absoluto desconhecido. Não transportar o offset de um boot anterior como se o relógio tivesse continuado contando: o intervalo desligado é desconhecido. Uma referência posterior pode permitir reconstruir os eventos daquele novo boot. A validade temporal precisa chegar também à TM conforme contrato com TT&C, sem substituir desconhecido por UTC inventado.

Receber `CMD_SET_TIME`/`set_time` por meio do TT&C, validar `args.unix` em segundos Unix e acrescentar uma referência associada ao contador local em ms. O contrato TT&C usa segundos Unix em `ts`; converter unidades explicitamente, sem alterar esse contrato ou confundir a resolução dos registros internos com a do downlink. Precisão também depende da resolução de `args.unix` e da latência; o exemplo de enlace não comprova persistência ou recuperação do OBC. Definir identificação persistente de boots sem ambiguidades após reset, inclusive com SD indisponível.

O SD armazenará missão, TM, eventos, falhas, estatísticas e reinicializações. FAT32 é a referência dos PDFs; definir formato, retenção, extração, escrita/sincronização, fechamento e comportamento de SD cheio/indisponível. Recuperar registros incompletos e avaliar o sistema de arquivos após interrupção de alimentação. Escrita em append ou fechamento normal não garante integridade durante uma queda.

## 6. Missão e supervisão

Inicializar automaticamente após energização, aplicar os estados de saída acordados, identificar falhas e iniciar no modo do ConOps. Considerar motor e Burn Wire durante boot/reset, inclusive os níveis impostos pelo circuito antes de o firmware executar.

O TT&C valida o envelope JSON; o OBC valida `args`, modo e condições de execução, encaminha a ação e responde com `SendAck()`. O TT&C deduplica os oito TCs mais recentes por 30 s, com reenvio do ACK anterior; isso não garante proteção depois de reset ou fora dessa janela. Definir persistência/condições das atuações críticas sem duplicar a deduplicação já existente. Comando catalogado não comprova ação implementada ou aprovada no ConOps. Umbilical usa o mesmo contrato JSON; ACK volta pelo meio de origem.

Compor TM com status, energia, atitude e contagens da missão via `SendTelemetry()`. Estados ADS-B agregados usam pacote `t = "ac"`, gerado por `BuildAircraft()` e enfileirado com `Send()`. Essa separação de conteúdo não substitui a demonstração de timestamp na TM prevista nos requisitos; preservar o critério e acordar sua evidência com os responsáveis. Não alterar entradas ou resultados esperados para acomodar o formato do enlace.

Integrar modos/transições registrados no README geral e no contrato TT&C, conferindo o ConOps vigente. Preservar LOW-POWER com rádio em RX para o TC de recuperação; beacon periódico é habilitado só em safe-mode e permanece desligado por padrão. Prioridades energéticas e Thermal Watchdog dependem de EPS/ConOps; proteção térmica é distinta do watchdog de tarefas.

Burn Wire exige autorização, duração, energia, persistência e confirmação física. Usar `Ttc::HoldTransmit(true, wait_ms)` e conferir ausência de TX antes do acionamento; retomar conforme política acordada. Interlock de rádio não autoriza a queima nem comprova liberação. Preservar os limites/ensaios dos setores; não concluir atuação por uma flag volátil.

Detectar perda de comunicação, falhas e travamentos, registrar eventos/reinicializações e recuperar periférico, tarefa ou OBC conforme limites acordados. Ausência de aeronaves ADS-B não significa perda de comunicação com o Payload. Watchdog não recupera falha física; não existe redundância física confirmada. Validar entradas, limites e falhas desde cada integração, seguindo as cinco entregas do checklist.

Reutilizar `GetLinkStats()`/`last_tc_ms` e a verificação de saúde/reset do SX1276 já implementada no TT&C. Camada de integração deve preservar os limites registrados por esse setor (entrega do TC ao OBC ≤200 ms e resposta ≤2 s), sem alterar testes ou declarar atendimento por inspeção documental. Enquadramento binário UART, proteção EPS/Térmico, atendimento FOC e persistência do OBC mantêm contratos/evidências próprios.
