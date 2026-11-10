# DAD-002 — Tempo monotônico, boots e referências absolutas

**Estado:** Concluída
**Responsáveis:** OBC-DH, Payload, TT&C e coordenação

## Situação atual da integração — 10/11/2026

Boot/NVS já é chamado pelo coordenador e `set_time` já aplica referências no fluxo INT-006, inclusive pelo USB. As conexões descritas abaixo como futuras foram feitas posteriormente. Precisão LoRa, limites temporais e persistência sob interrupção continuam nos ensaios correspondentes; lógica simulada e teste USB não comprovam essas propriedades. [Situação da integração](../../03-integracao/INT-006-fluxo-de-dados.md).

## Objetivo

Representar e reconstruir o horário dos dados e eventos sem inventar horário absoluto.

## Entrega

Implementar lógica temporal verificável com entradas simuladas, sem exigir rádio ou SD. Definir identificação/contagem dos boots e persistência inclusive com SD indisponível. A recepção real de set_time e a gravação das referências serão conectadas em [INT-006](../../03-integracao/INT-006-fluxo-de-dados.md).

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Definir precisão, resolução, deriva e identificação persistente dos boots com coordenação, Payload e TT&C.

## Definições e condições

- RTC externo está fora da configuração atual; considerar sua instalação apenas se o método monotônico/LoRa falhar na validação temporal acordada. DS3231 histórico usa `0x68`, que conflita com a IMU; eventual reconsideração exige decisão elétrica sem mudar MPU9250 para `0x69` por suposição. Payload UART não recebe endereço I2C.
- Decisão: ms monotônicos por boot e referências absolutas pelo LoRa via `CMD_SET_TIME`/`set_time`, com reconstrução retroativa sem reescrever registros. RTC externo fica fora da configuração atual e só será reconsiderado se esse método não atender aos critérios temporais acordados; DS3231 permanece histórico.
- `args.unix` e `ts` do contrato TT&C estão em segundos Unix. Definir instante representado/recebido, conversão ms/s, resolução, erro/deriva admissível, periodicidade e validade; considerar quantização do horário transmitido e atraso no enlace. Não alterar silenciosamente o contrato ou inventar precisão.
- Definir identificação dos boots, persistência e contagem sem colisões após reset, inclusive com SD indisponível. Tratar duração, possível estouro do contador monotônico e representação dos tempos nos registros.
- Definir escolha/interpretação de referências sucessivas, ajuste inicial sem internet, análise posterior e indicação de horário desconhecido nos registros e na TM. Não inferir intervalo desligado usando apenas a referência do boot anterior.
- Definir o instante do timestamp OBC e preservar o horário produzido pelo Payload sem remover ou redefinir seu campo. O exemplo TT&C faz ajuste em memória e não comprova recuperação ou precisão.

## Contrato implementado — 01/11/2026

Plano aprovado pelo responsável na sessão de implementação: resolução interna de 1 ms; compensação fixa inicial ajustável; precisão absoluta e deriva física serão medidas na integração, sem impor silenciosamente um máximo de 300 ms.

- `ObcTime.h`/`ObcTime.cpp`: tipos internos `EVENT_STAMP` e `TIME_REFERENCE`, criação da referência, reconstrução explícita e reserva do contador persistente. Não são formatos de arquivo nem exigem serializar memória bruta.
- Fórmula: `unix_ms = unix_s * 1000 + delay_ms + (evento_ms - recepção_ms)`. A compensação é positiva porque o horário enviado representa, por hipótese inicial, o início do TX. Diferenças anteriores à referência são subtraídas sem underflow. Resultados aceitos: zero a `INT64_MAX`; fora da faixa retorna erro.
- Parâmetro global `obc_time::reference_delay_ms = 162`, constante ajustável por recompilação. Origem: 162,048 ms arredondados, para o exemplo de `set_time` de 63 bytes com SF7, BW 125 kHz, CR 4/6, preâmbulo 32, cabeçalho explícito e CRC. Não mede nem compensa filas anteriores ao TX, retransmissões, quantização em segundos ou deriva. O comprimento real pode variar. Uma estimativa dinâmica futura poderá fornecer a compensação da referência; cada referência conserva a compensação que utilizou.
- Sem referência: retorno `NO_REFERENCE`; boot divergente: `BOOT_MISMATCH`; argumentos e limites inválidos possuem estados próprios. Saída numérica só é válida com `OK`. Não inventar UTC zero para representar desconhecido; zero também é uma época Unix válida.
- Referências sucessivas são entradas explícitas. Pode-se reconstruir o mesmo evento com cada referência, sem alterar os dados nem apagar a referência anterior. Não há histórico ilimitado em RAM, sincronização periódica ou compensação automática de deriva. O caller seleciona a referência explicitamente; persistência/interpretação integrada pertence a DAD-003/INT-006.
- `ObcTimeEsp32`: `GetMonotonicMs()` usa `esp_timer_get_time() / 1000` em 64 bits. Na integração, associar o timestamp OBC à recepção validada dos dados; conservar separadamente o timestamp do Payload. Relacionar o `TC_FRAME.rx_ms` de 32 bits ao mesmo domínio monotônico de 64 bits, sem substituir recepção pelo instante posterior de processamento. O código desta tarefa não conecta esses consumidores.
- Boots: NVS padrão, namespace `obc_time`, chave `boot_count`, contador de 64 bits. Ausência da chave inicia em 1; zero persistido ou falhas não são tratados como primeiro boot. O ID só é publicado após `nvs_commit()` bem-sucedido. O adaptador conserva o ID em RAM e não grava novamente após inicialização bem-sucedida no mesmo boot. Inicializar antes dos consumidores, sem concorrência; a API portátil usa callbacks mínimos de leitura/escrita para o ensaio nativo.
- Reinicializações registradas = `boot_id - 1`, considerando reservas confirmadas. Não conta energizações interrompidas antes da reserva. Falhas de persistência deixam ID indisponível; gravação de resultado incerto pode consumir um número sem publicar ID. Esgotamento não causa volta do contador. Não apaga a NVS em erro; apagamento/restauração da partição invalida a continuidade dos identificadores. A referência de um boot não revela o intervalo desligado nem sincroniza outro boot.

