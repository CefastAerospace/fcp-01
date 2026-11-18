# INT-005 — Integração ADCS, encoder e FOC

**Estado:** Em andamento — aquisição AS5600, BH1750 e MPU9250 integrada em bancada. Pendem contratos operacionais, consumo pelo ADCS e FOC.
**Responsável pela implementação OBC:** OBC-DH
**Responsável pelos algoritmos e limites:** ADCS

## AS5600 em bancada — 18/11/2026

`int005-as5600-check` inicia um produtor de bancada depois do boot do
executor I2C. Cada pedido seleciona `RAW ANGLE` (`0x0C`) no AS5600 (`0x36`),
lê dois bytes com repeated START e termina com STOP dentro do mesmo
callback. A contagem bruta de 12 bits vai de 0 a 4095. A seleção do
registrador é a única escrita no dispositivo neste perfil.

A bancada consulta o encoder a cada 1 s, com prioridade de pedido 0 e
espera na fila de 100 ms. A amostra em RAM fica indisponível em caso de
falha, idade acima de 3 s ou recuperação do barramento. Uma nova leitura
bem-sucedida na geração atual restabelece a disponibilidade. O diagnóstico
`[AS5600]` sai pela UART0 bufferizada e publica `raw_count=null` enquanto a
amostra estiver indisponível. Esses parâmetros servem à bancada. A
cadência para FOC depende do contrato com ADCS.

A classificação dos retornos de Wire foi movida para `i2c::MapWireFailure`.
`obc_eps::MapWireFailure` delega à função comum e conserva o contrato
consumido por INA219 e LM75A.

### Verificação de desenvolvimento

Executar em `obc-dh/`. Usar destinos novos para os relatórios USB.

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Itests/as5600_native -Iheaders \
    src/ObcAs5600.cpp tests/ObcAs5600Check.cpp -o /tmp/fcp01-as5600-check
/tmp/fcp01-as5600-check
pio run -e obc -e int005-as5600-check
pio run -e int005-as5600-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStationCheck.py /dev/ttyUSB0 --report /tmp/as5600-usb-novo.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStationCheck.py /dev/ttyUSB0 --report /tmp/as5600-obc-novo.json
```

| Recorte | Resultado |
|---|---|
| Nativo com ASan/UBSan | AS5600 aprovado com limites da contagem, ordem dos bytes, NACK, falha de buffer, leitura curta, STOP, idade e geração de recuperação. Regressões INA219 normal e com macro `EPS`, além da LM75A, aprovadas com os vetores originais. |
| [Perfil AS5600 por USB](../../examples/int005-as5600-2026-11-18/01-as5600-usb.json) | ESP32 sem encoder. Seis diagnósticos `READ_INCOMPLETE` com `raw_count=null` e executor pronto. Roteiro original aprovado com 64 TCs, 37 TMs, `q` máximo de 1 ms, RTT de 111,7 ms e poll UART de 2 ms. |
| [`obc` restaurado](../../examples/int005-as5600-2026-11-18/02-obc-restaurado.json) | Roteiro original aprovado com 64 TCs, 37 TMs, `q` máximo de 117 ms, RTT de 156,4 ms, poll UART de 2 ms e `no_sd` esperado. |

Builds `obc` e `int005-as5600-check` aprovados com Espressif32 6.6.0 sobre
`55fbdc0` com as demais alterações locais presentes. SHA-256 dos binários:

- `int005-as5600-check`: `fcf40a10660e44ccee714d222351ddb0e497938947cb43069db172260cef6a84`
- `obc`: `afc3b17ac169ca61727d4d34810780222a0cfad834be156d1ee45f0c4995026f`

As contagens válidas foram exercitadas no computador com registradores
simulados. A medição com encoder físico pertence aos ensaios existentes.
IMU, uso da luminosidade no ADCS, orientação, fases, condições de atuação e
limites de latência do FOC seguem para implementação conjunta com ADCS.

## BH1750 em bancada — 18/11/2026

`int005-bh1750-check` usa o executor I2C para os canais `0x23` e `0x5C`.
Cada configuração envia `0x01`, `0x42`, `0x65` e `0x10`, um comando por
transação com STOP. O primeiro erro encerra a configuração do canal. A
sequência aplica MTreg nominal 69 e alta resolução contínua. Cada leitura
obtém dois bytes, MSB primeiro, e converte a contagem em lux fora do callback
com `bruto / 1,2`.

Uma tarefa atende ambos os canais em rodadas de 1 s, com prioridade de
pedido 0 e espera na fila de 100 ms. A leitura exige pelo menos 200 ms após
a configuração. Uma falha torna o canal indisponível e exige nova
configuração na rodada seguinte. Cada canal tem validade própria por 3 s.
Uma mudança de geração I2C invalida as amostras antigas e exige
reconfiguração antes da leitura. A guarda dentro do callback também
descarta uma leitura enfileirada para a geração anterior.

O diagnóstico `[BH1750]` publica lux e estado por endereço na UART0
bufferizada. Zero lux é uma leitura válida. Canal indisponível publica
`lux=null`. O recorte entrega a aquisição de bancada. A orientação dos
sensores e o significado da diferença de lux para o apontamento solar
dependem da integração com ADCS.

### Verificação de desenvolvimento

Executar em `obc-dh/` com o ESP32 somente por USB. Usar a porta detectada e
destinos novos para os relatórios.

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Itests/bh1750_native -Iheaders \
    src/ObcBh1750.cpp tests/ObcBh1750Check.cpp -o /tmp/fcp01-bh1750-check
/tmp/fcp01-bh1750-check
pio run -e obc -e int005-as5600-check -e int005-bh1750-check
pio run -e int005-bh1750-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStationCheck.py /dev/ttyUSB0 --report /tmp/bh1750-usb-novo.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStationCheck.py /dev/ttyUSB0 --report /tmp/bh1750-obc-novo.json
```

