# Estação terrena do FCP-01

Software do notebook da estação terrena (DP §3.8 e §7.4). Conversa em JSON com o satélite, conforme o contrato em [`common/protocol/ttc-json.md`](../../../common/protocol/ttc-json.md).

```
satélite (ESP32 + SX1276) ~~ LoRa 921 MHz ~~ ponte (ESP32 + SX1276) == USB ==> notebook (este app)
satélite (ESP32)          ======================= USB-C, umbilical =======> notebook (--wired)
```

- **Modo RF:** o notebook se conecta à ponte (`tt-c/ground-station`, firmware ESP32), que repassa cada quadro LoRa com RSSI e SNR.
- **Modo umbilical (`--wired`):** o notebook se conecta direto ao USB do satélite (UART0). A competição pontua o modo cabeado separadamente do RF.

Funciona sem internet: não usa CDN nem serviços externos.

## Instalação

Requer Python 3.10 ou superior.

```sh
cd tt-c/ground-station/app
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

Para instalar sem internet no local de testes, baixe os pacotes antes (`pip download -r requirements.txt -d wheels/`) e depois rode `pip install --no-index --find-links wheels/ -r requirements.txt`.

## Uso

```sh
.venv/bin/python -m fcp_gs ports                                # lista as portas seriais
.venv/bin/python -m fcp_gs serve --port /dev/ttyUSB0            # interface web, modo RF
.venv/bin/python -m fcp_gs serve --port /dev/ttyUSB0 --wired    # interface web, umbilical
.venv/bin/python -m fcp_gs serve --demo                         # interface web sem hardware
.venv/bin/python -m fcp_gs serve --port ... --map-center -34.57,-58.52,9   # centro inicial do mapa (LAT,LON[,ZOOM])
```

Abra `http://localhost:8080`. O painel mostra:

- o estado do enlace: pacotes, perdas pelo contador `n`, CRC inválido e sinal no solo;
- a última TM: modo, energia, temperaturas, atitude, missão e enlace a bordo;
- gráficos de bateria, temperaturas e RSSI;
- o mapa das aeronaves, com o rastro de cada uma (últimas 200 posições, guardadas no servidor, então recarregar a página não apaga). O rastro fica esmaecido após 30 s sem notícia e some após 15 min;
- a tabela de aeronaves;
- o envio de TCs (com retentativa automática e o mesmo `seq`): botões para os comandos sem argumento, seletor de modo para `set_mode` e formulário livre que preenche o exemplo de `args` do catálogo. ACK e NACK (`invalid_param`, `state_rejected`...) aparecem na lista de resultados;
- os roteiros de verificação;
- o log ao vivo.

## Entregas identificadas do OBC

Pacote de aeronave com `"id":[boot,seq]` é uma entrega da INT-006. A estação grava o pacote no log da sessão, registra o ID com `fsync` em `state/deliveries.jsonl` e só então responde `{"t":"cf","id":[boot,seq]}`. Quando chega um ID já registrado, inclusive de uma sessão anterior, a estação reenvia a confirmação e não reaplica o conteúdo na tabela, no rastro ou no mapa; no log ao vivo a linha aparece como "entrega repetida". Campos indisponíveis chegam como `null` e aparecem como "—". O painel `/api/state` traz os contadores em `deliveries`. Para começar uma campanha nova sem histórico, mova ou apague `state/deliveries.jsonl` com a estação parada. O modo demo usa um registro só em memória.

## Arquivos do SD (request_log)

```sh
.venv/bin/python -m fcp_gs logs list --port /dev/ttyUSB0             # nome, bytes, aberto/fechado
.venv/bin/python -m fcp_gs logs get NOME --port /dev/ttyUSB0         # extrai para downloads/
.venv/bin/python -m fcp_gs logs get --all --port ... --out pasta/    # todos os arquivos fechados
```

A extração pede blocos de 90 bytes e só avança depois de validar a resposta do mesmo pedido. Em `busy` ou resposta perdida, repete o mesmo offset com um `seq` novo. O arquivo é gravado como `NOME.part` e renomeado no fim. Para conferir os CRCs dos registros, use `obc-dh/tools` sobre o arquivo extraído. Esse comando depende de o OBC implementar o `request_log`; hoje o OBC responde `invalid_cmd`.

