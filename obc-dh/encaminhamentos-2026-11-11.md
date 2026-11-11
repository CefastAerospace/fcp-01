# Encaminhamentos OBC-DH — 11/11/2026

Mensagens preparadas para envio pelo responsável. Nenhuma comunicação externa
foi realizada pelo agente. Base: revisão de `f7af605` e integração OBC local
subsequente, sem commit nesta entrega.

## Para TT&C

Atualização OBC: plataforma consumidora fixada em Espressif32 6.6.0,
dez builds e regressões originais nativas/USB aprovados. Bancadas
adicionais verificaram cache cheio (32 admissões/oito perdas), confirmação
FIFO e retomada, além de JSON de 65.536 bytes e NO_MEMORY preservando o
snapshot. Maior q observado de 92 ms e ACK de 795,705 ms nesses dois
cenários. São recortes USB com Payload/SD simulados; o timeout de 10 s
continua provisório. Nenhum código ou ambiente próprio TT&C foi alterado.

Pelo OBC, aceito o ID `[boot,seq]`, a confirmação `cf`, os blocos de até 90
bytes e a listagem com `end`. A integração OBC foi implementada: envio de
aeronaves, consumo de eventos TX e confirmações, além do despacho de
`request_log` com destino selecionável. Os testes nativos locais e os quatro
builds ESP32 passaram. A bancada de extração reprovou inicialmente por uma
admissão incorreta de memória; após correção aprovada, o mesmo roteiro
passou integralmente, com retentativa identificada e três arquivos extraídos
com CRC válido. O firmware normal sem periféricos também passou e ficou na
placa. As entradas e expectativas dos testes não foram alteradas.
O timeout OBC é 10 s provisórios, pendente de medição conjunta.

Também executei `ObcStationCheck.py` com as APIs reais Link/list_files/
download_file e armazenamento simulado no ESP32. O cliente confirmou sete
entregas e baixou três arquivos DATA/CONFIRM, conferidos por CRC e pelo
leitor OBC. TM/tempo passou nos recortes de bancada e firmware normal,
sem alterar a implementação TT&C. Logs, estado, downloads e relatórios
estão isolados nas evidências OBC da INT-006. A aprovação em operação
normal não encerra os problemas de falha/reinício descritos abaixo.

Na revisão da estação sobre `f7af605`, encontrei três problemas para vocês
organizarem as correções e verificações:

1. `fcp_gs/session_log.py`, `SessionLog.write`: o conteúdo recebe `flush`,
   sem `fsync`, antes de `cf`. A rastreabilidade confirmou sincronização
   somente de `deliveries.jsonl`. Uma queda do sistema pode preservar o ID
   sem preservar o conteúdo. A garantia exige conteúdo e ID duráveis antes
   de confirmar a entrega.
2. `fcp_gs/deliveries.py`, abertura em append após `_load`: a cauda truncada
   é ignorada na leitura, mas recebe diretamente a próxima gravação.
   Reproduzi com `{"id":[3,41]}\n{"id":[3,` e a gravação de `(3,42)`.
   Após outro reinício, `(3,42)` não é reconhecido. O teste atual de reinício
   não recarrega o registro uma segunda vez após essa gravação.
3. `fcp_gs/link.py`, `handle_line` e `_read_loop`: falha de escrita do log
   escapa do laço de recepção. Reproduzi com `OSError("disco cheio")`:
   nenhum `cf` é enviado, mas os quadros seguintes deixam de ser atendidos.
   Precisamos conservar a recepção e um diagnóstico que não dependa do log
   defeituoso enquanto o OBC retransmite.

Os 17 testes existentes da biblioteca, incluindo os cinco novos, passaram
no ESP32 com FakeTransport, sem alteração do setor. A fila comum cheia
preservou o ACK prioritário; falha local e reserva dos quatro eventos TX
também passaram. A bancada USB adicional OBC aprovou timeout de leitura,
busy, erro parcial, confirmação desconhecida/duplicada/atrasada e falha de
gravação simulada, mantendo as sete entregas e os comandos. Retentativa
em 10.000,149 ms, q máximo de 32 ms, maior ACK em 1.060,223 ms durante o
timeout. Isso não mede a fila RF nem define timeout operacional.

Continuam pendentes entrega/reconfirmação após reinício da estação e as
medições conjuntas de fila, timeout, congestionamento e prioridade no enlace
real. O TODO do TT&C também
conserva a informação antiga de que o `main.cpp` do OBC está vazio e um link
para `P-DADOS` que já não representa o painel vigente. Encaminho para gestão
do setor; nenhum arquivo de `tt-c/` foi alterado nesta integração.

## Para Payload

