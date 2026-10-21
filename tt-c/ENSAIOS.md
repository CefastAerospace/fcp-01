# Ensaios do enlace TT&C

Registro dos ensaios em hardware do enlace LoRa entre o satélite (`examples/link-test`, que simula o OBC) e a estação terrena (`ground-station` + `ground-station/app`). Os logs e relatórios brutos ficam em `ground-station/app/logs/` e `ground-station/app/reports/`, que não são versionados; os números relevantes são copiados para cá.

Configuração comum: SX1276 a 921 MHz, SF7, +17 dBm, CRC de hardware ligado; ligação do rádio conforme `common/interfaces/pinout.md` (VSPI: SCK 18, MISO 19, MOSI 23, CS 5, RST 14, DIO0 4). Satélite: ESP32 com CH340. Estação terrena: ESP32 com CP2102.

## Comparação

| Ensaio | Data | Distância / cenário | TMs recebidas / pedidas | Perda pelo `n` | CRC inválido | RSSI (mín / mediana / máx) | SNR (mín / mediana / máx) | Resultado |
|---|---|---|---|---|---|---|---|---|
| E1 | 21/10/2026 | Bancada, placas lado a lado | 255 / 1000 | 1 em 258 (0,39%) | 0 antes da falha | −45 / −44 / −42 dBm | — | **Inválido**: o rádio da estação terrena caiu aos 108 s |
| E2 | 21/10/2026 | Bancada | 1011 / 1000 | 0 em 1025 (0,00%) | 0 | −94 / −68 / −61 dBm | 8,8 / 9,8 / 12,8 dB | **Aprovado** (REQ-TTC-003) |
| E3 | | Entre prédios: | | | | | | |

Nas TMs recebidas entram também as TMs periódicas de 10 s, por isso o total passa de 1000. No E2: 1000 TMs da rajada, 11 periódicas, 13 aeronaves e 2 ACKs (1026 pacotes). As demais TMs periódicas e aeronaves encontraram a fila de descida cheia e foram descartadas antes de receber `n`, sem contar como perda. A métrica do requisito é a perda pelo `n`; a contagem de TMs é só uma conferência.

O relatório do E2 mostra 1025 pacotes porque o ACK do ping de fechamento não entrou na conta: o app entregava o ACK a quem o esperava antes de repassá-lo ao contador do `tm-loss`. Sem perdas no E2, o resultado não muda; a ordem foi corrigida depois.

## E1 — bancada, 21/10/2026

- Comando: `python -m fcp_gs tm-loss --port /dev/ttyUSB1 --count 1000`
- Log: `ground-station/app/logs/2026-10-21_101648.jsonl`. Relatório: `ground-station/app/reports/tm-loss_2026-10-21_101848.{json,md}`.

A ferramenta marcou **APROVADO** (0,39% de perda, limite 1,0%), mas o ensaio não é válido para o REQ-TTC-003:

- Chegaram só 255 das 1000 TMs pedidas. Os contadores `n` foram contínuos de 28 a 285 entre 0,5 s e 108,5 s (cerca de 2,4 TMs/s).
- Aos 108,7 s, o rádio da **estação terrena** parou de responder pelo SPI: RSSI lido como −157 dBm (registrador em zero), seguido de `[GS] ERRO: falha no TX` e `timeout no TX, reinicializando o SX1276`. O `ping` de fechamento esgotou as três tentativas.
- O único pacote perdido (`n` = 154, aos 53,6 s) chegou ao notebook como uma linha serial corrompida, não como erro de RF.
- O contador `rst` do satélite subiu de 3 para 83 durante a rajada; ver o diagnóstico abaixo.

A ferramenta foi corrigida depois deste ensaio: agora reprova quando o `ping` de fechamento falha ou quando faltam mais de 1,0% das TMs pedidas.

## E2 — bancada, 21/10/2026

- Firmware das duas placas com a correção do retorno ao RX (ver abaixo).
- Comando: `python -m fcp_gs tm-loss --port /dev/ttyUSB0 --count 1000` (estação terrena em `/dev/ttyUSB0` nesta sessão).
- Log: `ground-station/app/logs/2026-10-21_125646.jsonl`. Relatório: `ground-station/app/reports/tm-loss_2026-10-21_130351.{json,md}`.
- Duração: 424,5 s. `ping` de fechamento confirmado. Satélite com `rst` = 0 e `err` = 0 durante todo o ensaio.

Uma rodada anterior (relatório `tm-loss_2026-10-21_105757`) foi descartada: o `ping` de fechamento saiu antes do fim da rajada, porque as TMs periódicas completaram a contagem cerca de 6 s antes. O satélite, transmitindo sem pausa, não ouviu o `ping`, e a estação perdeu 4 TMs enquanto transmitia (enlace half-duplex). O roteiro passou a esperar 2 s de silêncio antes do `ping`.

## Diagnóstico do `rst` do satélite

Sintoma: reinicializações do SX1276 sempre logo após o fim de um quadro, quando a task do enlace emenda um TX no outro (rajada de 30 s com TM e três aeronaves; no E1, quase toda verificação de saúde de 1 s). Nenhuma com TMs isoladas. O padrão sistemático descartou mau contato.

Causa, confirmada imprimindo os registradores na falha: `RegVersion=0x12 RegOpMode=0x81`, ou seja, o rádio estava em STANDBY. Depois do TX Done, o SX1276 ainda volta sozinho a STANDBY; o pedido de RX feito logo em seguida era sobrescrito e o rádio ficava surdo até a verificação de saúde reinicializá-lo. A estação terrena tem o mesmo driver e não faz verificação de saúde, então podia ficar surda indefinidamente depois de transmitir um TC.

Correção: `Sx1276Radio::EnterReceive()` espera o fim do TX, pede RX contínuo e confirma pelo `RegOpMode`. A verificação de saúde passou a imprimir `RegVersion` e `RegOpMode` quando falha.

## Observações sobre a serial

- No Linux, abrir a porta reinicia a placa mesmo com DTR/RTS desligados, às vezes mais de 1 s depois da abertura. Um TC enviado nesse intervalo se perde. O app agora só envia o primeiro TC depois de a serial ficar 1 s sem bytes novos.
- O adaptador CP2102 da estação terrena entrega lixo na abertura da porta e, de vez em quando, troca cerca de 64 bytes do início de uma linha por lixo. Isso custou um pacote no E1. Se voltar a aparecer, trocar o cabo ou a porta USB.
- `Fila de descida cheia, pacote descartado` no log do satélite durante `test_tm` são as tentativas do `link-test` de enfileirar a próxima TM da rajada, repetidas a cada 20 ms; não há perda, pois o `n` só é atribuído no TX.

## Procedimento para o ensaio entre prédios

1. Levar o satélite para o outro prédio e anotar o local exato de cada placa (prédio, andar, janela ou interior) e a distância estimada.
2. No PC do satélite, deixar o log serial gravando durante todo o ensaio.
3. No PC da estação terrena, confirmar a porta com `python -m fcp_gs ports`, mandar um `ping` e rodar `tm-loss`; se o enlace permitir, `tc-latency`.
4. Preencher a linha E3 da tabela e anotar os nomes dos arquivos de log e relatório.
