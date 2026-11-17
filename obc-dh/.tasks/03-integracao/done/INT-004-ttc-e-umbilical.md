# INT-004 — Integração de TT&C e umbilical

**Estado:** Concluída — entrega técnica verificada e aceite TT&C informado pelo responsável OBC-DH em 17/11/2026
**Responsáveis:** OBC-DH e TT&C

## Conclusão e aceite TT&C — 17/11/2026

O responsável OBC-DH informou nesta sessão: **“Sobre as definições do
INT-004, TT&C aceitou. Pode marcar na documentação e mover para concluido”**.
Essa comunicação registra o aceite das definições e da matriz do contrato
consumido abaixo e autoriza o encerramento da INT-004. O nome do
representante TT&C e uma referência externa não foram fornecidos; não
foram presumidos. A referência do aceite é a declaração do responsável
OBC-DH nesta sessão em 17/11/2026. Ressalvas adicionais não foram informadas.

Adaptador central, TC/ACK, TC/TM, dependências fixadas e regressões locais
já atendem aos critérios registrados. Com o aceite informado, o último
impedimento desta INT foi resolvido. A task é movida para `done/`, com
atualização do painel e dos links, mantendo os históricos e evidências.

O aceite não encerra TES-008 ou outros ensaios, não aprova requisito de
missão e não resolve contratos operacionais próprios da INT-006.
Mapeamentos físicos de sensores, modos e atuações permanecem nas suas
integrações. O diagnóstico de core dump inválido continua preservado,
sem reparo ou nova garantia sobre sua origem histórica.

## Consolidação da entrega técnica — 17/11/2026

O plano autorizado adota o contrato TT&C existente, sem criar protocolo,
API ou comportamento novo. O adaptador central já implementa a integração;
esta entrega consolida suas interfaces, reexecuta as verificações atuais e
corrige descrições desatualizadas na documentação TT&C. Código, dependências,
testes, entradas e resultados esperados permanecem intactos.

Contrato consumido: [ttc-json.md](../../../../common/protocol/ttc-json.md).
Referências executáveis: [TtcProtocol.h](../../../../tt-c/include/TtcProtocol.h),
[TtcProtocol.cpp](../../../../tt-c/src/TtcProtocol.cpp),
[Ttc.cpp](../../../../tt-c/src/Ttc.cpp) e
[ObcTtc.cpp](../../../src/ObcTtc.cpp). A matriz abaixo identifica o contrato;
não constitui outro formato nem substitui o documento compartilhado.

### Matriz da interface consumida

