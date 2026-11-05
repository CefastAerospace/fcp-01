# DAD-003 — Codificação e interpretação dos registros

**Estado:** Em andamento
**Responsáveis:** OBC-DH

## Objetivo

Produzir e interpretar registros reprodutíveis de missão, tempo, eventos e estatísticas.

## Entrega

Implementar codificação/decodificação conforme o formato acordado, com identificação de registros incompletos ou inválidos. Conservar referências de tempo e boots sem reescrever dados antigos. Documentar extração e interpretação; reutilizar ferramenta de análise existente quando aplicável. I/O físico fica em [COM-005](../01-comunicacao/COM-005-armazenamento-sd.md) e o fluxo conectado em [INT-006](../03-integracao/INT-006-fluxo-de-dados.md).

## Pré-requisitos

- **Tarefas:** [DAD-001](DAD-001-modelos-e-formatos.md), [DAD-002](done/DAD-002-tempo-e-boots.md)
- **Condições externas:** Contratos definidos nas tarefas anteriores; não requer hardware.

## Definições e condições

- Definir contagem/identificação de boots e estados necessários à retomada, incluindo Burn Wire e referências temporais, sem reescrever dados já gravados.
- Revisão de 05/11/2026: DAD-001 acrescentou modelos em memória para snapshots JSON completos do Payload com extras. O codec/leitor v1 foi preservado e não representa esses tipos. Definir persistência dos snapshots, preservação de extras e identificação dos limites de cada lista (inclusive vazia), com versionamento explícito se alterar o layout. Não fabricar sessão/sequência ou atualizações individuais a partir de `id`. A implementação dessa extensão não ocorreu em DAD-001.
- Extensão posterior de 05/11/2026: persistência e leitura dos snapshots implementadas em formato v2 próprio, conforme a entrega abaixo. A definição anterior descreve a lacuna identificada após DAD-001; permanecem pendentes a consolidação dos produtores e os estados duráveis de retomada, não o codec dos snapshots.

## Critérios de conclusão

Registros e interpretação disponíveis, unidades e versões documentadas, tratamento de limites implementado e verificação local reprodutível. Os cenários detalhados estão em [TES-007](../04-testes/TES-007-dados-e-tempo.md).

## Evidências

Implementação local verificada em 01/11/2026, sobre a base Git `e734ebb` e os trabalhos locais de DAD-001/DAD-002. Alterações ainda não commitadas. A tarefa permanece **Em andamento**, sem antecipar o aceite de DAD-001 ou a definição dos estados duráveis de retomada do Burn Wire.

### Entrega local

- [API portátil](../../headers/ObcRecordCodec.h) e [codec](../../src/ObcRecordCodec.cpp): sete tipos do contrato v1, união discriminada em RAM, CRC-32/ISO-HDLC, serialização explícita e saídas publicadas somente em sucesso. Sem I/O, alocação dinâmica, dependência nova ou alteração dos modelos/validadores existentes.
- [Leitor C++](../../tools/ReadRecords.cpp) e [guia de uso](../../tools/README.md): mesmo decoder do firmware, janela de 512 bytes, descarte relatado por offsets e busca de registros íntegros posteriores. Não corrige bytes, não solicita reenvio e não modifica a origem.
- Duas passagens para reconstrução temporal: referências válidas por boot, última anterior/simultânea ou primeira para reconstrução retroativa; empates resolvidos pela última ocorrência no arquivo. Preserva timestamp do Payload, ts da TM e compensação gravada em cada referência, acrescentando horário estimado/estado/proveniência à exportação JSON Lines.
- Arquivo de entrada deve ser uma cópia estável e seekable de um único OBC. Memória de referências cresce com sua quantidade, não com todos os registros de missão. UTF-8 inválido em descrição é exportado como bytes hex com indicação explícita; não vira JSON inválido nem descarte binário.
- Registro do Burn Wire limitado aos eventos já disponíveis. Não define autorização, confirmação física, estado durável ou retomada de atuação após reset.

