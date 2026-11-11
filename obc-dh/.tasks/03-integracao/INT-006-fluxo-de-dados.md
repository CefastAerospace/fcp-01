# INT-006 — Fluxo Payload–tempo–SD–telemetria

**Estado:** Em andamento — bancada USB, cliente real da estação e TM/tempo aprovados nos recortes locais; aceites, operação e ensaios físicos restantes pendentes
**Responsáveis:** OBC-DH, Payload e TT&C

## Limites de capacidade e memória — 11/11/2026

Plano autorizado implementado em duas bancadas separadas, derivadas de
int006-ttc-check. Espressif32 fixado em 6.6.0 no consumidor OBC. Nenhum
roteiro, entrada ou expectativa anterior foi alterado; os geradores
históricos, limites do receptor, interfaces de produção e código TT&C
foram preservados. Não houve remoções. Hooks novos só são compilados
nas bancadas e observam o snapshot no contexto proprietário do receptor.

| Recorte | Resultado |
|---|---|
| Capacidade | 40 snapshots da mesma aeronave com altitude distinta, solo sem confirmar e primeira gravação DATA retida até o processamento completo. Exatamente 32 admissões, oito perdas e ocupação/pico 32; depois 32 confirmações FIFO, esvaziamento e aceitação/confirmação da 41ª alteração. Total final: 33 aceitas/confirmadas, oito perdas, ocupação zero, sem falhas de persistência ou evicções |
| Arquivos de capacidade | Somente essa bancada reserva 66 arquivos, suficientes para 33 pares DATA/CONFIRM; a barreira tem limite de 8 s e cede CPU. Primeiro DATA ID [42,1]/altitude 100 e último [42,33]/altitude 140 extraídos com CRC e conteúdo corretos. Boot 42 veio da NVS real; sessão FCPU 42 é metadado independente do gerador |
| Memória: referência e máximo de corpo | Referência pequena e JSON de 65.536 bytes com espaços válidos: READY, IDs 1 e 2, uma aeronave. Não houve perda de entrega por alteração apenas do id opaco |
| Memória: snapshot denso | Corpo válido de 63.181 bytes, 20 aeronaves, 32 extras por aeronave, identificação de 16 e tick de 40 caracteres: READY. O contrato prévio permitia READY ou NO_MEMORY nesse cenário |
| Memória: esgotamento e retomada | Vetor de 50.034 bytes com 10.000 null retornou NO_MEMORY. O snapshot anterior conservou ID 3, 20 aeronaves e CRC de toda a RAM 3899320051. Snapshot pequeno posterior: READY/ID 5. Foram realizadas 13 leituras bem-sucedidas de arquivo fechado durante os casos, além de ping/TM |
| Regressões | Dez builds aprovados. Receptor, entregas e downlink nativos passaram com C++11, -Wall -Wextra -Werror, ASan/UBSan sem desativar LeakSanitizer. ObcDownlinkCheck.py --bench original passou: sete entregas, retentativa e três arquivos extraídos |

Métricas dos relatórios USB, com ESP32-D0WD-V3 rev 3.1/CH340 em
/dev/ttyUSB0, somente USB e sem cartão, rádio ou Pi:

| Métrica | Capacidade | Memória |
|---|---:|---:|
| ACKs recebidos | 344 | 70 |
| Maior q | 82 ms | 92 ms |
| Maior resposta ACK | 171,185 ms | 795,705 ms |
| Maior intervalo do transporte | 2 ms | 2 ms |
| Menor pilha livre: processador/escritor/transporte | 3.120/5.512/1.332 bytes | 3.104/5.736/1.332 bytes |
| Mínimo histórico de heap interno por byte | 16.944 bytes | 1.040 bytes |

Todos ACK/lg ficaram dentro de 2 s, quadros dentro de 255 bytes e contador
n consecutivo. Houve pilhas positivas e transporte abaixo de 10 ms. Na
bancada de memória, o pico do parser observado foi 22.220 bytes, abaixo do
teto de 98.304. A rejeição pode ocorrer antes desse teto por falta de heap
ou bloco contíguo. O heap por byte terminou em 25.764 bytes nessa amostra;
o mínimo de 1.040 bytes evidencia margem baixa durante o esgotamento.
Não garante acomodação simultânea de todos os máximos nem prova ausência
de vazamento em ensaio prolongado. Não houve mudança de orçamento ou
tentativa de reduzir a pressão para obter aprovação.

Na memória, as confirmações permaneceram retidas: 21 entregas aceitas,
nenhuma perdida, e a primeira entrega transmitida. A bancada não promete
conservar essas pendências após reset, pois os arquivos são simulados em
RAM. O cenário de capacidade confirma e esvazia antes de testar retomada.

Evidências novas, preservando todos os relatórios anteriores:

