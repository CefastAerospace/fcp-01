# Arquitetura vigente do OBC-DH

Este documento distingue decisões vigentes, comportamento implementado e integração prevista. Revisão documental de 10/11/2026 sobre `839a186`; evidências de execução permanecem datadas nas tarefas. [REQUISITOS.md](REQUISITOS.md) preserva obrigações e validação; [TODO.md](TODO.md) reúne estado e dependências. Decisão documentada não comprova implementação, aceite externo ou teste físico.

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

As decisões de 06/10/2026 sobre IMU, luminosidade, EN e Burn Wire permanecem. A consolidação de 14/10/2026 mantém ADCS no ESP32 central, LoRa/VSPI e SD/HSPI. Em 05/11/2026, o responsável substituiu a decisão de UART binária do Payload por snapshots JSON completos diretamente pela UART. Tempo usa ms monotônicos por boot e referências recebidas pelo LoRa; RTC externo não integra a configuração atual e só será reconsiderado se esse método não atender à validação temporal acordada. Limites de precisão física e integrações de missão continuam abertos.

| Interface / recurso | Uso e decisão | Definições ainda necessárias |
|---|---|---|
| UART0 / USB | TC/TM JSON de bancada, um objeto por linha, pela biblioteca `UmbilicalLink`. | Integração USB local verificada; conferir circuito e aceite TT&C. |
| UART do Payload | Snapshots JSON em FCPU local; TX 17 / RX 16, 115,2 kbps 8N1. | Aceite do enquadramento pelo Payload, produtor/porta Pi, montagem, tráfego e retenção em INT-006. |
| I2C | Sensores de ADCS, EPS e Térmico, incluindo encoder conforme montagem. | Executor local a 400 kHz/20 ms por transferência; inventário, montagem e integração dos consumidores ainda pendentes. |
| VSPI | Rádio LoRa SX1276, 921 MHz SF7, `SPI` global. | Conferência elétrica, montagem e validação de bancada. |
| HSPI | Cartão SD, conforme pinagem local. | Conferência da montagem, velocidade e acesso aos arquivos. |
| GPIO / PWM | Rádio, EN/fases do motor e Burn Wire. | Níveis ativos, estados em boot/reset e condições de atuação. |

A referência dos GPIOs e endereços está em [PINAGEM.md](PINAGEM.md). GPIOs 16/17 são a alocação de projeto já presente em `Pinout.h`; disponibilidade no módulo e montagem precisam de conferência. UART exige TX/RX cruzados, GND comum e níveis de 3,3 V; a ponta Raspberry Pi precisa de UART habilitada e verificação de conflito com console serial e outras funções da porta escolhida. O mapa SPI invertido anterior permanece apenas no histórico.

Reutilizar `Wire`, `HardwareSerial`, `SPI` e `SD` do framework, com instância própria de HSPI para o SD. Integrar a biblioteca [TT&C](../../tt-c/include/Ttc.h), seu [protocolo](../../tt-c/include/TtcProtocol.h) e o [contrato JSON compartilhado](../../common/protocol/ttc-json.md): filas, ACK/NACK prioritário, deduplicação e `CMD_SET_TIME`/`set_time`. CRC é do hardware LoRa; não há CRC16 de aplicação nem fragmentação. Manter satélite e estação na mesma versão, sem protocolo paralelo ou PUS presumido.

## 3. Fluxo dos dados e modelos

O fluxo de missão será: recepção UART, delimitação, validação, associação de tempo, gravação e seleção para envio pelo TT&C. Dados de energia, atitude, estado, eventos, falhas e estatísticas também alimentam registros e TM conforme os contratos.

UART é um fluxo de bytes: uma leitura pode conter parte de um quadro ou vários quadros. O receptor implementado recebe snapshots JSON completos conforme `obc-dh/payload.json`, com delimitação, tamanho, integridade, sequência/repetição e timeouts definidos no [FCPU v1](../../common/protocol/payload-uart.md). O cabeçalho e os controles binários também atravessam o cabo; JSON como conteúdo não implica UART composta apenas por texto. O aceite do Payload, o produtor Pi e os parâmetros de tráfego/retenção permanecem em INT-006. A decisão antiga de payload binário compacto não vigora. A TES-005 conserva sua rastreabilidade e passa a exigir snapshots JSON, conforme orientação do responsável em 10/11/2026; os cenários de falha são mantidos. JSON por linha continua sendo o enquadramento do umbilical TT&C, não uma definição automática para o Payload.