### Procedimento e resultados

Executado dentro de `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcData.cpp src/ObcRecordCodec.cpp tests/ObcRecordCodecCheck.cpp -o /tmp/fcp01-codec-check
/tmp/fcp01-codec-check
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp tools/ReadRecords.cpp -o /tmp/fcp01-read-records
python3 tests/ReadRecordsCheck.py /tmp/fcp01-read-records
g++ -std=c++11 -Wall -Wextra -Werror -Iheaders src/ObcData.cpp tests/ObcDataCheck.cpp -o /tmp/fcp01-data-check
/tmp/fcp01-data-check
g++ -std=c++11 -Wall -Wextra -Werror -Iheaders src/ObcTime.cpp tests/ObcTimeCheck.cpp -o /tmp/fcp01-time-check
/tmp/fcp01-time-check
g++ -std=c++11 -Wall -Wextra -Werror tests/I2cSchedulingCheck.cpp -o /tmp/fcp01-i2c-check
/tmp/fcp01-i2c-check
pio run -e obc
```

Todos os comandos terminaram com código zero. O teste do leitor invoca deliberadamente casos de arquivo inválido para verificar os códigos 1/2 e só termina com zero quando os resultados esperados são observados. Não houve diagnóstico de comportamento indefinido pelo UBSan nos dois executáveis nativos novos.

- [Teste do codec](../../tests/ObcRecordCodecCheck.cpp): sete vetores completos e tamanhos 105/99/96/96/84/40/36 comparados byte a byte com entradas independentes produzidas por `struct`/`zlib` da biblioteca padrão Python. CRC de `123456789` observado = `0xCBF43926`; vazio = zero. Não depende somente de um round-trip do próprio codec.
- Cada prefixo truncado dos sete registros retornou `INCOMPLETE`, contagem zero e saída preservada. Todas as capacidades menores que o registro retornaram `BUFFER_TOO_SMALL`, sem modificar o buffer. Conferidos argumentos nulos, assinatura, versão/tipo, comprimento, CRC e conteúdo inválidos, incluindo campo NaN disponível com CRC recalculado.
- Verificados buffer desalinhado, bytes posteriores preservados, registros consecutivos, limite ICAO de 24 bits, sessão/sequência máximas, RSSI nos extremos de int16, ausência com lixo em RAM, zero negativo de float/double, descrição de 64 bytes e padding zerado. Modelo de origem não foi modificado pelo encoder.
- [Teste do leitor](../../tests/ReadRecordsCheck.py): sete layouts lidos de entradas independentes; JSON real conferido, offsets observados 0/105/204/300/396/480/520 e referência final usada retroativamente nos registros anteriores. Horário do Payload e ts da TM preservados. Origem comparada byte a byte após cada execução.
- Recuperação observada: assinatura cruzando fronteira de 512 bytes, lixo com 2000 bytes, CRC incorreto, cabeçalho com comprimento falso, versão/tipo não suportado e conteúdo inválido não esconderam os registros íntegros seguintes. Assinatura dentro de corpo íntegro não criou registro adicional. Cauda truncada foi relatada; arquivo vazio aceito.
- Referências sucessivas fora da ordem de instante e empates produziram os resultados esperados. Para eventos em 100/1000/1800/2500/3000 ms, com as referências do ensaio, foram observados 999110/1000010/1000810/3000530/3001030 ms Unix. Referências de outro boot não foram usadas. Ausência e resultado fora da faixa emitiram `null` com estado explícito, sem inventar intervalo desligado.
- Conferidos inteiros de 64 bits na exportação, precisão de float, escapes JSON, UTF-8 válido/inválido, preservação hexadecimal de bytes e zero negativo. Uso inválido/arquivo ausente retornaram 2; corrupção recuperável retornou 1; leitura íntegra retornou 0.
- Testes anteriores de modelos, tempo e I2C recompilados e aprovados, sem mudar suas entradas/asserts ou resultados esperados.
- Build ESP32 aprovado: PlatformIO 6.2.0, Espressif32 6.6.0, Arduino-ESP32 2.0.14, Xtensa 8.4.0+2021r2-patch5. Codec compilou sem avisos. Permanece o aviso externo já conhecido em `esp32-hal-uart.c:153`, retorno sem valor em `uartSetPins`, que retorna bool; dependência não alterada.