| Recorte | Resultado |
|---|---|
| Nativo com ASan/UBSan | BH1750 aprovado com comandos, STOP, ordem dos bytes, zero, conversão em lux, NACK, falha de buffer, leitura curta, falha isolada, primeira conversão, expiração e reconfiguração. Regressão AS5600 aprovada. |
| [Perfil BH1750 por USB](../../examples/int005-bh1750-2026-11-18/01-bh1750-usb.json) | ESP32 sem sensores. Seis linhas por endereço com `DEVICE_NACK`, `lux=null` e executor pronto. Roteiro original aprovado com 64 TCs, 37 TMs, `q` máximo de 1 ms, RTT máximo de 105,4 ms e poll de 2 ms. |
| [`obc` restaurado](../../examples/int005-bh1750-2026-11-18/02-obc-restaurado.json) | Roteiro original aprovado com 64 TCs, 37 TMs, `q` máximo de 117 ms, RTT máximo de 156,3 ms, poll de 2 ms e `no_sd` esperado. |

Os três builds passaram em Espressif32 6.6.0 sobre `55fbdc0` com as demais
alterações locais presentes. SHA-256 dos binários gravados:

- `int005-bh1750-check`: `e82a4335bd465b700b3a7be1c5700a314597ec8b7a41ba979f60053069fd4502`
- `obc`: `82f78d6ada11ac246bf81e96bac31ae6847d6cd8fc247513db65d51744084e59`

As leituras válidas de lux, a falha isolada e a recuperação foram
exercitadas no computador. A comprovação física permanece nas TES-003,
TES-008 e TES-009.

## MPU9250 em bancada — 18/11/2026

`int005-mpu9250-check` inicia a aquisição de bancada pelo boot do OBC.
O executor I2C atende no máximo um pedido por rodada de 1 s, com prioridade
0 e espera na fila de 100 ms. O primeiro pedido confirma `WHO_AM_I=0x71`
no endereço `0x68` antes de resetar o dispositivo. O produtor espera pelo
menos 100 ms após o reset para acordar o sensor e 100 ms após acordá-lo
para configurar faixas, filtros e divisor. A primeira leitura exige
200 ms após a configuração. Essas esperas ocorrem entre pedidos ao executor.