**Implementação local e integração:** COM-004 fornece `PayloadReceiver` e `PayloadReceiverEsp32`; a primeira etapa INT-006 já inicia recepção e diário no firmware normal. O contexto reserva dois blocos no heap. O produtor Pi continua sem UART implementada no repositório. Os ensaios nativos, a injeção em RAM, o loopback legado e a bancada integrada simulada comprovam escopos distintos; nenhum deles comprova troca ESP32–Pi. O beta permanece isolado em `uart-loopback`.

| Referência legada do Payload (`telemetry.py`) | Significado a preservar |
|---|---|
| `icao`, `lat`, `lon` | Identificador ICAO e posição geográfica. |
| `alt`, `vel`, `heading` | Altitude em pés, velocidade em nós e direção em graus. |
| `timestamp` | UTC em ISO 8601 do log de bancada; preservar seu significado, separado do timestamp de voo associado pelo OBC em ms. |
| `msg_type`, `val_flags`, `seq_id` | Tipo, validade mínima e sequência atuais; semântica de dados acumulados e reset ainda precisa de contrato. |

O snapshot atual usa `id`/`messages` e os campos textuais da amostra; seu mapeamento está no contrato OBC. A tabela acima descreve o modelo legado, não o esquema UART atual. Essa referência vem de [payload/README.md](../../payload/README.md) e do script existente. Não remover campos, converter unidades ou redefinir timestamps silenciosamente. Registrar tipos, limites, campos opcionais e versão do contrato. Fixar tamanho máximo em bytes, pois o comprimento do JSON varia.

`headers/models/ObcData.h` e `src/ObcData.cpp` implementam modelos portáteis e validadores do [contrato OBC](../../common/protocol/obc-data.md). Os registros v1 cobrem atualizações individuais de missão, TM local, eventos/falhas, estatísticas, boot e referências de tempo. Em 05/11/2026 foram acrescentados `PAYLOAD_SNAPSHOT`, `AIRCRAFT_STATE` e `EXTRA_FIELD`, reutilizando `EVENT_STAMP`, `STATUS` e os bits de posição/altitude. Arrays limitados conservam até 20 aeronaves, campos padronizados opcionais e extras textuais. O parser FCPU pertence a PayloadReceiver; DAD-003 implementa seu formato persistido v2 próprio. Os aceites dos produtores seguem nas integrações correspondentes; o script atual do Payload e o contrato TT&C permanecem preservados.

Cada snapshot válido substitui a lista ativa por ICAO; lista vazia é válida. O receptor conserva o estado anterior em entrada incompleta/inválida e não apaga histórico. `tick` conserva o texto original, separado do instante OBC; não prova que todos os campos foram adquiridos juntos. `id` é opaco. Coordenadas não decodificadas permanecem nos extras, sem validade numérica. Não inferir unidades de velocidade. Limites, tamanho em memória e mapeamento estão no contrato; o snapshot não deve ficar na pilha padrão de uma tarefa FreeRTOS. A preservação de cada mensagem ADS-B intermediária ainda precisa ser conciliada com os snapshots sem mudar os critérios existentes.

A persistência v1 proposta é binária: assinatura FCP1, versão/tipo, comprimento, boot e ms monotônicos, corpo tipado e CRC-32/ISO-HDLC, sem padding da RAM. Registros atuais têm 36–105 bytes e limite global de 512 bytes. Campos indisponíveis usam máscara no binário e `null` nos exemplos JSON. Mudanças do layout binário exigem nova versão; codec, leitura e tratamento de truncamento pertencem a DAD-003, enquanto COM-005 cuida de I/O e integridade física. O orçamento de armazenamento foi atualizado sem alterar os parâmetros do rádio ou critérios de validação.

DAD-003 acrescentou snapshots v2 variáveis: `FCP2`, versão 2/tipo 8, cabeçalho de 24 bytes nas posições anteriores, corpo com `id`/lista/indisponibilidade/extras e CRC do mesmo algoritmo. Cada string grava comprimento u8 e bytes significativos; campos ausentes são omitidos. Um snapshot vazio tem 37 bytes e o máximo 64497, sem alterar o limite ou os layouts v1. As APIs `GetSnapshotSize`, `EncodeSnapshot` e `DecodeSnapshot` reutilizam os validadores; a união `RECORD` mantém os corpos pequenos v1 e também atende a API dedicada de TM v3, sem incorporar o snapshot. Decodificação valida integralmente antes de publicar em duas passagens, com espaço temporário de uma aeronave, sem outro snapshot completo ou alocação dinâmica. Modelo e buffer de saída pertencem ao chamador e não devem ficar na pilha padrão das tarefas; o orçamento de pilha dos helpers também precisa ser considerado na integração.