SHA-256 dos artefatos verificados (caminhos relativos à raiz do repositório):

```text
9c278b1352fc4c2ea89ca3fe11f7f9a5995cef4113b92c8e64a7e04da4cc990f  obc-dh/headers/ObcRecordCodec.h
e48dad3f0a665a6d65f73d35dde2473102d7295bbfe41d4266e92443ce6374c8  obc-dh/src/ObcRecordCodec.cpp
7cda765b85cfd37dd7d00738d6bb0443200da4b0f41011a40ee445ef2fa2e4df  obc-dh/tools/ReadRecords.h
14eeee41baa5ba5c96ed0b119bfb736da6154bb53fd9439f8421405e13c01d2e  obc-dh/tools/ReadRecords.cpp
124883bb44ea2ab60f6e9b72b177eeb892d22fef80257fac14de086f3273289f  obc-dh/tests/ObcRecordCodecCheck.cpp
2f78b65f7840334c542a5105e8d8486b02ed2127541185412d2b0589117d16a6  obc-dh/tests/ReadRecordsCheck.py
```

### Pendências e limites

- Consolidar DAD-001 com os produtores. O contrato v1 proposto e os artefatos da entrega anterior foram preservados; o guia do codec documenta a nova implementação sem alterar o layout.
- Revisão documental posterior de 01/11/2026: README da raiz/OBC, guias e texto de estado do contrato foram atualizados para refletir a implementação local. A alteração de hash do documento, sem mudança de layout ou exemplos, está registrada em DAD-001; os seis artefatos de código/teste desta entrega não mudaram.
- Resolver explicitamente a exigência de estados duráveis de retomada, inclusive Burn Wire, com os responsáveis. Eventos são evidência de ocorrência registrada, não autorização nem confirmação física de atuação.
- Sem upload ou execução física, sem I/O SD, sem recuperação de FAT32, sem extração pelo umbilical e sem integração operacional. COM-005/INT-006/INT-008 conservam suas responsabilidades.
- Não concluir automaticamente TES-007 ou ensaios físicos por estes resultados locais. Não mover DAD-003 para `done/` enquanto as pendências de seu escopo não estiverem resolvidas.

### Persistência de snapshots v2 — 05/11/2026

Implementação local sobre a base Git `28504b1` e os trabalhos anteriores de DAD-001/COM-005/INT-003. O responsável escolheu um registro variável por snapshot, com buffer do chamador, preservando o v1. Sem commit, upload ou ensaio físico. DAD-003 permanece **Em andamento** até consolidação de DAD-001 e definição dos estados duráveis de retomada, inclusive Burn Wire. Não foi movida para `done/`.

