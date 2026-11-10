# Bancada UART2 — beta preservado

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
Não encerra TES-004/TES-005. O conflito histórico de formato em TES-005
continua pendente, sem alteração de seus critérios.

Para restaurar o firmware normal, feche o monitor e execute:

```bash
pio run -d obc-dh -e obc -t upload --upload-port /dev/ttyUSB0
```

Com a placa desligada, remova o jumper ao encerrar a bancada.

## Receptor de snapshots — FCPU v1

O receptor final fica em `PayloadReceiver`/`PayloadReceiverEsp32`, separado
do beta. O [contrato FCPU v1](../../common/protocol/payload-uart.md) preserva
o JSON e define o controle externo. `obc` compila os novos módulos e exclui
o beta; seu `main.cpp` ainda não inicia a recepção. Não há integração com
SD/downlink ou alteração de `payload/telemetry.py`.

O contexto possui dois blocos internos no heap, reservados uma única vez por
`Init`: quadro de 65562 bytes e snapshot de 64672 bytes. Verificar o retorno
de `Init`. Contexto e acesso ao snapshot devem permanecer estáveis na mesma
tarefa. A integração chama `Poll()` ao menos a cada 10 ms e consome READY
antes da próxima chamada. Não usar UART2 pelo beta e pelo receptor ao mesmo
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

Requer somente ESP e USB, sem jumper. `int006-check` reserva boot real em
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

Os relatórios temporários completos são `/tmp/fcp01-int006-bench.json` e
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