`ObcRecordCodec` implementa localmente esse layout, reutilizando os validadores. Codificação/decodificação usam buffers do chamador, sem alocação dinâmica ou I/O, e não publicam saídas parciais em falha. O [leitor nativo](../tools/README.md) usa os mesmos decoders para exportar JSON Lines de arquivos v1/v2/v3/mistos: descarta trechos inválidos da interpretação, informa seus offsets e procura o próximo registro integralmente validado. Não corrige bytes, não pede retransmissão e não modifica o arquivo. Em duas passagens, coleta referências e reconstrói o horário por boot usando a última referência anterior, ou a primeira para eventos anteriores à sincronização; empates usam a última ocorrência no arquivo. Os registros originais e seus horários próprios são preservados. O desenvolvimento do codec/leitor DAD-003 foi encerrado em 09/11/2026. Aceites dos produtores seguem nas integrações INT-004/INT-005/INT-006; estados duráveis e retomada seguem em INT-007/INT-008. Eventos do Burn Wire continuam sem constituir autorização de atuação.

O objetivo é conservar registros completos no SD e enviar estado agregado por aeronave, conforme decisão de Payload/TT&C. A primeira etapa já grava, com descartes explícitos quando faltam recursos; o envio agregado continua pendente na INT-006. Isso não declara atendimento aos critérios de conservação e perdas. `Ttc::Send()` recebe JSON sem `n`, de até 245 bytes; o TT&C insere o contador na transmissão e limita o quadro final a 255 bytes. Usar `ttc::BuildAircraft()` e `Ttc::SendTelemetry()` para os formatos existentes. Mensagens maiores são rejeitadas, sem fragmentação. Definir seleção/frequência, agrupamento e tratamento de falhas de serialização ou enfileiramento. ACKs já têm prioridade; transmissão nominal é sob demanda. Enfileirar ou transmitir não comprova recepção no solo.

## 4. Organização da execução

`main.cpp` executa `Serial.begin(115200)`, o coordenador `ObcStartup`, `obc_ttc::Init()` e `obc_flow::esp32::Init(report)`, nessa ordem. O coordenador tenta tempo/NVS, I2C e montagem SD, conservando o resultado de cada serviço mesmo quando outro falha. Boot indisponível fica com ID zero no relatório; não fabricar identificação ou UTC. A montagem não cria arquivos por si: o diário da INT-006 os cria depois, se houver recursos. `loop()` chama o atendimento TT&C e os diagnósticos do fluxo; a espera de 100 ms da entrega inicial INT-003 é histórica. O atendimento TT&C espera até 20 ms por TC. Logs `[OBC]`/`[FLOW]` são distintos dos pacotes JSON.

O coordenador de boot permanece intacto. Após ele, `ObcTtc` inicializa rádio e umbilical; se um transporte estiver pronto, tenta iniciar a tarefa TT&C existente. Na instância recém-criada, o umbilical só fica pronto depois da criação de todas as filas e do mutex. Assim, falha do rádio não é confundida com falha de filas nem desabilita o USB. A integração TT&C não duplica suas tarefas ou filas. A primeira etapa INT-006 acrescenta consumidores próprios para UART/processamento e gravação, descritos abaixo. Falha de início é diagnosticada sem interromper os outros serviços. A recuperação periódica do rádio pertence ao TT&C.

O firmware inicia a recepção do Payload e o diário quando houver recursos, conforme INT-006; não inicia aquisição de sensores, missão ou atuadores. Não programa EN/PWM/Burn Wire ou afirma segurança elétrica de reset por omissão dessas saídas. A tarefa I2C existente é iniciada pelo próprio serviço. O modo de voo SAFE/INIT e as condições elétricas continuam pendentes em INT-003. Essa entrega não substitui o ensaio físico de TES-008.