**Entrega:** `GetSnapshotSize`, `EncodeSnapshot` e `DecodeSnapshot` no módulo existente, reutilizando validadores, resultados, primitivas little-endian/f64 e CRC-32. O [contrato](../../../common/protocol/obc-data.md#persistência-de-snapshots--v2) define `FCP2`, versão 2/tipo 8, cabeçalho de 24 bytes, `id` u64, contagem/lista de aeronaves, campos opcionais e extras com comprimento u8 e bytes sem NUL/padding. Registro vazio = 37 bytes; somente ICAO = 42; máximo = 64497. O comprimento u16 é suficiente. A união/tipos v1 e seu limite de 512 bytes não foram ampliados.

Codificação valida modelo/capacidade antes da primeira escrita e copia somente uma aeronave temporária. Decodificação verifica CRC e conteúdo em primeira passagem, depois publica/normaliza em segunda passagem sobre a mesma entrada estável, sem outro snapshot completo. Campos/slots ausentes são zerados. Fonte/destino não podem se sobrepor. Não há alocação dinâmica ou I/O no codec, nem novos buffers reservados pelo firmware central. Modelo de 64672 bytes e buffer de até 64497 são responsabilidade da integração futura, fora da pilha padrão de tarefas.

**Leitor:** mesma ferramenta e CLI, agora buscando `FCP1`/`FCP2` em janela de 64497 bytes. V1/v2/mistos são aceitos. Reutiliza reconstrução temporal e referências v1 por boot; exporta um JSON Lines por snapshot, com `type: "payload_snapshot"`, `version: 2`, `data.id` e `data.aircraft`. Preserva `tick`, ausência, zero negativo, extras e inteiros de 64 bits. A política de descrição não UTF-8 foi consolidada em helper compartilhado: campos textuais dos snapshots também recebem indicação/hex sem perder bytes ou gerar JSON inválido. Exportação v1 e seus códigos de saída foram preservados.

**Procedimento**, executado em `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcData.cpp src/ObcRecordCodec.cpp tests/SnapshotCodecCheck.cpp -o /tmp/fcp01-snapshot-codec-check
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp tools/ReadRecords.cpp -o /tmp/fcp01-read-records
python3 tests/SnapshotRecordsCheck.py /tmp/fcp01-snapshot-codec-check /tmp/fcp01-read-records
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcData.cpp tests/ObcDataCheck.cpp -o /tmp/fcp01-data-check
/tmp/fcp01-data-check
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcData.cpp src/ObcRecordCodec.cpp tests/ObcRecordCodecCheck.cpp -o /tmp/fcp01-codec-check
/tmp/fcp01-codec-check
python3 tests/ReadRecordsCheck.py /tmp/fcp01-read-records
pio run -e obc
```

Todos os comandos terminaram com código zero, sem diagnóstico do UBSan. O novo script usa apenas `struct`, `zlib` e demais recursos da biblioteca padrão Python; gera entradas temporárias e invoca os executáveis C++ reais. Resultados:

- `Snapshot v2: vetores independentes, limites e saidas preservadas aprovados.`
- `Snapshot v2: leitor misto, referencias, recuperacao e preservacao da origem aprovados.`
- `Leitor C++: layouts, recuperação, JSON, referências e preservação da origem aprovados.`

Os quatro vetores independentes representam lista vazia, ICAO isolado, amostra com opcionais/extras e limite máximo de 20 aeronaves × 32 extras. Foram comparados byte a byte, incluindo texto UTF-8/escapes e f64 −0.0. Conferidos buffers desalinhados, origem preservada, bytes posteriores intactos, ausência com lixo em RAM e normalização. Vetores curtos exercitam todos os prefixos/capacidades menores; o máximo exercita fronteiras sem repetir comparações redundantes de gigabytes. Falhas mantêm toda a saída e contagem zero.

CRC válido não contornou validação de boot, contagens, ICAOs/chaves duplicados, máscara, par de coordenadas, NaN/Inf, limites de strings/extras, NUL embutido ou corpo com bytes sobrando. O leitor recuperou registros posteriores sem saltar comprimentos inválidos, preservou a origem byte a byte, detectou caudas truncadas e assinatura cruzando a janela. Assinaturas dentro de registro íntegro não criaram registros adicionais. `id` repetido conservou ambos os snapshots.

Arquivo misto de snapshot/boot/snapshot/referência/snapshot conservou ordem e offsets. Referência v1 do boot 7 em 1000 ms, Unix 100 s e compensação 162 ms produziu 104162 ms Unix para snapshot em 5000 ms e 99162 ms para boot em zero. Outros boots ficaram sem referência. Textos não UTF-8 de identificação, `tick`, chave/valor de extras foram preservados em hex sem descartar o registro; strings vazias disponíveis continuaram distintas de ausência.

Testes anteriores de modelos, codec e leitor passaram sem editar seus arquivos, entradas/asserts ou resultados esperados. Novos arquivos de teste: [SnapshotCodecCheck.cpp](../../tests/SnapshotCodecCheck.cpp) e [SnapshotRecordsCheck.py](../../tests/SnapshotRecordsCheck.py). Sem alterações em modelos/validadores, serviço SD, inicialização central, UART, TT&C ou atuadores.

Build incremental ESP32 aprovado: PlatformIO 6.2.0, Espressif32 6.6.0, Arduino-ESP32 2.0.14, Xtensa 8.4.0+2021r2-patch5. Codec novo compilou sem avisos. RAM estática 23776/327680 bytes; flash 347429/1310720 bytes. Os novos codecs não são chamados pelo boot e o buffer máximo/modelo de snapshot não estão reservados no firmware; esses valores não demonstram consumo do fluxo integrado.

Verificação adicional de pilha, executada em `obc-dh/`:

```bash
/home/o-grande-kayky/.local/share/platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++ -std=c++11 -Os -Wall -Wextra -Werror -fstack-usage -Iheaders -c src/ObcRecordCodec.cpp -o /tmp/fcp01-snapshot-codec-esp32.o
```

O relatório `/tmp/fcp01-snapshot-codec-esp32.su` informou quadros individuais de 3280 bytes para `WriteAircraft`, 3408 para `ReadSnapshotBody`, 64 para `GetSnapshotSize`, 80 para `EncodeSnapshot` e 96 para `DecodeSnapshot`. Confirma ausência de outra cópia integral do snapshot no quadro dessas funções; não é medição do máximo total de pilha ou de execução física. A integração deverá provisionar a pilha dos consumidores além do modelo e buffer externos.

SHA-256 desta extensão; os hashes de 01/11 acima continuam identificando a entrega histórica, não os fontes estendidos:

```text
74ab002547d39d272bd430afa8f003241c5066f65032c4c41d90392025cb0a2d  obc-dh/headers/ObcRecordCodec.h
1d473b1bb13b404b428df510bd779aee45889196a29144a9821c67bfd3f4d8ce  obc-dh/src/ObcRecordCodec.cpp
b32a60a978c20622181e3fe18b657b0db0ec7242e684a62f57d138ff0f4d170c  obc-dh/tools/ReadRecords.h
4fbf7d3f0e1e23a50ab568836419b0d7b45ce7f80d74889b2f5c4bea5e07f1b7  obc-dh/tools/ReadRecords.cpp
f7ad752ea9e11f1ee88f6027a528bc93eb3f28ad4343e793f09f26284e022344  obc-dh/tests/SnapshotCodecCheck.cpp
53ee717e67a5d60f9a06b7892751b3512d3bb729f4292db4e7b6a95839e66c40  obc-dh/tests/SnapshotRecordsCheck.py
dee7938e8a5c0ee5cf7dc4f3201e27cb6c90668efb5e576ea55546ebb4b81bd5  common/protocol/obc-data.md
```

**Pendências atuais:** consolidação dos contratos/aceites de DAD-001 e definição dos estados duráveis de retomada com os responsáveis. Persistência dos snapshots e extras já tem codec e leitor localmente verificados. Recepção UART permanece em COM-004; ligação de registros ao SD e extração pelo umbilical em INT-006. Nada nesta entrega comprova recuperação física de FAT32, requisito por mensagem ADS-B ou atuação/rearme de Burn Wire. Não encerrar automaticamente TES-007 ou os ensaios físicos.

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-OBC-009, REQ-OBC-010, REQ-NF-OBC-002, REQ-NF-OBC-003, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-REGISTROS.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