- [Capacidade](../../examples/obc-limits-2026-11-11-capacity.json).
- [Memória](../../examples/obc-limits-2026-11-11-memory.json).
- [Regressão USB original](../../examples/obc-limits-2026-11-11-regression.json).
- [Firmware normal restaurado](../../examples/obc-limits-2026-11-11-obc.json).
- [Comandos e condições](../../tools/README.md#limites-de-capacidade-e-memória--11112026).

Artefatos FCPU gerados independentemente com JSON/struct/zlib:

- Capacidade: 41 quadros; SHA-256 `c1a34e81e3ff7bef4d9002cf3a947238cafbdd5bcdbaa37aaeb02b6567858c91`.
- Memória: cinco quadros, corpos 85/65536/63181/50034/85 bytes; SHA-256 `fb1c960310ca0f60f759c717898329d5c9d6491964337ea8f05f76e56801b2f3`.

Builds: Espressif32 6.6.0, Arduino ESP32 2.0.14, GCC Xtensa 8.4.0,
ArduinoJson 7.0.4 e LoRa 0.8.0; aviso preexistente uartSetPins no framework.
SHA-256 dos binários carregados, com hash de upload verificado:

- obc: `4dd7f005f13f8d11c536a6cb9b4be1e2ef0a2ae7747b9910453f7febf1004dcf`.
- int006-ttc-check: `98fd5746a160c5e314c870e827610c4fe50f3fcd3fd603cac67065c009cffa4a`.
- int006-capacity-check: `639744194ba5d53dceb3dc69e14497913edad5f94a57fa7b5c7b6f1be9e7a710`.
- int006-memory-check: `67afe7ea8745a969fd62b985dc4598db6c24ffd48e04678cb5f9fcef2919b5d2`.

Etapa local de limites concluída; placa restaurada a obc e verificada
sem periféricos. Não houve commit. INT-004/006 e TES-008/009 continuam
abertas pelos aceites e cenários completos existentes: cartão/Pi/RF,
persistência após reset, estação sob falha e condições operacionais.
Os limites simulados não aprovam taxa física ou perfil de missão.

## Estresse adicional sem periféricos — 11/11/2026

Etapa autorizada para ESP32/USB. Acrescentados `int006-stress-check` e
`ObcStressCheck.py`, com falhas somente no backend simulado da bancada.
Não foram alterados código TT&C, firmware de produção, bytes FCPU,
capacidade dos ambientes anteriores ou expectativas dos testes existentes.
Não houve substituição ou remoção de casos. A tarefa permanece aberta.

| Recorte | Resultado observado |
|---|---|
| Suíte TT&C existente na placa | 17/17 aprovados, incluindo fila comum cheia/ACK prioritário, falha local, reserva dos quatro eventos TX e confirmações repetidas. FakeTransport e Update manual; não é ensaio RF nem do agendamento integrado |
| Timeout e exclusividade de leitura | Backend retido por 1.500 ms; resposta busy no prazo local de 1.000 ms; pedidos concorrentes recusados inclusive após expiração; resultado tardio descartado; próxima leitura correta |
| Erros de leitura | BUSY e READ_FAILED injetados no backend; bytes parciais da falha não apareceram em lg |
| Confirmações e FIFO | ID desconhecido não avançou a fila; primeira entrega repetiu em 10.000,149 ms com ID/conteúdo preservados; confirmações duplicada e atrasada não liberaram a segunda; sete entregas confirmadas em ordem |
| Falha do diário | NO_SPACE injetado após oito arquivos FCD1; próxima persistência falhou, diário ficou inativo e falha foi contabilizada. USB, leitura e confirmações em RAM continuaram; sete confirmadas, nenhuma entrega perdida, fila final vazia |
| Firmware restaurado | Upload de obc com hash verificado e ObcDownlinkCheck.py sem --bench aprovado; no_sd esperado sem cartão |

O limiar de oito arquivos pertence somente à injeção de estresse: não é
capacidade física nem alteração da bancada normal de 32 arquivos. Os
sete DATA foram persistidos antes da primeira confirmação; após um CONFIRM
gravado, a falha impediu persistir os seis seguintes. Confirmar em RAM sob
falha não equivale a conservar essas confirmações após reset: retransmissões
posteriores continuam possíveis e exigem deduplicação durável no solo.

O ensaio USB retornou 59 ACKs, q máximo de 32 ms e maior resposta ACK de
1.060,223 ms durante o timeout. Todos ACK/lg ficaram dentro de 2 s, os
quadros dentro de 255 bytes e n consecutivo. Seis pings foram enviados
em rajada durante a leitura lenta. Transporte máximo de 2 ms; menor pilha
livre de 3.104/5.268/1.332 bytes para processador/escritor/transporte.
Ao final, o fluxo contabilizou 54 registros gravados, uma falha e 20
descartes; entregas contabilizaram seis falhas de persistência, oito
tentativas, sete confirmações e uma confirmação desconhecida.

Evidências preservadas, com relatórios novos:

- [Estresse USB: log e pacotes](../../examples/obc-stress-2026-11-11-bench.json).
- [Firmware normal restaurado](../../examples/obc-stress-2026-11-11-obc.json).
- [Resultado dos 17 testes TT&C na placa](../../examples/obc-ttc-2026-11-11-esp32.txt).
- [Procedimento reproduzível](../../tools/README.md#estresse-em-um-esp32-e-usb--11112026).

Builds de estresse e obc aprovados com Espressif32 6.6.0, Arduino ESP32
2.0.14, GCC Xtensa 8.4.0, ArduinoJson 7.0.4 e LoRa 0.8.0. A suíte própria
TT&C conservou sua resolução ArduinoJson 7.4.3. Aviso preexistente do
framework em uartSetPins. SHA-256 dos binários efetivamente carregados:

- `int006-stress-check`: `d370bb56493fe51facac02d6d7c5f9e53a3b166ffe770e1306ea423c0cf2f2b9`.
- `obc`: `d7dba72f428d9fb30171763c547432ad0ba491369cb201ebb356c1082ecc3a9e`, igual à etapa anterior.
- `test_ttc`: `99a0b694805a4347e5e78f8feb74dcea9743928e2bdf234074d215e972ef2fe8`.

Placa devolvida a **obc**, sem commit. Este recorte não comprova fila TT&C
cheia pela USB integrada; essa condição foi exercitada deterministicamente
na suíte com transportes falsos. Permanecem pendentes RF, cartão, Pi,
recuperação persistente após reset, falha/reinício da estação, convivência
com os demais setores e acordo final de tráfego/retenção/timeout.

## Cliente real da estação e TM/tempo — 11/11/2026

Etapa autorizada para um ESP32 e USB, sem periféricos. Foi acrescentado
`tests/ObcStationCheck.py`, sem alterar firmware, protocolos, dependências,
implementação TT&C ou entradas/expectativas dos roteiros anteriores.
O roteiro usa diretamente `Link`, `SessionLog`, `DeliveryStore`, `list_files`
e `download_file`, com log, estado e downloads em diretórios isolados OBC.
Não substitui o teste anterior de perda deliberada de `cf`.

| Recorte | Procedimento e resultado |
|---|---|
| Cliente real na bancada | Upload de `int006-ttc-check`; `ObcStationCheck.py /dev/ttyUSB0 --bench --report examples/obc-station-2026-11-11-bench.json --reader /tmp/fcp01-read-records`: aprovado. 75 TCs diretos, 84 ACKs no log incluindo extração, 48 TMs e oito itens listados; sete entregas confirmadas automaticamente sem erro |
| Downloads e leitura | As funções reais da estação baixaram três arquivos de 68, 36 e 68 bytes; tamanho, EOF, ausência de .part após sucesso e CRC válidos. O leitor C++ existente interpretou DATA, CONFIRM e DATA. Arquivo ausente produziu not_found |
| Firmware normal | Upload de `obc`; mesmo roteiro sem --bench, relatório `examples/obc-station-2026-11-11-obc.json`: aprovado. 64 TCs diretos, 65 ACKs incluindo listagem, 37 TMs e no_sd esperado |

Nos dois boots novos, a primeira TM tinha UTC desconhecido e `ac:null`.
Argumentos inválidos originais de `set_time` continuaram rejeitados. Época
Unix zero foi observada como número, e a referência atual manteve a
tolerância existente de um segundo. A bancada observou `ac=7` e depois
`ac=0`; `msg` e fontes não integradas continuaram null. Foram repetidos os
50 pedidos alternados ping/get_status/request_tm do roteiro temporal
histórico, com apenas uma TM pendente por vez.

Maior q em todos os ACKs: 17 ms na bancada e 10 ms no firmware normal.
Maior RTT informado pelo cliente para os TCs diretos: 83,4 ms e 49,9 ms.
Conferência posterior dos logs, correlacionando seq/rq com a primeira
transmissão do TC e usando pc_ms, verificou também ACK/lg do cliente de
arquivos: 93 respostas na bancada e 66 no normal, todas entre 0 e 2.000 ms.
Os maiores intervalos entre esses eventos foram 421 ms e 50 ms, com a
resolução de milissegundos do log do PC. Isso é distinto do RTT informado
pelo cliente e não constitui medição da precisão física do relógio OBC.
Transporte UART: máximo 2 ms nos dois recortes. Menor pilha livre na
bancada: processador 3.136 bytes, escritor 5.492 bytes e transporte 1.332
bytes. No firmware normal: processador 7.604 bytes e transporte 2.140 bytes;
escritor ausente é esperado sem cartão. Os contadores n foram consecutivos
nos pacotes recebidos e todos os quadros respeitaram 255 bytes.

Montagem e firmware são os mesmos da etapa de admissão abaixo: ESP32-D0WD-V3
revisão 3.1, CH340 em `/dev/ttyUSB0`, sem rádio, cartão ou Pi. Os hashes dos
binários permaneceram iguais e os uploads foram verificados pelo esptool.
`ReadRecords` foi compilado sem mudanças, com C++11, `-Wall -Wextra -Werror`
e UBSan; SHA-256 do executável nativo:
`f1d5a81b5f1b032dfc7d763fab7ee4009a13f7d57b72c9f67a7ca1b63b8cfcb1`.

Evidências, sem substituir relatórios anteriores:

- [Relatório cliente/bancada](../../examples/obc-station-2026-11-11-bench.json).
- [Relatório cliente/obc](../../examples/obc-station-2026-11-11-obc.json).
- Os diretórios de mesmo nome, sem a extensão .json, conservam o log da
  sessão, registro de IDs, arquivos baixados e interpretação JSON Lines.

Etapa local concluída; placa devolvida a **obc** e verificada. O ensaio
exercita o cliente real em operação normal, sem injetar falha de disco,
queda do PC ou reinício da estação. Portanto, não aprova as três pendências
externas de durabilidade/recuperação já encaminhadas. Também não comprova
cartão físico, RF, produtor Pi ou precisão temporal externa. INT-004,
INT-006 e os cenários completos da TES-008 continuam abertos.

## Correção da admissão e reexecução USB — 11/11/2026

O responsável aprovou preservar o contrato atual do receptor: 98.304 bytes
são o teto de consumo do parser por quadro, sem reserva permanente ou
garantia de acomodar todos os máximos simultaneamente. `NO_MEMORY`, NACK e
preservação do último snapshot válido continuam previstos e inalterados.

`ObcDataFlowEsp32` substitui a comparação de heap total com teto do parser
mais tarefas pela admissão em heap interno acessível por byte. O orçamento
de inicialização é 19.456 bytes das pilhas pendentes mais os mesmos 4.096
bytes de margem, total de 23.552 bytes. O maior bloco livre também precisa
comportar a maior pilha de 8.192 bytes. Alocação real dos blocos e criação
das tarefas continuam verificadas; insuficiência não fabrica disponibilidade.
Não foram modificados parser, buffers, modelos, simulador, protocolos,
timeouts, entradas ou resultados esperados dos roteiros.

O pico existente do parser é copiado pelo seu proprietário sob o lock do
fluxo. `[FLOW-MEMORY]` acrescenta heap total, heap/mínimo/maior bloco com
acesso por byte, pico e teto do parser. Não há leitura concorrente do receptor,
I/O dentro do lock ou log por byte recebido.

| Verificação | Procedimento e resultado |
|---|---|
| Receptor original | Recompilado C++11 com `-Wall -Wextra -Werror`, ASan/UBSan; `PayloadReceiverCheck.py` original passou, inclusive NO_MEMORY, limites e snapshot preservado após rejeição |
| Entregas/downlink | `ObcDeliveryCheck` com UBSan e `ObcDownlinkCheck` com ASan/UBSan passaram novamente, sem alteração dos testes |
| Builds | `obc`, `int006-check`, `int006-delivery-check` e `int006-ttc-check` aprovados com o mesmo ambiente registrado anteriormente |
| Bancada USB, roteiro original | `ObcDownlinkCheck.py /dev/ttyUSB0 --bench --report examples/obc-downlink-2026-11-11-bench-02.json`: aprovado integralmente; 46 ACKs, sete IDs, retentativa confirmada e três arquivos de 68 bytes extraídos com CRC válido; q máximo 32 ms e resposta máxima 486,945 ms |
| Firmware normal sem periféricos | Restaurado `obc`; `ObcDownlinkCheck.py /dev/ttyUSB0 --report examples/obc-downlink-2026-11-11-obc-02.json`: aprovado; seis ACKs, q máximo 0 ms, resposta máxima 30,069 ms e no_sd esperado |

Mesma montagem anterior: ESP32-D0WD-V3 revisão 3.1, CH340 em `/dev/ttyUSB0`,
somente USB. Uploads com hash de gravação conferido pelo esptool. A bancada
iniciou com `byte_heap=46356`, `byte_largest=16372`, `reserva=23552` e
`parser_limit=98304`; o escritor simulado iniciou e gravou 63 registros.
Foram contabilizados 18 descartes sob pressão, sem promessa de ausência de
perdas. A simulação conserva sua retenção e suas entradas originais.

Na bancada, transporte teve intervalo máximo de 2 ms; processamento, 21 ms.
Mínima pilha livre observada: processador 3.104 bytes, escritor 5.268 bytes
e transporte 1.332 bytes. Pico do parser: 7.190 bytes. O mínimo informado
pelo alocador para heap interno acessível por byte foi 14.192 bytes; o menor
maior bloco amostrado foi 8.180 bytes. São medições da amostra, não garantia
de aceitação do maior snapshot ou de todo tráfego. No firmware normal, o
intervalo máximo do transporte foi 2 ms e o mínimo de heap por byte, 90.600.

Binários desta correção, SHA-256:

- `obc`: `d7dba72f428d9fb30171763c547432ad0ba491369cb201ebb356c1082ecc3a9e`;
  RAM estática 31.896 bytes, flash 420.609 bytes.
- `int006-ttc-check`: `914369ec8724fc02d22de1326acaa5f94dd53365b57e7ac4b82ba73b8859d533`;
  RAM estática 34.840 bytes, flash 538.381 bytes.

Relatórios novos, preservando a reprovação anterior:

- [Bancada aprovada](../../examples/obc-downlink-2026-11-11-bench-02.json).
- [Firmware normal aprovado](../../examples/obc-downlink-2026-11-11-obc-02.json).

A etapa local de admissão de memória e reexecução está concluída. A placa
ficou com **obc**, restaurado e verificado. A INT-006 continua em andamento
pelos aceites, produtor Pi, tráfego/retenção/operação e ensaios de cartão,
RF, estação e resets físicos; esses cenários não são aprovados pela simulação.

## Execução USB em ESP32 isolado — 11/11/2026

Histórico da primeira execução, anterior à correção e reexecução acima.

Execução autorizada pelo responsável após conectar somente ESP32 e USB,
sem Pi, cartão ou rádio. Porta `/dev/ttyUSB0`, conversor CH340
`VID:PID=1A86:7523`, ESP32-D0WD-V3 revisão 3.1, cristal de 40 MHz.
Foram gravados os mesmos binários cujos SHA-256 estão abaixo; o esptool
confirmou integridade da gravação. Nenhum código ou teste foi alterado.

| Recorte | Entrada/procedimento e esperado | Observado | Resultado |
|---|---|---|---|
| Bancada `int006-ttc-check` | `ObcDownlinkCheck.py /dev/ttyUSB0 --bench --report examples/obc-downlink-2026-11-11-bench.json`; perda do primeiro cf, retentativa, listagem e extração FCD1 com CRC, conforme roteiro existente | 26 ACKs; q máximo 10 ms; resposta máxima 55,407 ms; quatro pacotes ac de três IDs, primeiro ID recebido duas vezes com conteúdo preservado. A listagem retornou lg/no_sd e o roteiro falhou antes da extração | Reprovado no conjunto; observações de entregas/TC são parciais |
| Firmware normal `obc`, sem periféricos | `ObcDownlinkCheck.py /dev/ttyUSB0 --report examples/obc-downlink-2026-11-11-obc.json`; comandos válidos/inválidos e no_sd na ausência do serviço SD | Roteiro passou; seis ACKs; q máximo 1 ms; resposta máxima 30,406 ms; no_sd observado | Aprovado somente nesse recorte |

**Diagnóstico da falha:** após reservar o bloco SD simulado, o firmware
registrou `heap=117744`, `byte_heap=46364` e `reserva=121856`.
A verificação existente recusou iniciar o escritor por uma diferença de
4.112 bytes no heap total considerado. O fluxo liberou os blocos e manteve
recepção/entregas em RAM, com SD indisponível. Portanto, `no_sd` não decorre
da falta de cartão físico: o armazenamento dessa bancada é simulado.
Falta corrigir o orçamento/uso de memória antes de repetir a extração,
sem reduzir limites do parser, entradas ou expectativas para obter aprovação.
A capacidade do heap acessível por byte também precisa integrar a análise;
heap total não representa memória intercambiável para todos os consumidores.

A bancada observou `poll_gap_ms` máximo de 2 ms e `process_gap_ms` de 19 ms.
A menor pilha livre observada foi 3.100 bytes no processador e 1.104 bytes
no transporte; `stack_sd=0` corresponde ao escritor que não iniciou.
Esses dados não aprovam o caso completo de memória/extração. No firmware
normal, `poll_gap_ms` máximo observado foi 1 ms.

Evidências preservadas, inclusive a falha e todas as linhas capturadas:

- [Relatório da bancada](../../examples/obc-downlink-2026-11-11-bench.json).
- [Relatório do firmware normal](../../examples/obc-downlink-2026-11-11-obc.json).

Ao terminar, a placa ficou com **obc**, restaurado e verificado. Não houve
tentativa de corrigir a falha ou alterar o critério de memória. RF, UART
entre placas, cartão físico, persistência da estação e cenário completo de
extração permanecem sem aprovação. A correção da bancada pertence ao OBC.

## Integração TT&C no OBC — 11/11/2026

Registro da implementação e verificações anteriores ao ensaio USB acima.

Entrega local sobre `f7af605`, ainda sem commit. O responsável OBC aceitou
ID `[boot,seq]`, `cf`, blocos de 90 bytes e paginação com `end`, além de
10 s provisórios para confirmação. Esse valor inclui a fila TT&C e não é
um limite operacional medido.

`ObcTtc::Service` envia por `SendAircraft`, consome eventos TX e `cf`, e
conserva o atendimento de TC. Recusa imediata finaliza a tentativa local
como falha. Confirmação antecipada/repetida e evento local atrasado usam
as operações já existentes do controle de entregas. Disponibilidade,
unidades e conteúdo original persistido são conservados; ICAO inválido e
altitude não representável no tipo TT&C recusam envio sem fabricar `null`.

`ObcDownlink` mapeia aeronaves, monta respostas usando o protocolo existente
e implementa a caixa fixa de um pedido. O escritor é o único proprietário
do I/O. `request_log` valida argumentos, lista e lê arquivos fechados,
responde erros e respeita destino explícito. Após 1 s local produz `busy`;
resultado tardio não pode alcançar um pedido novo. ACK `ok` depende de
enfileirar `lg`; fila recusada recebe `state_rejected`. Ausência do serviço
produz `no_sd`. Não foram alterados formatos persistidos, recuperação,
capacidade do cache, entradas ou resultados esperados de testes anteriores.

| Verificação local | Procedimento | Resultado |
|---|---|---|
| Mapeamento, caixa e extração | `ObcDownlinkCheck.cpp`, C++11, `-Wall -Wextra -Werror`, ASan/UBSan; comando em `tools/README.md` | Aprovado: campos null/zero, ID/conteúdo estável, ICAO fora de 24 bits, altitude não representável, prazos antes/durante/depois de I/O, resultado tardio, destino, paginação, arquivo ativo, blocos de arquivos reais, EOF, erros e limites 245/255 |
| Regressão de entregas | `ObcDeliveryCheck.cpp` original, recompilado com C++11, `-Wall -Wextra -Werror` e UBSan | Aprovado; cache/controle 2.848 bytes no nativo; preservados cenários de falha TX, confirmação antecipada/repetida, reset e saturação |
| Builds ESP32 | `pio run -d obc-dh -e obc -e int006-check -e int006-delivery-check -e int006-ttc-check` | Quatro aprovados; Espressif32 6.6.0, Arduino 2.0.14, GCC Xtensa 8.4.0, ArduinoJson 7.0.4 e LoRa 0.8.0 |
| Roteiro USB novo | Análise sintática de `tests/ObcDownlinkCheck.py` | Aprovada; execução em placa ainda não realizada |

`obc` usou 31.888 bytes de RAM estática e 420.249 bytes de flash.
`int006-ttc-check` usou 34.832 bytes de RAM estática e 538.021 bytes de flash.
São números de build, sem medição de heap ou pilha em operação. O aviso
preexistente do framework em `uartSetPins` permanece. ASan/UBSan executaram
fora do sandbox porque LeakSanitizer não opera sob ptrace.

SHA-256 dos binários compilados, sem upload:

- `obc`: `7aabbe896d0a6ed959ec05efd5624009733a693e491f6dc01c307793e61d9e31`.
- `int006-ttc-check`: `1b115fa4253fbf2f547ec5f1712fd49244c43ac9d29538e711e7eeba031ae729`.

A primeira compilação nativa recusou possível truncamento do ICAO. O
responsável autorizou explicitar a validação de 24 bits; a verificação
posterior acima passou sem reduzir avisos ou mudar critérios anteriores.

`ObcDownlinkCheck.py` é o roteiro do firmware atual. A nova bancada
`int006-ttc-check` usa TT&C/USB reais com Payload, SD e solo simulados.
As bancadas antigas conservam seu consumidor simulado, sem disputar as
entregas com o adaptador novo. `ObcTtcCheck.py` e `ObcDataFlowCheck.py`
permanecem para suas versões históricas. Não há porta de placa detectada
nesta execução; os testes físicos continuam em TES-008/TES-009.

Permanecem nesta tarefa os aceites do Payload, produtor Pi, origem/unidades,
tráfego/retenção e acordo operacional de timeout/congestionamento com TT&C.
A revisão da estação identificou três problemas externos de persistência e
continuidade da recepção, sem corrigi-los. Mensagens para os setores estão
em [encaminhamentos-2026-11-11.md](../../encaminhamentos-2026-11-11.md).
Esta entrega não encerra INT-004, INT-006 ou seus ensaios.

## Segunda etapa local — implementação de 10/11/2026

O plano autorizado acrescentou identificação e retenção de cada mudança da
projeção disponível por aeronave. `ObcDelivery` fornece cache de 32 entregas,
FIFO, uma entrega aguardando ACK, consumidores por cópia, eventos separados
de TX e confirmação, CRC FCD1 e recuperação conservadora de DATA/CONFIRM.
O escritor do fluxo grava e fecha segmentos antes de permitir descarte das
cópias duráveis. O controle continua disponível para TX em falha do SD.

`ObcStorage::List` e seu adaptador ESP32 acrescentam listagem lexical com
memória constante. `DeliveryLogs` lista/lê arquivos fechados e reconstrói
pendências. `ReadRecords` interpreta os novos registros em arquivos mistos.
O formato e os encaminhamentos TT&C, incluindo a proposta `request_log`,
estão em [obc-delivery.md](../../../common/protocol/obc-delivery.md). Os
comandos reproduzíveis estão em [tools/README.md](../../tools/README.md).

Resultados locais desta implementação.

- `ObcDeliveryCheck` passou com C++11, `-Wall -Wextra -Werror`, ASan e UBSan.
  O LeakSanitizer exigiu execução fora do sandbox que usa ptrace. Cache e
  controle ocuparam 2.848 bytes no build nativo.
- `DeliveryLogsCheck.py` passou com vetores independentes struct/zlib,
  arquivos temporários reais, leitura por blocos, EOF, arquivo ausente,
  disponibilidade, corrupção, caudas, confirmações órfãs e 40 pendências.
- Passaram as verificações existentes de armazenamento, fluxo, codec v1,
  snapshots v2, TM v3, snapshot partido e leitor misto, com UBSan.
- `pio run -e obc -e int006-check -e int006-delivery-check` passou nos três
  ambientes. Após o ajuste de reserva, `obc` usou 31.376 bytes de RAM estática
  e 408.641 bytes de flash.
  O framework Arduino 2.0.14 emitiu seu aviso em `uartSetPins`.
- A nova bancada simula TX, perda de ACK, retentativas, escritor de 100 ms,
  respostas de arquivos fechados e falta de espaço após 32 registros.
  `ObcDeliveryBenchCheck.py` está preparado. Sua execução, medição de heap,
  pilhas e cadência UART estão pendentes por ausência de porta ESP32.

Escolhas locais a avaliar no ensaio incluem fechamento a cada DATA/CONFIRM,
com aumento do número de arquivos, e cruzamento de registros por múltiplas
passagens com memória constante. O pior custo da busca cresce quadraticamente
por consulta. Saturação volátil gera perda explícita. Depois de reset, a
primeira observação nova estabelece novamente a base de comparação em RAM.

### Diagnóstico da revisão final — ajuste autorizado e verificado

O caso reproduzido antes da correção combinava consulta inicial pendente,
recepção ocupando as 32 posições com entregas voláteis e falha de escrita.
Uma pendência antiga legível ficava sem espaço em `Queue::Restore`, mantendo
a barreira de recuperação ativa e impedindo TX.

O responsável autorizou reservar uma das 32 posições enquanto houver
consulta pendente. `Queue::Reserve` aplica esse limite à admissão e permite
que `Restore` use a posição reservada. A substituição de cópias duráveis
também conserva essa reserva quando inicia uma nova consulta. A saturação
continua contabilizada e a conclusão da consulta libera as 32 posições.

`ObcDeliveryCheck` acrescentou a combinação de rajada, falha SD e pendências
antigas, verificando transmissão FIFO, ID/instante preservados e avanço após
falha persistindo CONFIRM. Também verifica admissão acima de 32 alterações
com substituição de cópias duráveis e espaço reservado à recuperação.
Passou novamente com ASan/UBSan. `DeliveryLogsCheck.py` e as regressões do
leitor v1/v2/v3, snapshots e snapshot partido passaram com os executáveis
recompilados usando UBSan. Os três builds ESP32 passaram novamente.

A INT-006 permanece em andamento. A ligação das APIs de entrega ao rádio,
ACK e deduplicação na estação, contrato final de `request_log`, execução da
nova bancada e aceites Payload/TT&C continuam pendentes. Os critérios e
roteiros anteriores conservam suas entradas e resultados esperados.

## Encaminhamento anterior à integração TT&C — 11/11/2026

As implementações locais da segunda etapa estão descritas acima. O setor de
testes pode executar os roteiros de bancada e registrar os recortes na
TES-008. A disponibilidade de ESP32 foi confirmada na execução da COM-003.
O roteiro `ObcDeliveryBenchCheck.py` da bancada de entregas continua com
execução pendente, conforme encaminhamento à TES-008.

| Interface | Disponível | Pendência de integração |
|---|---|---|
| Payload–OBC | Receptor FCPU com corpo JSON e validação local | Aceite do enquadramento com Payload, produtor UART no Pi, unidades e perfil de tráfego |
| Controle de entregas | ID estável, cache, DATA/CONFIRM e recuperação local | Ligação das APIs de entrega à biblioteca e à estação TT&C |
| Arquivos fechados | Listagem, leitura por blocos, leitor FCP1/FCP2/FCP3/FCD1 e reconstrução local de pendências | Contrato final e implementação de `request_log` com respostas USB |

O despacho atual aceita `ping`, `get_status`, `request_tm` e `set_time`.
`request_log` permanece com resposta `invalid_cmd`. O pacote com ID estável,
ACK de dados pelo solo, deduplicação na estação e eventos de TX precisam das
adaptações descritas no [contrato de entregas](../../../common/protocol/obc-delivery.md).

## Histórico da preparação da segunda etapa — 10/11/2026

A primeira etapa abaixo já está implementada e verificada nos recortes registrados. Com computador/ESP32/USB, priorizar os contratos e o desenvolvimento local da segunda etapa, conservando as predecessoras e os aceites pendentes.

- [ ] **Com Payload/TT&C:** definir quais campos podem compor aeronaves agregadas, unidades, idade/validade, seleção/frequência e comportamento de fila cheia/serialização recusada, respeitando o limite do TT&C e sem converter `id` em contagem ADS-B.
- [ ] **Extração `request_log`:** acordar argumentos, seleção de arquivo fechado, respostas em blocos, término/erro, controle de avanço e ferramenta de solo. A API `Read` já existe; o contrato do comando não está definido apenas por seu nome no catálogo. Não inventar fragmentação RF ou resposta aceita por outro setor.
- [ ] **Após o acordo:** reutilizar modelos, `BuildAircraft`/`Send`, armazenamento e leitor existentes; verificar a lógica local antes dos ensaios físicos. Nenhuma implementação da segunda etapa é declarada por esta revisão.
- [ ] **Ponta Pi:** obter responsável, porta/configuração, produtor de snapshots, aceite do enquadramento e perfil nominal/rajadas/backlog. Snapshots JSON são o conteúdo confirmado; FCPU também transmite cabeçalho e controles binários pelo cabo, sujeitos ao aceite externo.
- [ ] **Critério por mensagem:** manter a pendência de cada atualização ADS-B intermediária com Payload/coordenação. O snapshot acumulado ou seu `id` não prova conservação individual.

A representação interna do SD permanece binária versionada, conforme a escolha reafirmada pelo responsável em 10/11. Isso não define os bytes aceitos pelo Payload ou pela estação. Relatórios em `/tmp` citados nas evidências são saídas locais regeneráveis; para encaminhar novos ensaios, preservar os arquivos efetivamente produzidos junto da versão/procedimento.

## Objetivo

Conectar recepção, referência temporal, registro e envio dos dados da missão.

## Entrega

Associar tempo aos dados/eventos, gravar missão/TM/estatísticas/falhas e persistir boots/referências. Tratar set_time validando args.unix em segundos Unix e correlacionando ao contador local em ms. Usar SendTelemetry() para estado e BuildAircraft()/Send() para aeronaves agregadas. Tratar falhas de serialização, filas e SD sem perdas silenciosas; implementar a extração pelo umbilical conforme contrato.

## Pré-requisitos

- **Tarefas:** [COM-004](../01-comunicacao/done/COM-004-uart-payload.md), [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md), [DAD-003](../02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md), [INT-004](INT-004-ttc-e-umbilical.md)
- **Condições externas:** Acordar seleção/frequência de downlink e validade temporal com Payload e TT&C.

## Definições e condições

- TM via `SendTelemetry()` e aeronaves agregadas via `BuildAircraft()`/`Send()`. Corpo JSON ≤245 bytes sem `n`, quadro final ≤255 bytes, sem fragmentação e CRC de hardware LoRa. ACK prioritário e TX sob demanda já estão definidos; acordar frequência/seleção e tratar falhas de geração/enfileiramento. Não substituir demonstração de timestamp na TM por outro teste; preservar o critério e discutir evidência correspondente.

## Critérios de conclusão

Fluxo central e tratamento de erros implementados, contrato temporal preservado e verificações locais registradas. Demonstrar ADS-B simulado com timestamp na TM e extração reproduzível em [TES-008](../04-testes/TES-008-integracao.md).

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [COM-004](../01-comunicacao/done/COM-004-uart-payload.md) | Acordar com Payload o FCPU v1, ponto focal/ponta Pi, porta/configuração, taxa nominal, rajadas e retenção; acompanhar o produtor e preservar a pendência de cada atualização ADS-B e a semântica dos campos. | Pendente |
| [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Ligar registros ao SD, nomes/rotação por boot e limite de arquivo, contabilização de perdas, encaminhamento de erros e extração pelo umbilical. | Fluxo e extração implementados/verificados no computador; USB/cartão físicos pendentes |
| [DAD-001](../02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md) | Consolidar o aceite de Payload/TT&C e das estatísticas: id opaco, idade/validade dos campos, unidades de velocidades, extras e migração explícita dos dados legados, sem inventar sessão/sequência ou validade. | Pendente |
| [DAD-003](../02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) | Integrar criação, gravação e extração de registros e referências temporais por boot, preservando formatos v1/v2 e leitura retrocompatível. | Criação/gravação/referências ligadas, leitura preservada e extração implementada; ensaio integrado pendente |
| [TES-004](../04-testes/done/TES-004-uart-software.md) | Consolidar o contrato com o produtor; qualquer adaptação acordada exige regressão explícita, sem alterar resultados esperados para acomodar falhas. | Pendente |

## Evidências

### Primeira etapa — decisões do responsável

Implementar recepção, tempo, registros e TM primeiro; manter envio agregado
de aeronaves e extração para a segunda etapa. Acrescentar formato versionado
para TM com contagens indisponíveis. Continuar recepção/TM em falha ou
ocupação do SD, com perdas explícitas e sem retenção ilimitada. Preservar
`ObcTtcCheck.py` para a versão INT-004 e criar roteiro próprio INT-006,
pois `set_time` deixa de ser comando rejeitado. Nenhum critério original
desta tarefa ou resultado esperado de teste anterior foi alterado.

### Implementação local — 09/11/2026

- `ObcDataFlow` fornece estado portátil, fila de oito registros de até
  105 bytes e diário exclusivo por boot/segmento. O adaptador ESP32 liga
  o receptor e o SD existentes, sem alterar o coordenador portátil do boot.
- UART2 usa tarefa Core 1/prioridade 3/pilha 3 KiB e ring de 1 KiB;
  processamento usa prioridade 2/pilha 8 KiB; escritor usa prioridade 1/
  pilha 8 KiB. TT&C conserva Core 0/prioridade 10, filas e ACK por origem.
  O transporte e o processador possuem métricas distintas de cadência.
- O bloco de snapshot usa duas reservas de 32768 + 31729 bytes. A nova
  API do codec mantém exatamente o registro v2, com fonte/blocos estáveis.
  Gravação parcial é diagnosticada e não confirma um registro completo.
- O diário registra BOOT/motivo de reset, referências, snapshots v2, TM
  v3, estatísticas e eventos/falhas. Cria arquivos exclusivamente, rotaciona
  entre registros e desativa o escritor em falha terminal. Não há remontagem
  ou recuperação automática. SD ausente não impede o restante do fluxo.
- `set_time` valida u32, inclusive época zero, expande `rx_ms` no mesmo
  boot e aplica UTC em RAM; persistência é separada. Compensações conservadas:
  cabo 0 ms, RF 162 ms, sem afirmar precisão física. TM publica timestamp
  reconstruído e `ac` do snapshot válido, conservando `msg:null` e os demais
  campos ausentes. A mesma amostra de enlace enfileirada é usada no registro.
- TM v3 (`FCP3`, versão 3/tipo 9, 99 bytes) acrescenta disponibilidade dos
  cinco contadores. Formatos v1/v2 e seus validadores permanecem preservados;
  o leitor aceita arquivos mistos. Não há migração automática dos arquivos.
- Os contadores antigos do receptor conservam seu significado. Observações
  aditivas permitem registrar candidatos completos e posições ausentes
  distintas, sem confundir estas últimas com ocorrências de gap.

Os ajustes de nome `EPS_SUBSYSTEM` (valor 5 preservado), inicialização do
evento, linhas de log atômicas, separação UART/parser e reservas partidas
foram discutidos e autorizados após os diagnósticos. O dimensionamento final
de pilha usa as medições, sem reduzir limites de quadro ou mudar critérios.

### Procedimentos e resultados

Ambiente: ESP32-D0WD-V3 revisão 3.1, USB `/dev/ttyUSB0`, Espressif32 6.6.0,
Arduino ESP32 2.0.14/GCC Xtensa 8.4.0, ArduinoJson 7.0.4 com patch controlado
existente e LoRa 0.8.0. Os testes nativos PIO do TT&C conservaram sua resolução
existente ArduinoJson 7.4.3; a suíte do fluxo usa o 7.0.4 do consumidor.

| Verificação | Procedimento | Resultado |
|---|---|---|
| Regressões e fluxo nativo | 12 grupos, recompilados com C++11, `-Wall -Wextra -Werror`, ASan e UBSan; comandos base em `tools/README.md` e nos testes | Aprovados: modelos, tempo, boot, armazenamento, v1, snapshots, receptor, leitor, v3, diário/filas e buffer partido |
| Compatibilidade de arquivo | Vetores independentes `struct`/`zlib`, arquivos mistos e falhas/truncamentos | Bytes antigos preservados; snapshot máximo 64497 bytes; cauda parcial não publicada |
| TT&C | `pio test -e native -e esp32 --upload-port /dev/ttyUSB0 --test-port /dev/ttyUSB0` em `tt-c/` | 14 testes nativos + 12 no ESP aprovados; saída opcional da TM preservada em falha |
| Builds | `pio run -e obc -e uart-loopback -e uart-receiver-check`; `pio run -e int006-check` em `obc-dh/` | Quatro ambientes aprovados; bancadas UART e suas dependências isoladas |
| Bancada integrada | Upload `int006-check`; `tests/ObcDataFlowCheck.py /dev/ttyUSB0 --bench` com Python/pyserial | 77 cenários aprovados; `q` máximo 18 ms; resposta máxima 64,785 ms |
| Firmware normal sem SD/Pi | Upload `obc`; `tests/ObcDataFlowCheck.py /dev/ttyUSB0` com Python/pyserial | 75 cenários aprovados; `q` máximo 11 ms; resposta máxima 61,985 ms |

A bancada usa os mesmos bytes FCPU independentes através do ring e do
receptor reais. A fonte em flash e o backend de SD lento são exclusivos
desse ambiente. Escritas aguardam 100 ms e verificam estabilidade dos bytes.
Houve 24 candidatos, um CRC inválido e uma repetição; 53 registros confirmados
pelo backend simulado e 25 descartes contabilizados sob pressão. `uart_full=604`
conta observações de ring cheio; a fonte simulada retém bytes, não representando
taxa física ou promessa de ausência de perdas em missão.

O maior intervalo do transporte foi 1 ms, enquanto o consumidor chegou a
21 ms, sem redefinir o limite de 10 ms da UART. Pilha mínima livre observada:
processador 3156 bytes, escritor simulado 6144 bytes e transporte 1332 bytes.
As reservas partidas foram concluídas; o parser mantém teto de 98304 bytes,
com falta de memória explícita. Heap total inclui regiões com capacidades
distintas: as medições de RAM por byte aparecem nos logs. A amostra não
garante toda combinação de tráfego, parse e driver SD físico.

Sem cartão/Pi, o firmware normal manteve USB e UTC em RAM: transporte máximo
1 ms, SD indisponível/status 11, nenhum registro confirmado e descartes
explícitos. A placa foi devolvida ao firmware **obc** após os ensaios.

Relatórios locais temporários, regeneráveis pelos roteiros:
`/tmp/fcp01-int006-native.log`, `/tmp/fcp01-int006-bench.json` e
`/tmp/fcp01-int006-obc.json`. Entradas e resultados anteriores permanecem
nos testes originais; o roteiro INT-004 não foi usado para declarar o novo
comportamento de `set_time` aprovado.

Binários verificados (SHA-256):

- `obc`: `ab3fedeb049dcae244dedf63b58ba074d5c6cc822a9c4b318eebdff8e95f8c10`.
- `int006-check`: `a0963b7a9f3634c8c12d5d04e520b0ff2e93f39d3785c64ea40970e8f355f7c5`.

### Pendências que mantêm a tarefa aberta

- A integração OBC de entregas e `request_log`, a bancada USB, o cliente
  real e os limites simulados foram verificados conforme os registros
  de 11/11 acima. Falta o acordo operacional de tráfego/timeout/retenção.
- Aceites Payload/TT&C e ponta Pi: FCPU, tráfego, rajadas, retenção, unidades,
  idade dos campos, estatísticas e preservação de cada atualização ADS-B.
- Ensaios físicos com Pi/cartão, recuperação após interrupção e comprovação
  integrada em TES-005/TES-006/TES-008/TES-009. A bancada simulada não os encerra.
- Boot de voo, sensores, modos/atuações e recuperação continuam em suas
  tarefas próprias. Manter esta tarefa fora de `done/`: a entrega é parcial.

## Referências

- Requisitos relacionados: REQ-OBC-003, REQ-OBC-005, REQ-OBC-006, REQ-OBC-009, REQ-OBC-010, REQ-NF-OBC-002, REQ-NF-OBC-003. Implementação não equivale a requisito validado.
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-DADOS.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