| Interface | Contrato adotado | Responsabilidade e limite |
|---|---|---|
| Enquadramento | Um objeto JSON por quadro RF ou linha umbilical; corpo até 245 bytes, quadro até 255 bytes; sem fragmentação | TT&C transporta e insere `n`; OBC usa os geradores existentes e trata recusa de enfileiramento |
| TC | `t:"tc"`, `seq` u16, comando do catálogo e `args` objeto opcional; parser também aceita `null` | TT&C valida envelope; OBC valida conteúdo, recurso, estado e efeitos |
| ACK/NACK | `ok`, `bad_json`, `invalid_cmd`, `invalid_param`, `state_rejected`, `unknown_cmd`; ACK prioritário pelo meio de origem | OBC responde com `SendAck`; `ok` não comprova recepção no solo ou persistência |
| Deduplicação | Oito TCs por 30 s; repetição respondida recebe o ACK anterior sem reexecutar a ação | Reutilizar TT&C; não garante persistência de atuações após reset |
| Comandos atuais | `ping`, `get_status`, `request_tm`, `set_time`, `request_log`, `reset_obc` | Comandos básicos nesta INT; evoluções de tempo/arquivos na INT-006 e reset na INT-012; demais comandos mantêm a rejeição atual |
| TM sob demanda | `get_status`/`request_tm` enfileiram uma TM antes do ACK `ok`; recusa produz `state_rejected` | `ObcTtc` compõe dados; TT&C completa os campos de enlace; não há nova TM periódica |
| Disponibilidade | Chaves conservadas com `null`; zero é valor válido; `TM_TEMPS` representa as cinco temperaturas em conjunto | Publicar somente fontes disponíveis; não fabricar leituras ou definir disponibilidade individual de temperaturas nesta entrega |
| Tempo/modo | `args.unix` e `ts` em segundos Unix u32, inclusive zero; sem referência, `ts:null`; modo atual `bench` | OBC fornece referência/estado; ConOps e modos de voo permanecem na INT-007 |
| Energia/temperatura | `vb`/`vs` em V, `ib`/`is` em mA, `tp` com cinco temperaturas em °C | Transporte definido; fontes, posições térmicas e integração operacional permanecem na INT-009; atualmente `null` |
| Atitude/missão | `w` em °/s, `az` em graus, `ac` como quantidade do último snapshot válido; `msg` como contagem ADS-B | ADCS na INT-005; Payload na INT-006; lista vazia permite `ac:0`, ausência de snapshot produz `null`; `msg` não é inferido de `id` |
| Enlace | `rssi` em dBm, `snr` em dB, `rx`/`err`/`rst` como contadores TT&C; RSSI/SNR indisponíveis antes de TC RF | `SendTelemetry` usa estatísticas TT&C; `rst` conta tentativas existentes de reinicialização do rádio e não comprova saúde |
| Execução | Tarefa TT&C Core 0/prioridade 10, proprietário exclusivo do rádio; ISR apenas notifica; SD usa HSPI próprio | Preservar `Start`, sem `Update` concorrente; não duplicar filas ou drivers |
| Entregas/extração | `[boot,seq]`, `cf`, eventos TX e `request_log` com blocos de até 90 bytes, paginação `end` e offset derivado | Formatos já consumidos; operação pertence à INT-006; timeout de confirmação de 10 s continua provisório |
| Prazos | Entrega do TC ao OBC ≤200 ms; resposta ≤2 s | Preservados nas regressões USB; comprovação integrada RF e demais condições permanece na TES-008 |

### Registro de aceite

| Parte | Situação | Referência, responsável e data |
|---|---|---|
| OBC-DH | Adoção técnica do contrato existente e consolidação autorizadas | Responsável OBC-DH nesta sessão, 17/11/2026; aceite OBC de entregas/extração de 11/11 preservado abaixo |
| TT&C | **Aceito** | Aceite informado pelo responsável OBC-DH nesta sessão em 17/11/2026, com autorização para concluir a INT-004; nome do representante TT&C e referência externa não fornecidos |

O aceite informado se refere às interfaces, unidades, disponibilidade e
divisão de responsabilidades desta matriz, associada à revisão/manifesto
da verificação técnica abaixo. As pendências com tarefas destinatárias
permanecem nelas. A concordância foi comunicada pelo responsável OBC-DH;
não foi inferida da existência da biblioteca ou dos resultados USB.
Nenhuma mensagem externa foi enviada pelo agente.

### Verificações desta execução

Base Git `ca961edf6a1b684b967bb9c794e9e462005cb624`, checkout inicialmente
limpo. ESP32-D0WD-V3 rev. 3.1, CH340 em `/dev/ttyUSB0`; rádio/SD indisponíveis
e nenhum produtor Pi conectado. Bancada com Payload/SD simulados, firmware
e roteiros originais. A placa terminou com `obc` restaurado e verificado.