## Critérios de conclusão

Lógica e contrato temporal implementados, com verificações locais para ausência de referência, referências sucessivas, resets e estouro do contador. Precisão física continua dependente de medição em [TES-008](../../04-testes/TES-008-integracao.md).

## Evidências

Os relatos, comandos, hashes e resultados a seguir descrevem as revisões datadas de cada entrega. Afirmações de ausência de integração ou de estado ainda em andamento nesse histórico não substituem o encerramento e os encaminhamentos atuais acima.

Verificação executada em 01/11/2026. Base Git: `e734ebb`, com a implementação local desta tarefa ainda não commitada. Versão dos arquivos identificada pelos SHA-256 abaixo:

```text
e9508bb97959b763d4efed0d96faed56f812bb6accf86ddbd7db9317cc5fe502  headers/ObcTime.h
15a3b34fdd8afa8783436a4b780f62865ceeb4062470a4c6b1990769ce9205e2  src/ObcTime.cpp
22b1c2bcf4d115e3ced544b733b704baf495699228f639687a4bad60ccff90b9  headers/ObcTimeEsp32.h
046eb4cb7bc5f630e39f8ced58bc040d7b477738eb693be128be94f83308f6b3  src/ObcTimeEsp32.cpp
077e430d91e3692ef976b274e7a300cdbca6cde7fd4fc1e384d07b5ddcb3e77f  tests/ObcTimeCheck.cpp
```

Executado dentro de `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iheaders src/ObcTime.cpp tests/ObcTimeCheck.cpp -o /tmp/fcp01-time-check
/tmp/fcp01-time-check
g++ -std=c++11 -Wall -Wextra -Werror tests/I2cSchedulingCheck.cpp -o /tmp/fcp01-i2c-check
/tmp/fcp01-i2c-check
pio run -e obc
```

- Teste temporal: compilação e execução com código zero. Com referência `{boot=1, recepção=5000 ms, unix=1000 s, compensação=200 ms}`, eventos em 0/5000/5015 ms resultaram em 995200/1000200/1000215 ms Unix. Os valores de ensaio são independentes da calibração global de voo.
- Ausência, referência de outro boot, argumentos inválidos, referências sucessivas e preservação da compensação passaram. Verificados época Unix zero, resultado anterior à época rejeitado, continuidade além de `UINT32_MAX`, conversão antes de multiplicar, `INT64_MAX` exato e rejeição de resultados superiores.
- Persistência simulada: primeiros IDs 1/2, falhas de leitura/escrita sem publicar ID, recuperação, escrita de resultado incerto sem reutilizar ID publicado e esgotamento em `UINT64_MAX`. Esperados e observados coincidiram em todos os asserts.
- Teste I2C existente: compilação e execução com código zero, sem alterar entradas, asserts ou resultados esperados.
- Build ESP32 aprovado: PlatformIO 6.2.0, Espressif32 6.6.0, Arduino-ESP32 2.0.14, toolchain Xtensa 8.4.0+2021r2-patch5. Os dois módulos novos compilaram. Aviso externo no framework, em `esp32-hal-uart.c:153`: `return` sem valor em `uartSetPins`, que retorna `bool`; dependência não alterada.
- Limites da evidência: sem upload, rádio, SD ou execução na placa. O teste nativo substitui o armazenamento e comprova a lógica de publicação, não a implementação física da NVS. O build não comprova precisão, resistência a corte de alimentação nem boot integrado. `setup()`/`loop()`, UART e TT&C permanecem inalterados. Ensaios de integração/falhas continuam pendentes nas tarefas correspondentes.

## Referências

- Requisitos relacionados: REQ-OBC-006, REQ-OBC-010, REQ-NF-OBC-003. Implementação não equivale a requisito validado.
- [Contrato temporal portátil](../../../headers/ObcTime.h), [implementação](../../../src/ObcTime.cpp) e [teste nativo](../../../tests/ObcTimeCheck.cpp).
- [Adaptador ESP32](../../../headers/ObcTimeEsp32.h) e [implementação NVS/timer](../../../src/ObcTimeEsp32.cpp).
- [obc-dh/.agents/ARCHITECTURE.md](../../../.agents/ARCHITECTURE.md)
- [common/protocol/ttc-json.md](../../../../common/protocol/ttc-json.md)
- [obc-dh/.agents/REQUISITOS.md](../../../.agents/REQUISITOS.md)
- Origem no planejamento anterior: P-ENDERECOS, P-TEMPO.
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