O atendimento atual aceita `ping`, `get_status` e `request_tm`, sem argumentos ou com objeto vazio, e `set_time` com `args.unix` u32. Outros comandos catalogados recebem `invalid_cmd`; argumentos inesperados recebem `invalid_param`. Pedidos de TM só são confirmados após enfileiramento. `set_time` confirma aplicação em RAM, com `state_rejected` se o fluxo não estiver ativo; persistência é separada. A TM publica `bench`, UTC e quantidade de aeronaves quando disponíveis, mantendo `null` para fontes ausentes, inclusive `msg`. RSSI/SNR só ficam disponíveis depois de TC RF. O aceite conjunto TT&C continua na INT-004; modos/atuações seguem na INT-007.

Separar o atendimento frequente do controle FOC das operações demoradas do rádio e do SD. O código ADCS existente não admite operações bloqueantes no atendimento do motor. Após `Ttc::Init()`, `Ttc::Start()` cria a tarefa do enlace no Core 0, prioridade padrão 10; só essa tarefa acessa o rádio e executa `Update()`. Não chamar `Update()` em paralelo com `Start()`. A transmissão por quadro espera TX Done com timeout de 1 s, sem bloquear a tarefa FOC.

OBC consome TCs com `ReceiveTelecommand()` e responde com `SendAck()`; `Send()` e `SendTelemetry()` enfileiram a descida. DIO0 apenas notifica a tarefa na ISR, sem acesso SPI. Quantidade, prioridades e afinidade das demais tarefas dependem de [INT-005](../.tasks/03-integracao/INT-005-sensores-e-adcs.md)/[INT-007](../.tasks/03-integracao/INT-007-comandos-modos-e-protecoes.md) e das medições; preservar a decisão de execução do TT&C, sem impor o antigo mapa completo de tarefas por núcleo.

Estabelecer responsável pelo acesso a cada recurso. Coordenar sensores/encoder no I2C sem inicializações concorrentes ou transações intercaladas. Definir limites de espera e recuperação de SDA/SCL presos. Separar barramentos SPI não resolve concorrência sobre um driver ou arquivo. Reutilizar filas do TT&C; acrescentar filas/buffers limitados somente onde forem necessários para absorver rajadas ou isolar escrita.

Integrar o ADCS como módulo do firmware central: seus `setup()`/`loop()` atuais são da bancada e não podem coexistir como uma segunda entrada do OBC. A integração preservará os algoritmos e conciliará EN, fases, encoder e alinhamento no boot conforme o checklist. Não há aprovação automática das frequências, prioridades ou estados apenas por existirem no código de bancada.

### Primeira etapa INT-006

Após o boot e TT&C, `ObcDataFlowEsp32` inicia o transporte UART2 (Core 1/prioridade 3/pilha 3 KiB), o processamento (Core 1/prioridade 2/pilha 8 KiB) e, quando disponível, o escritor SD (Core 1/prioridade 1/pilha 8 KiB). O transporte é o único proprietário de Serial2 e usa ring fixo de 1 KiB; somente o processador acessa o receptor e a lista. O escritor recebe oito cópias de registros pequenos de até 105 bytes e um bloco exclusivo de snapshot, dividido em 32768 + 31729 bytes. Proteção compartilhada cobre apenas metadados/cópias pequenas, sem I/O ou parse dentro dela.

O resumo de TM não copia a lista de aeronaves. O mesmo snapshot válido fornece `ac`; o tempo da TM é reconstruído no instante da composição. `msg` não é inferido de `id`, pacotes ou aeronaves. `SendTelemetry` devolve opcionalmente a amostra enfileirada para persistência v3. A aplicação de `set_time` confirma RAM e tenta gravar a referência separadamente; ausência/falha de SD não fabrica persistência.

O diário serializa BOOT, referências, snapshots v2, TM v3, estatísticas e eventos/falhas. Cria nomes exclusivos por boot/segmento e rotaciona entre registros; falha terminal desativa escrita. Ocupação de fila/bloco ou SD indisponível gera descarte contado, sem retenção ilimitada e sem interromper recepção/TM. `records_written` só conta um snapshot partido após ambas as partes. A segunda etapa conserva seleção/envio de aeronaves e extração de arquivos; recuperação automática continua em INT-008.

`poll_gap_ms` mede a tarefa de transporte e `process_gap_ms` mede o consumidor. O parser pode demorar mais de 10 ms sem bloquear o atendimento UART. As medições da bancada e os limites de evidência constam na INT-006. Logs concorrentes novos usam uma escrita por linha, incluindo a quebra; não podem envolver um pacote JSON na mesma linha.

## 5. Tempo e persistência

