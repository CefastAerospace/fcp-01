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
```

Abra `http://localhost:8080`. O painel mostra:

- o estado do enlace: pacotes, perdas pelo contador `n`, CRC inválido e sinal no solo;
- a última TM: modo, energia, temperaturas, atitude, missão e enlace a bordo;
- gráficos de bateria, temperaturas e RSSI;
- a tabela de aeronaves;
- o envio de TCs (com retentativa automática e o mesmo `seq`);
- os roteiros de verificação;
- o log ao vivo.

A porta é aberta com DTR/RTS desligados, mas no Linux a placa ainda pode reiniciar na abertura. Por isso o app só envia o primeiro TC depois de a serial ficar 1 s sem bytes novos (no mínimo 1,5 s, no máximo 6 s); as mensagens do boot vão para o log da sessão.

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
