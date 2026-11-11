# INT-006 — Fluxo Payload–tempo–SD–telemetria

**Estado:** Em andamento — primeira etapa implementada e verificada localmente; segunda etapa e aceites pendentes
**Responsáveis:** OBC-DH, Payload e TT&C

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

## Preparação da segunda etapa — 10/11/2026

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
| [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Ligar registros ao SD, nomes/rotação por boot e limite de arquivo, contabilização de perdas, encaminhamento de erros e extração pelo umbilical. | Fluxo/nomes/rotação/perdas verificados localmente; extração e cartão físico pendentes |
| [DAD-001](../02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md) | Consolidar o aceite de Payload/TT&C e das estatísticas: id opaco, idade/validade dos campos, unidades de velocidades, extras e migração explícita dos dados legados, sem inventar sessão/sequência ou validade. | Pendente |
| [DAD-003](../02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) | Integrar criação, gravação e extração de registros e referências temporais por boot, preservando formatos v1/v2 e leitura retrocompatível. | Criação/gravação/referências ligadas e leitura v1/v2 preservada; extração pendente |
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

- Segunda etapa: seleção/frequência/validade do envio de aeronaves e contrato
  de extração por `request_log`, incluindo ferramenta de solo e reprodução.
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