| Verificação | Resultado e evidência |
|---|---|
| Contrato TT&C nativo | 25/25 casos aprovados, ArduinoJson 7.4.3 resolvido pelo ambiente nativo; [saída](../../../examples/int004-2026-11-17-141352/02-ttc-native.txt) |
| Consumidor/downlink nativo | C++11, `-Wall -Wextra -Werror`, ASan/UBSan com LeakSanitizer ativo e ArduinoJson 7.0.4 do consumidor; [aprovado](../../../examples/int004-2026-11-17-141352/04-downlink-native.txt) |
| Builds | `obc` e `int006-ttc-check` aprovados; [saída](../../../examples/int004-2026-11-17-141352/05-firmware-builds.txt). Somente o aviso externo conhecido de `uartSetPins` no framework |
| Entregas/extração USB | `ObcDownlinkCheck.py --bench`: 44 ACKs, `q` máximo 54 ms, resposta máxima 369,253 ms; três arquivos com CRC válido; [relatório](../../../examples/int004-2026-11-17-141352/08-bench-downlink.json) |
| Cliente real na bancada | `ObcStationCheck.py --bench`: 75 TCs diretos, 48 TMs, sete entregas confirmadas e três arquivos extraídos/interpretados; `q` máximo dos TCs diretos 68 ms e RTT máximo 171,5 ms; [relatório](../../../examples/int004-2026-11-17-141352/14-bench-station.json) |
| `obc` restaurado, roteiro de entregas | Seis ACKs, `q` máximo 10 ms e resposta máxima 65,902 ms; `no_sd` esperado; [relatório](../../../examples/int004-2026-11-17-141352/11-obc-downlink.json) |
| `obc` restaurado, cliente real | 64 TCs diretos, 37 TMs, `q` máximo 116 ms e RTT máximo 156,3 ms; `no_sd` esperado; [relatório](../../../examples/int004-2026-11-17-141352/16-obc-station.json) |