Cada leitura obtém 14 bytes contíguos a partir de `0x3B`, com repeated START
e STOP ao final. Os três eixos do acelerômetro usam ±2 g e fator de
16.384 contagens/g. Os três do giroscópio usam ±250 °/s e fator de
131 contagens/(°/s). Uma falha inicia nova identificação. A disponibilidade
exige leitura válida na geração I2C atual, serviço `READY` e idade de até
3 s. O diagnóstico `[MPU9250]` publica os eixos exclusivamente pela UART0
bufferizada neste perfil. Publica `null` nos vetores quando indisponíveis.

### Verificação de desenvolvimento

Executar em `obc-dh/` com o ESP32 somente por USB e Python com `pyserial`.
Usar a porta detectada e destinos novos para os relatórios.

```bash
g++ -std=c++11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -Itests/mpu9250_native -Iheaders \
    src/ObcMpu9250.cpp tests/ObcMpu9250Check.cpp -o /tmp/fcp01-mpu9250-check
/tmp/fcp01-mpu9250-check
pio run -e obc -e int005-as5600-check -e int005-bh1750-check -e int005-mpu9250-check
pio run -e int005-mpu9250-check -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStationCheck.py /dev/ttyUSB0 --report /tmp/mpu9250-usb-novo.json
pio run -e obc -t upload --upload-port /dev/ttyUSB0
python3 tests/ObcStationCheck.py /dev/ttyUSB0 --report /tmp/mpu9250-obc-novo.json
```

| Recorte | Resultado |
|---|---|
| Nativo com ASan/UBSan | MPU9250 aprovado com identidade, sequência de configuração, ordem dos bytes, STOP, sinal, escala, falha de transporte, preservação da saída, expiração e recuperação. Regressões AS5600 e BH1750 aprovadas. |
| [Perfil MPU9250 por USB](../../examples/int005-mpu9250-2026-11-18/01-mpu9250-usb.json) | ESP32 sem IMU. Seis diagnósticos `READ_INCOMPLETE` com identidade e eixos `null`, `available=0` e executor pronto. Roteiro original aprovado com 64 TCs, 37 TMs, `q` máximo de 2 ms, RTT máximo de 113,9 ms e poll UART de 3 ms. |
| [`obc` restaurado](../../examples/int005-mpu9250-2026-11-18/02-obc-restaurado.json) | Roteiro original aprovado com 64 TCs, 37 TMs, `q` máximo de 117 ms, RTT máximo de 156,5 ms, poll UART de 2 ms e `no_sd` esperado. O log não contém diagnósticos do MPU9250. |

Os quatro builds passaram em Espressif32 6.6.0 sobre `55fbdc0` com as demais
alterações locais presentes. Identificação dos binários gravados por SHA-256.

- `int005-mpu9250-check` — `bce42bf393e8177ba8b8db67d2b630a7bb6f5ec014bc16fe1baa1fd7bcc8a829`
- `obc` — `d511bdd98aea8a06361a637134f98c6475011de253f041a2910890c2c7eb08ad`

Após a execução USB, o ajuste de formatação de duas declarações no header
passou na regressão nativa e na recompilação. O SHA-256 de `obc` coincidiu
com o gravado. O perfil MPU9250 recompilado gerou
`b6580f426c99a5a47e9d966ac511bd5d801269d140012c3eb387131d1ce51dc5`.

As leituras válidas dos seis eixos foram verificadas com registradores
simulados. A comprovação física cabe às TES-003, TES-008 e TES-009.
Calibração, orientação, eixo da TM `w`, cadência operacional e FOC dependem
do contrato com ADCS.

## Objetivo

Integrar atitude, luminosidade, encoder e controle ADCS no ESP32 central, preservando algoritmos e acesso coordenado ao I2C.

## Redistribuição de 14/11/2026

O escopo anterior incluía aquisição EPS/Térmico. Essa entrega e seus encaminhamentos DAD-001/COM-003 passam à [INT-009](INT-009-aquisicao-eps-termico.md). Esta INT conserva os consumidores ADCS e a conciliação da medição solar com a INT-009. Nenhuma integração foi implementada ou encerrada por esta revisão.

## Entrega