O OBC associará ms desde o boot aos dados recebidos e aos eventos, com referências absolutas recebidas pelo LoRa. A correção retroativa é a reconstrução do horário a partir dessas referências, sem reescrever registros já gravados. DS3231 permanece em [REQUISITOS.md](REQUISITOS.md) como referência histórica e compra registrada. RTC externo só será reconsiderado se o método monotônico/LoRa não atender aos critérios de [DAD-002](../.tasks/02-dados-e-tempo/done/DAD-002-tempo-e-boots.md); não existe instalação aprovada nesta configuração.

| Registro planejado | Informação mínima |
|---|---|
| Dado / evento | Identificador do boot, tempo monotônico local e conteúdo/origem. |
| Sincronização | Identificador do boot, instante monotônico local da recepção e referência absoluta recebida, com validade e significado definidos. |
| Reinicialização | Identificação do novo boot e dados acordados para contagem e retomada. |

O núcleo `ObcTime` implementa, no mesmo boot, `unix_ms = unix_s * 1000 + delay_ms + (evento_ms - recepção_ms)`, com diferenças tratadas sem estouro ou underflow. Cada referência conserva sua compensação. O parâmetro global `obc_time::reference_delay_ms` começa em 162 ms, ajustável por recompilação: airtime estimado de um `set_time` de 63 bytes nos parâmetros LoRa atuais. Supõe horário capturado no início efetivo do TX; tamanho variável, filas, retransmissões, quantização em segundos e deriva limitam a precisão. Não é erro máximo garantido. Uma estimativa dinâmica futura pode fornecer a compensação da nova referência sem reinterpretar as anteriores.

`ObcTimeEsp32` fornece `esp_timer_get_time() / 1000` em 64 bits e reserva um contador de boots na NVS, sem SD. Primeiro boot confirmado = 1; contagem de reinicializações registradas = identificador menos um. Só publica o ID depois do commit e não apaga a NVS em caso de erro. A continuidade depende da preservação da NVS; não abrange apagamento/restauração nem energizações interrompidas antes da reserva. A inicialização deve preceder os consumidores, sem concorrência. Referências são entradas explícitas da reconstrução, sem histórico ilimitado em RAM; não há sincronização periódica nem compensação automática de deriva. Precisão física e acordos com os setores permanecem nas tarefas correspondentes. INT-003 chama reserva de boot e timer; a primeira etapa INT-006 recebe set_time por TT&C e usa/persiste referências no fluxo integrado.

Antes da primeira referência válida, conservar tempo relativo e indicar horário absoluto desconhecido. Não transportar o offset de um boot anterior como se o relógio tivesse continuado contando: o intervalo desligado é desconhecido. Uma referência posterior pode permitir reconstruir os eventos daquele novo boot. A validade temporal precisa chegar também à TM conforme contrato com TT&C, sem substituir desconhecido por UTC inventado.

A integração recebe `CMD_SET_TIME`/`set_time` pelo TT&C, valida `args.unix` em segundos Unix e acrescenta uma referência associada ao contador local em ms. A compensação aplicada é 0 ms no cabo e a estimativa de 162 ms no RF. O contrato TT&C usa segundos Unix em `ts`; converter unidades explicitamente, sem alterar esse contrato ou confundir a resolução dos registros internos com a do downlink. O fluxo relaciona `TC_FRAME.rx_ms` de 32 bits com o timer monotônico de 64 bits do mesmo boot, preservando o instante de recepção em vez do instante de processamento. O timestamp OBC dos snapshots é o da recepção validada, separado do horário de bancada produzido pelo Payload. Precisão física e persistência sob interrupção de alimentação continuam exigindo ensaios.

O SD conservará o histórico persistente da missão, TM, eventos, falhas, estatísticas e reinicializações. Em 05/11/2026, COM-005 implementou o núcleo POSIX `ObcStorage` e o adaptador `ObcStorageEsp32`: HSPI próprio nos pinos existentes, frequência inicial ajustável de 4 MHz, montagem em `/sd` e arquivos em `/sd/obc`. FAT32 previamente preparado; 64 GB exige também FAT32, pois o FatFs instalado não suporta exFAT. Não apagar, formatar ou truncar automaticamente. Nomes de arquivo são fornecidos pela integração e criados exclusivamente, sem sobrescrever registros anteriores.

