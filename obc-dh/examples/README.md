# Bancada UART2 — beta preservado

Limites adicionais de capacidade/memória, 11/11/2026, ESP32/USB e
Espressif32 fixado em 6.6.0: aprovados os relatórios de
[capacidade](obc-limits-2026-11-11-capacity.json),
[memória](obc-limits-2026-11-11-memory.json),
[regressão USB original](obc-limits-2026-11-11-regression.json) e
[obc restaurado](obc-limits-2026-11-11-obc.json).
Os novos roteiros preservam evidências e vetores anteriores. Condições,
resultados e limites de memória na
[INT-006](../.tasks/03-integracao/INT-006-fluxo-de-dados.md#limites-de-capacidade-e-memória--11112026).

Estresse adicional com ESP32/USB, 11/11/2026: ambiente separado
`int006-stress-check` e roteiro `ObcStressCheck.py` aprovados.
[Relatório da bancada](obc-stress-2026-11-11-bench.json) conserva log,
comandos, pacotes e medições. [Relatório normal](obc-stress-2026-11-11-obc.json)
confirma obc restaurado e no_sd esperado. Também passaram os
[17 testes TT&C existentes na placa](obc-ttc-2026-11-11-esp32.txt), com
transportes falsos. Procedimento e injeções no
[guia](../tools/README.md#estresse-em-um-esp32-e-usb--11112026).
Esses recortes não comprovam cartão, RF ou reinício da estação.

O ambiente `uart-loopback` executa o código atual de `src/PayloadUart.cpp`
com a entrada isolada `src/bench/UartLoopback.cpp`. O ambiente normal `obc`
continua sendo o padrão e não compila a entrada de bancada. Não há alteração
do boot central, do parser, dos vetores ou da lógica de sequência do beta.

`payload-uart-beta/` conserva uma cópia integral dos arquivos originais,
feita antes da tentativa interrompida de implementação do receptor final.
Essas cópias são referências e não participam da compilação de bancada.

## Ligação e execução

Para a placa identificada como **ESP32-WROOM-32**:

1. Desconecte a alimentação e qualquer ligação externa de G16/G17.
2. Ligue **G17 (TX2) a G16 (RX2)** com um jumper. Não use TX0/RX0.
3. Conecte o USB. A bancada utiliza níveis de 3,3 V e alimentação pelo USB;
   não conecte G16/G17 a 5 V ou ao Raspberry Pi durante o loopback.
4. Execute os comandos abaixo a partir da raiz do repositório. Identifique
   a porta com `pio device list` e substitua `/dev/ttyUSB0` pela porta real.

```bash
pio run -d obc-dh -e uart-loopback
pio device list
pio run -d obc-dh -e uart-loopback -t upload --upload-port /dev/ttyUSB0
pio device monitor -p /dev/ttyUSB0 -b 115200
```

Abra o monitor e pressione **EN/RESET** para capturar a execução desde o
início. Não confunda EN/RESET com BOOT. Feche o monitor antes de novo upload.

## Resultado esperado

O emissor envia uma mensagem por segundo, 60 vezes, com ICAO `E4953A`,
altitude `39000` pés e velocidade `489` nós. As sequências são `1, 2, 4, 5,
... 61`. O receptor deve mostrar as mesmas 60 mensagens, sem erro de JSON
ou excesso de buffer. Deve haver exatamente um diagnóstico de perda:

```text
PACOTE(S) PERDIDO(S): esperado 3, recebido 4
NACK SIMULADO: solicitar pacote 3
```

Por volta de 61 segundos, a entrada informa que os 60 envios terminaram.
Essa mensagem conta chamadas ao emissor; não é um resultado PASS e não
comprova recepção. O receptor continua sendo atendido. Pressionar EN/RESET
deve iniciar novamente a sequência em 1.

Registre firmware, placa, porta, ligação e log completo. Compare os 60
`Enviado SeqID=` com os 60 `Recebido:` e confira o único gap. Se houver
divergência, preserve os logs e diagnostique antes de alterar o código ou
os resultados esperados.

## Limites e retorno ao firmware normal

Loopback comprova apenas a UART2 local com os vetores do beta. Não comprova
comunicação com o Pi, snapshots completos, integridade, timeout ou NACK real.
Não substitui a verificação de software arquivada em TES-004 nem encerra TES-005. A TES-005 atual exige snapshots JSON; o beta preserva seus próprios vetores históricos.

Para restaurar o firmware normal, feche o monitor, desligue a placa, remova o jumper G17–G16 e reconecte somente o USB. Depois execute:

```bash
pio run -d obc-dh -e obc -t upload --upload-port /dev/ttyUSB0
```

## Receptor de snapshots — FCPU v1

O receptor final fica em `PayloadReceiver`/`PayloadReceiverEsp32`, separado
do beta. O [contrato FCPU v1](../../common/protocol/payload-uart.md) preserva
o JSON como conteúdo e define cabeçalho/controles binários transmitidos pelo cabo ao Pi. `obc` exclui o beta e inicia recepção, diário, downlink identificado e extração. A integração nova ainda não foi executada em placa; `payload/telemetry.py` não foi alterado e não implementa a ponta UART.

O contexto possui dois blocos internos no heap, reservados uma única vez por
`Init`: quadro de 65562 bytes e snapshot de 64672 bytes. Verificar o retorno
de `Init`. Contexto e acesso ao snapshot devem permanecer estáveis na mesma
tarefa consumidora. No caminho sequencial das bancadas, chamar `Poll()` ao menos a cada 10 ms e consumir READY antes da próxima chamada. No firmware integrado, `StartTransportTask()` transfere a propriedade da UART à tarefa de transporte, enquanto `Poll()`/parser ficam no consumidor. O limite de 10 ms refere-se ao atendimento do transporte; o custo do parser aparece separado em `process_gap_ms`. Não usar UART2 pelo beta e pelo receptor ao mesmo
tempo. Consumo e pilha precisam ser medidos novamente na integração real.

### Teste nativo sem Raspberry Pi

Execute este bloco dentro de `obc-dh/`:

```bash
pio pkg install -e obc
python3 tools/PatchArduinoJson.py
g++ -std=c++11 -Wall -Wextra -Werror -O1 -g \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Iheaders -I.pio/libdeps/obc/ArduinoJson/src \
    src/ObcData.cpp src/ObcRecordCodec.cpp src/PayloadReceiver.cpp \
    tests/PayloadReceiverCheck.cpp -o /tmp/fcp01-payload-receiver-check
python3 tests/PayloadReceiverCheck.py /tmp/fcp01-payload-receiver-check
```

ArduinoJson 7.0.4 tem correção local aprovada de uma inicialização de iterador.
`tools/PatchArduinoJson.py` aplica a alteração somente à versão e aos hashes
conhecidos, é idempotente e recusa conteúdo desconhecido. O PlatformIO executa
o mesmo hook automaticamente. O ambiente do beta não depende dessa biblioteca.
Remover o hook e reinstalar a dependência desfaz o patch no cache.

O gerador Python cria vetores independentes com `struct`/`zlib` em diretório
temporário. O teste C++ executa o receptor real e verifica estados, snapshots
e respostas. Não altera a amostra ou os testes anteriores. ASan/LeakSanitizer
precisa executar fora de ambientes com ptrace; no sandbox deste atendimento,
o mesmo comando foi executado no host, sem desabilitar sanitizadores.

### Autoverificação no ESP32

O ambiente `uart-receiver-check` incorpora a amostra real `payload.json` em
flash, constrói um DATA e injeta seus bytes diretamente no núcleo. Confere
aceitação da amostra, repetição sem republicação e rejeição de CRC inválido.
Também inicializa o adaptador UART2 e imprime heap/pilha. Não transmite os
quadros pela ligação ao Pi; não requer o jumper G17–G16. Usa boot sintético 1,
sem reservar NVS, iniciar serviços do OBC ou comandar atuadores.

Dentro de `obc-dh/`, substitua a porta pela identificada na sua máquina:

```bash
pio run -e uart-receiver-check
pio run -e uart-receiver-check -t upload --upload-port /dev/ttyUSB0
pio device monitor -p /dev/ttyUSB0 -b 115200
```

Abra o monitor e pressione EN/RESET. O resultado esperado inclui:

```text
[UART-RX-CHECK] PASS; accepted=1; duplicates=1; rejected=1
```

Valores de heap e pilha são medições, não entradas/limites a ajustar para
forçar aprovação. A medição de 09/11/2026 na placa ESP32-WROOM-32 registrou:
contexto de 128 bytes, parser com pico de 7190 bytes, heap livre após Init
de 205900 bytes e mínima pilha livre de 3164 bytes. Esses números cobrem a
amostra de sete aeronaves, sem garantir o consumo do firmware integrado.

### Evidências preservadas

- [Loopback UART2](uart-loopback-2026-11-09.txt): 60 envios/recepções, único gap
  proposital e sequência reiniciada em 1 após reset.
- [Reserva única no heap](uart-receiver-check-2026-11-09.txt): falha tratada;
  maior bloco livre de 110580 bytes não comportava o objeto monolítico.
- [Reserva dividida](uart-receiver-check-split-heap-2026-11-09.txt): amostra,
  repetição e CRC aprovados na placa, com medições de memória/pilha.

Na entrega COM-004 foi usado `uart-receiver-check`. Após a primeira etapa
INT-006, a placa foi devolvida ao firmware normal `obc`. A bancada UART
continua disponível e não declara conclusão do ensaio ESP32–Pi ou TES-005.

## Bancada integrada INT-006

**Cliente real e TM/tempo — 11/11/2026:** `ObcStationCheck.py` passou em
`int006-ttc-check` e em `obc` sem periféricos. Reutiliza o cliente TT&C
para confirmações e downloads, e verifica UTC, época zero e TM com ADS-B
simulado. Não substitui o teste de perda deliberada de cf abaixo.
Comandos e limites estão no
[guia](../tools/README.md#cliente-real-da-estação-e-tmtempo--11112026).
Relatórios: [bancada](obc-station-2026-11-11-bench.json) e
[normal](obc-station-2026-11-11-obc.json), com artefatos nos diretórios de
mesmo nome. A placa ficou com `obc`; firmware e critérios não mudaram.

**Reexecução após correção da admissão — 11/11/2026:** o roteiro original
passou na bancada e no firmware normal, sem alterar entradas/expectativas.
Relatórios: [bancada aprovada](obc-downlink-2026-11-11-bench-02.json) e
[firmware normal aprovado](obc-downlink-2026-11-11-obc-02.json).
A placa ficou com `obc`. A admissão usa memória por byte para pilhas/margem;
o parser conserva teto de 98.304 bytes e NO_MEMORY. A aprovação é do recorte
simulado, sem cartão/radio/Pi reais. Métricas e binários estão na INT-006.

**Histórico da primeira execução USB de 11/11/2026:**
`int006-ttc-check` reprovou na extração porque a reserva de memória recusou
iniciar o escritor simulado. Retentativa identificada foi observada, mas
não aprova o conjunto. `obc` passou sem periféricos e ficou restaurado na
placa. Relatórios completos: [bancada](obc-downlink-2026-11-11-bench.json)
e [firmware normal](obc-downlink-2026-11-11-obc.json). Diagnóstico e métricas
na [INT-006](../.tasks/03-integracao/INT-006-fluxo-de-dados.md).

**Firmware atual, desde 11/11/2026:** usar `int006-ttc-check` com
`ObcDownlinkCheck.py PORTA --bench`, conforme o
[procedimento atual](../tools/README.md#integração-ttc-e-request_log--11112026).
Ele liga o adaptador TT&C real, mantendo entrada/SD/solo simulados, sem o
segundo consumidor simulado da bancada anterior. O firmware normal `obc`
usa o mesmo roteiro sem `--bench`. Os resultados e o histórico estão
registrados acima. Ao terminar, fazer upload de `obc`.

### Procedimento histórico da primeira etapa

Os comandos e resultados abaixo pertencem à versão anterior ao despacho
`request_log`. `ObcDataFlowCheck.py` foi preservado, mas não é compatível
com a integração nova: espera `invalid_cmd` e rejeita pacotes `ac`.

Execute os comandos desta seção dentro de `obc-dh/`. Requer somente ESP e USB, sem jumper e sem dispositivos externos em UART2. `int006-check` reserva boot real em
NVS e inicia TT&C e as tarefas do fluxo. O backend de arquivos é simulado:
cada escrita espera 100 ms e confere estabilidade dos bytes antes/depois.
Não monta ou confirma persistência em cartão.

`BuildFlowFixtures.py` gera em `.pio/flow-fixtures.fcpu` os quadros FCPU
independentes `struct`/`zlib`, usando a amostra original de sete aeronaves.
Inclui repetição, rajada, CRC inválido e lista vazia. Os bytes ficam na
flash e, após o primeiro `set_time` válido, atravessam o ring e o receptor
reais. A fonte simulada retém bytes quando o ring está cheio; sua rajada
não representa taxa física de UART ou taxa nominal de missão.

```bash
pio run -e int006-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDataFlowCheck.py /dev/ttyUSB0 --bench
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcDataFlowCheck.py /dev/ttyUSB0
```

Usar Python com pyserial, inclusive o Python do PlatformIO. Executar os
roteiros após boot novo; a aplicação não recupera UTC de um boot anterior.
O primeiro roteiro cobre 77 cenários; o segundo cobre 75 no firmware normal
sem cartão/Pi. O roteiro INT-004 fica preservado para a versão anterior,
na qual `set_time` ainda recebia `invalid_cmd`.

Os relatórios são saídas locais regeneráveis, não anexos permanentes. Seus destinos padrão são `/tmp/fcp01-int006-bench.json` e
`/tmp/fcp01-int006-obc.json`. Conferem estado inicial, argumentos inválidos,
UTC/timestamp na TM com ADS-B simulado, época Unix zero, comandos ainda
rejeitados, 50 pedidos de latência, contadores, estabilidade e perdas
explicitamente contabilizadas. Os critérios são `q <=200 ms`, resposta
`<=2 s` e atendimento do transporte `<=10 ms`; os valores observados e
limitações estão na [INT-006](../.tasks/03-integracao/INT-006-fluxo-de-dados.md).

`poll_gap_ms` é a cadência do transporte; `process_gap_ms` mostra o custo
do consumidor sem esconder parse/codificação demorados. `uart_full` conta
observações de ring cheio, não perda de quadros. Pilhas livres são medidas
de cada amostra, não garantia de todos os cenários de voo.