A porta é aberta com DTR/RTS desligados, mas no Linux a placa ainda pode reiniciar na abertura. Por isso o app só envia o primeiro TC depois de a serial ficar 1 s sem bytes novos (no mínimo 1,5 s, no máximo 6 s); as mensagens do boot vão para o log da sessão.

## Mapa sem internet

O mapa tem duas camadas, que podem ser ligadas e desligadas no canto do mapa:

- **Contornos offline:** países e divisas estaduais do Natural Earth (`fcp_gs/static/vendor/borders/`). Estão sempre disponíveis.
- **OSM (cache local):** o servidor busca as folhas do OpenStreetMap e grava em `cache/tiles/`. Sem rede, só aparece o que já está no cache.

Antes de ir ao local de testes, com internet, pré-carregue a região:

```sh
.venv/bin/python -m fcp_gs tiles --bbox -16.2,-48.2,-15.4,-47.3 --zoom 4-11   # SUL,OESTE,NORTE,LESTE
```

A política de uso do OSM proíbe downloads em massa, por isso o comando recusa pedidos com mais de 2000 folhas: reduza a área ou o zoom máximo. Com `--zoom 4-11`, uma área de cerca de 1° × 1° fica bem abaixo do limite.

## Modo demo

`serve --demo` simula o satélite na memória, sem serial, para desenvolver e apresentar a interface:

- TM a cada 2 s, com a bateria e as temperaturas variando em ciclo de órbita;
- 6 aeronaves em voo ao redor de `--map-center`, em rotas retas com rumo coerente, mais uma em órbita de espera. O tempo de voo corre 20× mais rápido, para o rastro crescer em segundos, e quem sai da área é trocado por uma nova aeronave;
- os TCs mudam o estado como o OBC faria:
  - `set_mode`, `enter_safe`/`exit_safe` e `start_mission`/`stop_mission` aparecem no campo `md`;
  - `adcs_start` faz a taxa angular cair;
  - `set_time` desloca o horário de bordo;
  - `deploy_antenna` gera o evento `antenna_deployed`;
  - `reset_obc` reinicia o contador `n` e conta como reinício no painel;
- `args` inválidos geram NACK `invalid_param`, e `start_mission` em safe gera `state_rejected`;
- em safe, o satélite manda beacon a cada 10 s;
- no modo RF, os quadros chegam no envelope `gs` da ponte, com RSSI e SNR variando. Com `--wired`, chegam sem envelope.

## Roteiros de verificação

Podem ser executados pela interface ou pela linha de comando. Cada execução grava um relatório em `reports/` (JSON completo e resumo em Markdown). Na linha de comando, o código de saída é 0 se o teste passou.

| Comando | Requisito | Critério |
|---|---|---|
| `python -m fcp_gs tm-loss --port ... --count 1000` | REQ-TTC-003 | Rajada de 1000 TMs (`test_tm`) com perda < 1,0%, contada pelo `n`. Um `ping` final revela perdas no fim da rajada; sem o ACK dele, ou com mais de 1,0% das TMs faltando, o ensaio é reprovado. |
| `python -m fcp_gs tc-latency --port ... --count 50` | REQ-TTC-004, HLR-COMM-01 | 50 TCs distintos; entrega ao OBC (`q` do ACK) ≤ 200 ms e ida e volta ≤ 2 s. |

O `test_tm` é atendido pelo firmware `tt-c/examples/link-test`, que simula o OBC.

## Registros e análise pós-passagem

Cada sessão grava `logs/AAAA-MM-DD_HHMMSS.jsonl`: tudo o que foi enviado e recebido, com o horário do PC e, no modo RF, RSSI e SNR do solo (HLR-SW-02).

```sh
.venv/bin/python -m fcp_gs analyze logs/2026-10-14_120000.jsonl
```

O comando gera `telemetry.csv` e `aircraft.csv` em uma pasta com o nome do log, e imprime o resumo da sessão: perdas por meio, maior atraso de entrega de TC e aeronaves vistas.

## Testes

```sh
.venv/bin/python -m pytest tests
```

Os testes usam um satélite simulado na serial (`tests/conftest.py`) e não precisam de hardware.