No serviço de arquivos, cada bloco é escrito e sincronizado antes de retornar sucesso, com até três tentativas totais no mesmo último offset confirmado. O buffer do chamador permanece estável até o retorno; não há cache próprio, fila ou retenção após reset. Falha final bloqueia o escritor e preserva a cauda incerta. Para retomar, fechar/desativar, reinicializar e criar nome novo; `Init` idempotente não limpa a falha. `RESULT` informa status, bytes da última tentativa, errno e tentativas, sem confundir transferência com confirmação. O limite por arquivo é `INT32_MAX` bytes por causa dos offsets do runtime, não a capacidade do cartão.

O adaptador usa mutex estático sem espera (`BUSY` na disputa), somente em contexto de tarefa e fora do atendimento FOC. A primeira inicialização não pode ser concorrente; não acessar SD/HSPI por fora do adaptador. `Read` lê arquivos fechados por nome/offset em buffers do chamador para extração futura. INT-003 chama a montagem no boot; INT-006 liga UART, codecs e escrita. `request_log` permanece para a segunda etapa. Falhas de escrita/sincronização/fechamento são observáveis via APIs POSIX; `File::flush`, que retorna void, não é usado como comprovação.

`fsync` e repetição no arquivo lógico não garantem integridade física em queda de energia. Falha de montagem mantém indisponibilidade, sem reparo automático de FAT32. Em bancada, preservar cópia/imagem antes de reparo externo; usar o leitor existente em cópias de arquivos v1 para localizar registros íntegros. Confirmar módulo/alimentação com EPS e executar TES-006/TES-009 antes de afirmar comportamento físico. Políticas, evidências e limites estão em [COM-005](../.tasks/01-comunicacao/done/COM-005-armazenamento-sd.md).

## 6. Missão e supervisão

Inicializar automaticamente após energização, aplicar os estados de saída acordados, identificar falhas e iniciar no modo do ConOps. Considerar motor e Burn Wire durante boot/reset, inclusive os níveis impostos pelo circuito antes de o firmware executar.

O TT&C valida o envelope JSON; o OBC valida `args`, modo e condições de execução, encaminha a ação e responde com `SendAck()`. O TT&C deduplica os oito TCs mais recentes por 30 s, com reenvio do ACK anterior; isso não garante proteção depois de reset ou fora dessa janela. Definir persistência/condições das atuações críticas sem duplicar a deduplicação já existente. Comando catalogado não comprova ação implementada ou aprovada no ConOps. Umbilical usa o mesmo contrato JSON; ACK volta pelo meio de origem.

A TM atual usa `SendTelemetry()` com disponibilidade explícita. Status, energia e atitude serão completados conforme a integração dos respectivos produtores. O envio agregado previsto usa pacote `t = "ac"`, gerado por `BuildAircraft()` e enfileirado com `Send()`. Essa separação de conteúdo não substitui a demonstração de timestamp na TM prevista nos requisitos; preservar o critério e acordar sua evidência com os responsáveis. Não alterar entradas ou resultados esperados para acomodar o formato do enlace.

Integrar modos/transições registrados no README geral e no contrato TT&C, conferindo o ConOps vigente. Preservar LOW-POWER com rádio em RX para o TC de recuperação; beacon periódico é habilitado só em safe-mode e permanece desligado por padrão. Prioridades energéticas e Thermal Watchdog dependem de EPS/ConOps; proteção térmica é distinta do watchdog de tarefas.

Burn Wire exige autorização, duração, energia, persistência e confirmação física. Usar `Ttc::HoldTransmit(true, wait_ms)` e conferir ausência de TX antes do acionamento; retomar conforme política acordada. Interlock de rádio não autoriza a queima nem comprova liberação. Preservar os limites/ensaios dos setores; não concluir atuação por uma flag volátil.

Detectar perda de comunicação, falhas e travamentos, registrar eventos/reinicializações e recuperar periférico, tarefa ou OBC conforme limites acordados. Ausência de aeronaves ADS-B não significa perda de comunicação com o Payload. Watchdog não recupera falha física; não existe redundância física confirmada. Validar entradas, limites e falhas desde cada integração, seguindo os quatro grupos do checklist e as dependências de cada tarefa.

Reutilizar `GetLinkStats()`/`last_tc_ms` e a verificação de saúde/reset do SX1276 já implementada no TT&C. Camada de integração deve preservar os limites registrados por esse setor (entrega do TC ao OBC ≤200 ms e resposta ≤2 s), sem alterar testes ou declarar atendimento por inspeção documental. Enquadramento UART, proteção EPS/Térmico, atendimento FOC e persistência do OBC mantêm contratos/evidências próprios.