Comandos exatos, versões, hashes de fontes/testes/firmwares e inventário dos
artefatos ficam no [manifesto](../../../examples/int004-2026-11-17-141352/manifest.json).
Os logs, estado e downloads do cliente real estão em diretórios isolados
junto dos relatórios. Cada roteiro iniciou após boot novo; os hashes dos
uploads foram verificados pelo esptool. Procedimentos reproduzíveis no
[guia](../../../tools/README.md#integração-ttc-e-request_log--11112026).
Relatórios anteriores não foram sobrescritos; os roteiros históricos
`ObcTtcCheck.py`/`ObcDataFlowCheck.py` não foram aplicados ao firmware atual.

A primeira tentativa nativa foi impedida pelo sandbox ao criar lock/cache
do PlatformIO; os casos não chegaram a executar nessa tentativa. O
[log](../../../examples/int004-2026-11-17-141352/01-ttc-native-sandbox.txt)
foi preservado; a execução autorizada fora do sandbox aprovou os 25 casos.

### Diagnóstico de core dump e limites

O boot da bancada exibiu `esp_core_dump_flash` com checksum calculado
`1b646d2e` e gravado `ffffffff`. A etapa seguinte foi suspensa, `obc` foi
restaurado/verificado e o responsável solicitou investigação antes de
continuar. O mesmo diagnóstico aparece no boot normal.

A leitura de 64 KiB em `0x3f0000`, partição `coredump` da tabela carregada,
encontrou imagem declarada de 11.460 bytes, assinatura ELF no offset 24 e
checksum final `ffffffff`. O CRC-32 recalculado dos 11.456 bytes anteriores
é exatamente `1b646d2e`. O SDK instalado habilita verificação de core dump
no boot. Isso identifica a imagem persistida inválida/incompleta como causa
do aviso; não determina quando ou por que ficou incompleta e não comprova
um novo crash nesta execução. [Análise](../../../examples/int004-2026-11-17-141352/coredump-analysis.json),
[imagem preservada](../../../examples/int004-2026-11-17-141352/coredump.bin) e
[leitura](../../../examples/int004-2026-11-17-141352/12-coredump-read.txt).

Após o diagnóstico, os roteiros originais foram retomados sem reparo,
apagamento da partição ou mudança de expectativas. A imagem inválida
permanece na placa; sua origem histórica não foi determinada. Nenhum
apagamento integral, ajuste de partição, atualização do framework ou
correção de firmware foi realizado. A passagem dos roteiros não elimina
essa ressalva.

### Estado após verificação técnica, antes do aceite

- Entrega técnica documentada e verificações locais aprovadas; INT-004
  permanece **Em andamento**, fora de `done/`, pelo aceite TT&C pendente.
- Corrigidas as afirmações atuais de `main.cpp` vazio e `request_log`
  ausente na documentação TT&C; históricos e checkboxes de ensaio preservados.
- INT-006 conserva contratos operacionais, timeout definitivo e pendências
  do fluxo/estação; INT-005/009 conservam as fontes ADCS/EPS/Térmico;
  INT-007 conserva modos e INT-012 conserva o mecanismo de reset.
- TES-008 conserva RF, tempos integrados e convivência; nenhuma TES,
  requisito ou aceite externo foi aprovado por esta execução.
- Após receber a concordância TT&C, registrar responsável/data/referência,
  conferir os critérios e só então mover esta task, atualizar links e painel.


## Separação vigente — 14/11/2026

Os relatos datados a seguir conservam os estados e pendências da época.
O aceite e o encerramento de 17/11/2026 estão registrados no início desta
task e prevalecem sobre as menções históricas de aceite pendente.

Entrega desta INT: adaptador OBC–TT&C, comandos básicos, build/regressões pertinentes e contrato consumido identificado. O aceite conjunto das interfaces/unidades permanece engenharia OBC/TT&C; não é obrigação transferida ao setor de testes. Comprovação RF e dos tempos integrados continua na TES-008, sem bloquear o encerramento da implementação por si só. A capacidade de boot de bancada da INT-003 já é consumida; sua parte elétrica de voo não bloqueia o adaptador de bancada.

As lacunas de regressão do consumidor atual ficam na [TES-012](../../04-testes/TES-012-consumidor-telecomandos.md). `CMD_RESET_OBC` pertence à [INT-012](INT-012-watchdog-reset-obc.md). O aceite conjunto ainda está pendente; estado Em andamento preservado.

## Fixação do ambiente consumidor — 11/11/2026

Implementada a fixação aprovada: `platform = espressif32@6.6.0` em obc,
herdada pelos ambientes derivados. É a versão já observada nas evidências,
com Arduino ESP32 2.0.14 e GCC Xtensa 8.4.0. ArduinoJson 7.0.4, LoRa 0.8.0,
o checkout local TT&C e os contratos permanecem iguais. Nenhum arquivo de
tt-c/ foi alterado. Essa fixação não imobiliza a biblioteca local em outro
checkout; conservar o commit e as alterações locais junto das evidências.

Os dez ambientes OBC compilaram, incluindo as duas novas bancadas de
limites da INT-006. Regressões originais de receptor/entregas/downlink
passaram com ASan/UBSan. A bancada USB anterior passou sem alterar seu
roteiro, com 46 ACKs, q máximo de 31 ms e resposta máxima de 487,506 ms.
Preservados retentativa identificada e três arquivos extraídos com CRC.
Resultado em [obc-limits-2026-11-11-regression.json](../../../examples/obc-limits-2026-11-11-regression.json).

A fixação está concluída no escopo local; a INT-004 permanece em andamento
pelo aceite TT&C e demais condições existentes. Os ensaios de capacidade/
memória, firmware restaurado e hashes estão na
[INT-006](../INT-006-fluxo-de-dados.md#limites-de-capacidade-e-memória--11112026).

## Aceite OBC e evolução do consumidor — 11/11/2026

O responsável OBC aceitou os formatos `[boot,seq]`, `cf`, `request_log`
com blocos de 90 bytes, offset seguinte derivado e paginação com `end`.
O consumidor foi integrado na INT-006, com timeout de confirmação de 10 s
provisórios. Os quatro builds OBC/bancadas passaram; a nova integração
USB passou em placa após correção da admissão de memória, conforme a
INT-006, mantendo a reprovação anterior registrada. O roteiro é `ObcDownlinkCheck.py`;
`ObcTtcCheck.py` e `ObcDataFlowCheck.py` conservam suas versões históricas.

Esse aceite pelo OBC não substitui o aceite conjunto TT&C das interfaces,
unidades e condições operacionais, nem comprova persistência correta da
estação ou RF físico. Manter a INT-004 em andamento. As três falhas da
estação e os pedidos aos setores estão nos
[encaminhamentos](../../../encaminhamentos-2026-11-11.md).

## Histórico do estado e uso dos roteiros — 10/11/2026

A biblioteca TT&C já está ligada ao firmware central. O histórico abaixo registra a versão INT-004, na qual `set_time` era rejeitado. A INT-006 posterior implementou esse comando e a TM integrada. Orientação vigente de 14/11: usar `ObcDownlinkCheck.py` e o complemento `ObcStationCheck.py` para o firmware atual, conforme o [guia](../../../examples/README.md#bancada-integrada-int-006). `ObcDataFlowCheck.py` também é histórico, anterior a `request_log`/quadros `ac`. Preservar `ObcTtcCheck.py` e suas expectativas para a versão anterior.

O aceite conjunto TT&C ainda mantém esta tarefa aberta; a bancada parcial não encerra o boot de voo da INT-003 ou ensaios RF. É possível preparar o aceite com contratos/mapeamentos e evidências existentes sem novos dispositivos. As versões Espressif32/Arduino registradas abaixo são as observadas naquela execução, quando a plataforma ainda não estava fixada. A fixação posterior está registrada acima. ArduinoJson/LoRa estão fixados no consumidor OBC; `tt-c` acompanha o checkout local.

## Objetivo

Conectar o OBC ao transporte existente de telecomandos e telemetria.

## Entrega

Reutilizar a biblioteca TT&C: Init()/Start(), ReceiveTelecommand(), SendAck(), Send()/SendTelemetry() e UmbilicalLink. Não executar Update() concorrentemente com Start(). Preservar tarefa do enlace no Core 0, prioridade padrão 10, ISR de DIO0 apenas notificando e acesso exclusivo ao rádio. Declarar dependências e fixar versões validadas no ambiente consumidor. A aplicação de set_time pertence a [INT-006](../INT-006-fluxo-de-dados.md); ações de missão a [INT-007](../INT-007-comandos-modos-e-protecoes.md).

## Pré-requisitos

- **Tarefas:** [INT-003](../INT-003-inicializacao-central.md)
- **Condições externas:** Biblioteca TT&C disponível; conferir montagem, versões e contrato com o setor.

## Definições e condições

- Rádio usa `SPI` global; SD exige instância própria HSPI. Parâmetros atuais TT&C: 921 MHz, SF7, BW 125 kHz, CR 4/6, preâmbulo 32, CRC do rádio e TX sob demanda. Preservar decisão TT&C; validar velocidades/coordenação com estação e não duplicar filas.

## Critérios de conclusão

Transporte ligado ao firmware central, TC/ACK e TC/TM conectados, contrato necessário acordado, build e regressões pertinentes aprovados. Preservar ACK pelo meio de origem e os limites de entrega TC ≤200 ms e resposta ≤2 s; comprovação integrada em [TES-008](../../04-testes/TES-008-integracao.md).

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [DAD-001](../../02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md) | Consolidar com TT&C os mapeamentos e unidades do conteúdo, mantendo o contrato de TC/TM e registrando o aceite das interfaces usadas. | Atendida em 17/11/2026: mapeamento documentado e aceite TT&C informado pelo responsável OBC-DH, conforme registro no início desta task |

## Evidências

### Implementação e bancada — 09/11/2026

`ObcTtc` integra o transporte local `tt-c` ao firmware central após o boot
existente. Mantém uma tarefa TT&C no Core 0, prioridade 10, e usa as filas,
deduplicação e ACK por origem da biblioteca. O atendimento no `loop()`
aguarda até 20 ms por TC. Rádio ausente não impede o USB; falha de filas
ou de criação da tarefa é diagnosticada sem iniciar o atendimento.

Entradas suportadas: `ping`, `get_status` e `request_tm`, sem argumentos
ou com objeto vazio. Os dois últimos publicam TM de bancada com dados
indisponíveis em `null`. Comandos catalogados ainda não implementados
recebem `invalid_cmd`; argumentos inesperados recebem `invalid_param`.
Uma TM só recebe confirmação `ok` após enfileiramento. O mapeamento e as
unidades estão no [contrato compartilhado](../../../../common/protocol/ttc-json.md#disponibilidade-no-firmware-de-bancada-obc).

Ambiente consumidor: Espressif32 6.6.0, Arduino ESP32 2.0.14, GCC Xtensa
8.4.0; ArduinoJson fixado em 7.0.4 com o patch controlado existente, LoRa
fixado em 0.8.0, biblioteca local `tt-c` 0.1.0. ESP32-D0WD-V3 revisão 3.1,
conectado por USB em `/dev/ttyUSB0`, sem SD ou rádio operacional.

| Verificação | Procedimento | Resultado |
|---|---|---|
| Contrato TT&C nativo | `pio test -e native` em `tt-c/` | 14/14 aprovados com ArduinoJson 7.4.3 resolvido pelo ambiente existente |
| Contrato com a dependência do OBC | Compilação direta C++11, `-Wall -Wextra -Werror`, UBSan e ArduinoJson 7.0.4 do OBC | 14/14 aprovados, incluindo máscara, valores zero, compatibilidade e limites de quadro |
| Transporte em ESP com meios falsos | `pio test -e esp32 --upload-port /dev/ttyUSB0 --test-port /dev/ttyUSB0` em `tt-c/` | 11/11 aprovados; ACK por origem/prioridade, deduplicação, fila cheia e cabo sem rádio |
| Regressão do boot portátil | Comando C++11/UBSan de FUNCIONAMENTO_PIO, com os testes existentes | Aprovado, incluindo as oito combinações dos serviços |
| Builds OBC/UART | `pio run -e obc`; `pio run -e uart-loopback -e uart-receiver-check` em `obc-dh/` | Os três ambientes aprovados; UART permanece isolada das dependências TT&C |
| Firmware central na placa | `pio run -e obc -t upload --upload-port /dev/ttyUSB0` | Gravado e hash de flash conferido pelo esptool |
| TC/ACK e TC/TM por USB | Python com pyserial: `tests/ObcTtcCheck.py /dev/ttyUSB0` em `obc-dh/` | 77 cenários aprovados, 74 ACKs e 37 TMs; maior resposta 42,726 ms; maior `q` 0 ms |

A verificação USB inclui 50 pedidos distintos alternando os três comandos,
rejeições, objeto vazio, comando desconhecido, envelopes ilegíveis e linha
longa, recuperação para o próximo TC válido e repetição de `request_tm`
com apenas reenvio do ACK. Confere `n` consecutivo e os limites de quadro.
Entradas, saídas e tempos completos estão no relatório local temporário
`/tmp/fcp01-int004-usb.json`, regenerável pelo roteiro. Nenhum teste anterior
teve entradas ou resultados esperados alterados. As duas correções nos
testes novos foram discutidas e autorizadas pelo responsável.

Exemplo observado de TM, sem valores fabricados:

```json
{"n":1,"t":"tm","ts":null,"md":"bench","vb":null,"ib":null,"vs":null,"is":null,"tp":null,"w":null,"az":null,"ac":null,"msg":null,"rssi":null,"snr":null,"rx":2,"err":0,"rst":5}
```

Firmware verificado: SHA-256
`c483fff437508a50b9e84e3b0501c5f9f9aab0c98b8abceff0e2a839c5c59827`.
RAM estática 25.696 bytes; flash 372.649 bytes. Esses números de build não
medem o pico de heap/stack em operação.

### Pendências e limites do aceite

- Registrar o aceite conjunto TT&C dos mapeamentos/unidades e da extensão
  compatível de disponibilidade. Não houve contato ou aprovação externa.
- Boot de voo/estados elétricos permanecem em INT-003; o boot utilizado é
  o de bancada já implementado. `set_time`/payload seguem em INT-006;
  modos, missão e atuações seguem em INT-007.
- RF físico, concorrência com os demais setores e comprovação integrada
  dos limites permanecem em TES-008. Os resultados USB não os substituem.
- Permanecer fora de `done/` até resolver o aceite pendente desta tarefa.

## Referências

- Requisitos relacionados: REQ-OBC-002, REQ-OBC-003. Implementação não equivale a requisito validado.
- [tt-c/include/Ttc.h](../../../../tt-c/include/Ttc.h)
- [tt-c/include/TtcProtocol.h](../../../../tt-c/include/TtcProtocol.h)
- [common/protocol/ttc-json.md](../../../../common/protocol/ttc-json.md)
- [tt-c/ENSAIOS.md](../../../../tt-c/ENSAIOS.md)
- Origem no planejamento anterior: P-SPI.
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
