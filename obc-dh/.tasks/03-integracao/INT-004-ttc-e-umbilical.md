# INT-004 — Integração de TT&C e umbilical

**Estado:** Em andamento — implementação e bancada USB verificadas; aceite TT&C pendente
**Responsáveis:** OBC-DH e TT&C

## Objetivo

Conectar o OBC ao transporte existente de telecomandos e telemetria.

## Entrega

Reutilizar a biblioteca TT&C: Init()/Start(), ReceiveTelecommand(), SendAck(), Send()/SendTelemetry() e UmbilicalLink. Não executar Update() concorrentemente com Start(). Preservar tarefa do enlace no Core 0, prioridade padrão 10, ISR de DIO0 apenas notificando e acesso exclusivo ao rádio. Declarar dependências e fixar versões validadas no ambiente consumidor. A aplicação de set_time pertence a [INT-006](INT-006-fluxo-de-dados.md); ações de missão a [INT-007](INT-007-comandos-modos-e-protecoes.md).

## Pré-requisitos

- **Tarefas:** [INT-003](INT-003-inicializacao-central.md)
- **Condições externas:** Biblioteca TT&C disponível; conferir montagem, versões e contrato com o setor.

## Definições e condições

- Rádio usa `SPI` global; SD exige instância própria HSPI. Parâmetros atuais TT&C: 921 MHz, SF7, BW 125 kHz, CR 4/6, preâmbulo 32, CRC do rádio e TX sob demanda. Preservar decisão TT&C; validar velocidades/coordenação com estação e não duplicar filas.

## Critérios de conclusão

Transporte ligado ao firmware central, TC/ACK e TC/TM de bancada conectados e build aprovado. Preservar ACK pelo meio de origem e os limites de entrega TC ≤200 ms e resposta ≤2 s; comprovação integrada em [TES-008](../04-testes/TES-008-integracao.md).

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [DAD-001](../02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md) | Consolidar com TT&C os mapeamentos e unidades do conteúdo, mantendo o contrato de TC/TM e registrando o aceite das interfaces usadas. | Mapeamento local documentado; aceite conjunto TT&C pendente |

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
unidades estão no [contrato compartilhado](../../../common/protocol/ttc-json.md#disponibilidade-no-firmware-de-bancada-obc).

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
- [tt-c/include/Ttc.h](../../../tt-c/include/Ttc.h)
- [tt-c/include/TtcProtocol.h](../../../tt-c/include/TtcProtocol.h)
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [tt-c/ENSAIOS.md](../../../tt-c/ENSAIOS.md)
- Origem no planejamento anterior: P-SPI.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
