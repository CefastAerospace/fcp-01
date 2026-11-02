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

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-OBC-009, REQ-OBC-010, REQ-NF-OBC-002, REQ-NF-OBC-003, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-REGISTROS.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