Reutilizar drivers e algoritmos ADCS; encaminhar consumidores I2C pelo serviço. Integrar aquisição de atitude, encoder e luminosidade, comandos, estado e falhas no firmware central. Não duplicar setup()/loop() nem reimplementar algoritmos. Preservar atendimento FOC independente de operações demoradas de rádio/SD. Integrar inicialização e falhas ao boot existente.

## Pré-requisitos

- **Tarefas/capacidades:** COM-001/002/003, INT-002 e coordenação de boot da INT-003. A capacidade de bancada já implementada pode ser usada antes do fechamento do boot de voo; dependências elétricas para atuação permanecem obrigatórias.
- **Condições externas:** inventário/montagem ADCS, aquisição, orientação, comandos, status, condições de atuação e limites FOC/I2C acordados. EPS/Térmico fornecem seus dados na INT-009; não bloquear aquisição ADCS independente por implementação desses produtores.

## Definições e condições

- A bancada ADCS ainda usa `Wire` diretamente. Integrar todos os consumidores ADCS ao executor e coordenar sensores/encoder sem transações intercaladas. Timeout de transferência não limita um callback completo nem recupera sozinho o barramento.
- Conferir MPU9250 `0x68` com AD0 em GND; BH1750 #1 `0x23` com ADDR em GND e #2 `0x5C` com ADDR em VCC. Confirmar orientação e parâmetros de aquisição com ADCS.
- Encoder AS5600/I2C `0x36` está definido e usado no código ADCS; conferir montagem/aquisição coordenada.
- Execução no mesmo ESP32 confirmada. Integrar firmware sem duplicar entradas; conferir o papel da interface web na bancada e na missão.
- Conciliar fases 27/26/25 do ADCS com U/V/W = 25/26/27 do OBC. A revisão documental não altera código de fases.
- Definir frequência/latência FOC, aquisição e missão, prioridades/afinidade, sincronização I2C e atuação por modo. Rádio/SD não podem impedir atendimento do controle.
- Conciliar medição solar ADC nos PDFs versus sensores digitais com ADCS/EPS na INT-009; não reservar ADC silenciosamente.

## Encaminhamentos recebidos

- **DAD-001:** acordar mapeamento, unidades, disponibilidade, origens/códigos e dados ADCS. Campos de energia/temperatura e posições térmicas passam à INT-009.
- **COM-003:** acordar frequência, latência e efeito da recuperação nos consumidores/encoder ADCS. Mapear erros em `ExecuteChecked`, prazos por pedido, conclusão de transações antes do retorno e reconfiguração após recuperação. A supervisão do serviço fica na INT-008; o caso de driver sem retorno, na INT-012.

## Verificação de desenvolvimento

OBC mantém regressões para acesso exclusivo, encaminhamento de resultados/falhas, dados inválidos, disponibilidade e reconfiguração. Preservar algoritmos ADCS e verificar build integrado, sem interpretar mocks como medição FOC.

## Critérios de conclusão

Consumidores ADCS integrados e acesso a Wire coordenado, contratos necessários documentados, build e regressões pertinentes aprovados. Registrar versões, parâmetros, estados e falhas. Comprovação física de sensores/FOC e coexistência com rádio/SD pertence às TES-003/008/009, sem condicionar o encerramento desta implementação à aprovação desses ensaios.

## Evidências

Os recortes de [AS5600](#as5600-em-bancada--18112026),
[BH1750](#bh1750-em-bancada--18112026) e
[MPU9250](#mpu9250-em-bancada--18112026) registram as evidências de
desenvolvimento disponíveis. As demais entregas ainda precisam dos seus
contratos, builds e regressões.

## Referências

- Requisitos relacionados: REQ-OBC-003, REQ-OBC-011. Implementação não equivale a requisito validado.
- [ADCS](../../../adcs/README.md), [pinagem](../../.agents/PINAGEM.md), [arquitetura](../../.agents/ARCHITECTURE.md).
- [TES-003](../04-testes/TES-003-i2c-hardware.md), [TES-008](../04-testes/TES-008-integracao.md), [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md).
- Origem no planejamento anterior: P-ADCS, P-BARRAMENTO, P-ENDERECOS, P-EPS-ELETRICA; parcela EPS/Térmico encaminhada à INT-009.
- [Painel](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