Para concluir o contrato Payload–OBC, precisamos registrar o aceite do FCPU,
responsável e configuração da UART no Pi, produtor dos snapshots e perfil
nominal/rajadas/retenção. Também precisamos fechar origem, unidades,
idade/validade dos campos, incluindo altitude, velocidade, rumo, contagem e
horário individual.

A conservação de cada atualização ADS-B intermediária permanece aberta:
um snapshot acumulado não comprova esse requisito. O OBC já registra e
transporta alterações da projeção disponível, mas isso não recupera mensagens
que não chegaram no snapshot. Até o acordo, velocidade, rumo, contagem e
horário individual continuam `null` no downlink, sem inferir valores de `id`.

## Para o setor de testes

Limites adicionais aprovados: `int006-capacity-check` e
`int006-memory-check`, com `ObcLimitsCheck.py --case capacity|memory`.
Relatórios novos obc-limits-2026-11-11-capacity/memory/regression/obc.json
na INT-006. A saturação exige exatamente 32 admissões/oito perdas antes
de liberar confirmações; a memória exige NO_MEMORY sem modificar o último
snapshot e retomada com quadro válido. Foi observado heap interno mínimo
de 1.040 bytes durante esgotamento. Não garante todos os máximos
simultâneos nem substitui ensaios prolongados, físicos ou de reset.
Os testes antigos foram preservados e obc ficou restaurado/verificado.

Estresse adicional aprovado: `int006-stress-check`/`ObcStressCheck.py`,
apenas ESP32/USB, com injeções no backend simulado. Cobriu timeout de 1 s
com leitura de 1,5 s, caixa retida até conclusão, descarte tardio, busy,
erro parcial sem publicar dados, confirmações fora de ordem/repetidas e
NO_SPACE após oito FCD1. Sete entregas confirmadas, 59 ACKs, q máximo de
32 ms e maior ACK em 1.060,223 ms. Firmware normal restaurado e verificado.
Os 17 testes TT&C existentes também passaram na placa, com transportes
falsos. Relatórios e reprodução na INT-006. Não substitui falha FAT32,
corte de energia, RF ou reinício da estação; seis CONFIRM não persistidos
no cenário de falha podem gerar retransmissões após reset.

Complemento executado: `ObcStationCheck.py` passou com um ESP32/USB em
`int006-ttc-check` e em `obc` sem periféricos. Usa o cliente real da estação,
confere arquivos com o leitor OBC e acrescenta casos de UTC desconhecido,
época zero, set_time inválido, referência atual e ac=7/depois 0. Os relatórios
`obc-station-2026-11-11-bench.json` e `obc-station-2026-11-11-obc.json`
e os artefatos estão na INT-006. Esse recorte não cobre queda/reinício da
estação ou cartão real. Preservar também o roteiro de perda de cf abaixo.

Atualização após a correção: a primeira reprovação de `--bench` foi
preservada, e o OBC corrigiu a distinção entre teto do parser e admissão das
pilhas/margem em memória por byte. O mesmo `ObcDownlinkCheck.py --bench`
passou integralmente; `obc` sem periféricos também passou. Os relatórios
`obc-downlink-2026-11-11-bench-02.json` e `obc-downlink-2026-11-11-obc-02.json`
e os hashes da versão estão na INT-006. O teto do parser, seus erros previstos
e os critérios do roteiro permanecem. A placa voltou ao firmware normal.

A integração OBC de entregas identificadas e `request_log` está disponível
com roteiro próprio. Executar `int006-ttc-check` com
`tests/ObcDownlinkCheck.py PORTA --bench`, conforme `tools/README.md`, e
devolver a placa ao firmware `obc` ao terminar. O roteiro perde o primeiro
`cf`, confere a retentativa e extrai registros FCD1 com verificação do CRC.
Payload, SD e solo são simulados nessa bancada; a biblioteca TT&C e o
umbilical são reais. Isso não valida a persistência da estação.

O roteiro atual também pode exercitar `obc` sem `--bench`; ausência do serviço
SD deve produzir `no_sd`. `ObcTtcCheck.py` e `ObcDataFlowCheck.py` continuam
arquivados para suas versões históricas, sem alteração de entradas ou
expectativas. `ObcDeliveryBenchCheck.py` continua destinado à bancada com
transporte simulado `int006-delivery-check`.

Além desses recortes, precisamos verificar em TES-008/TES-009 o enlace real,
perda/repetição de `cf`, resets do OBC e da estação, extração com o cliente
TT&C, cartão físico, congestionamento, tempos de TC e integridade dos
registros. Registrar versão, condições, entradas, esperado, observado e
evidências. Encaminhar falhas aos responsáveis sem alterar firmware ou
critérios de aceitação pelo setor de testes.
