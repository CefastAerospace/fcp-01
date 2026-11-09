# DAD-001 — Modelos de dados e formatos persistidos

**Estado:** Concluída
**Responsáveis:** OBC-DH e produtores dos dados

## Encerramento do escopo próprio — 09/11/2026

Modelos, validadores e contratos locais v1/snapshots estão disponíveis, com exemplos, unidades, disponibilidade e limites explícitos. As verificações nativas registradas e repetidas nesta sessão passaram sem alterar seus vetores.

Por orientação do responsável, esta entrega é encerrada e arquivada em
`done/` no limite de sua responsabilidade. Aceites externos, integração e
ensaios restantes continuam obrigatórios nas destinatárias abaixo; não são
declarados aprovados por este encerramento. Os critérios, vetores, entradas
e resultados esperados originais permanecem preservados.

Menções a “em andamento” ou “não mover para done” no histórico abaixo
registram decisões anteriores a este encerramento. Se uma integração
revelar defeito na entrega, registrar correção ou reabrir a tarefa com
rastreabilidade, sem adaptar a validação para forçar aprovação.

| Destinatária | Responsabilidade ainda aberta | Situação |
|---|---|---|
| [INT-004](../../03-integracao/INT-004-ttc-e-umbilical.md) | Consolidar com TT&C os mapeamentos e unidades do conteúdo, mantendo o contrato de TC/TM e registrando o aceite das interfaces usadas. | Pendente na destinatária |
| [INT-005](../../03-integracao/INT-005-sensores-e-adcs.md) | Obter os aceites de ADCS/EPS/Térmico para mapeamento, unidades, disponibilidade, posições térmicas, origens/códigos e dados dos sensores. | Pendente na destinatária |
| [INT-006](../../03-integracao/INT-006-fluxo-de-dados.md) | Consolidar o aceite de Payload/TT&C e das estatísticas: id opaco, idade/validade dos campos, unidades de velocidades, extras e migração explícita dos dados legados, sem inventar sessão/sequência ou validade. | Pendente na destinatária |

## Objetivo

Definir representações documentadas para dados recebidos e registros armazenados.

## Entrega

Definir tipos concretos, unidades, validade, versão, tamanho máximo, enquadramento e leitura posterior. Cobrir missão, TM, eventos, falhas, estatísticas, reinicializações e referências de tempo. Usar headers/models/ apenas quando houver tipos efetivamente implementados. Não persistir memória bruta de structs nem alterar o contrato TT&C.

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Acordar campos, unidades e limites com Payload, TT&C, ADCS, EPS e Térmico.

## Definições e condições

- Definir status, temperatura, solicitações/respostas, significado de `val_flags`, validade de dados acumulados em instantes diferentes e comportamento de `seq_id` após reset. Manter distinção entre timestamp do Payload e recepção no OBC.
- Decisão de 05/11/2026: snapshots JSON completos diretamente pela UART substituem a decisão anterior de binário compacto. O modelo de atualizações individuais v1 permanece preservado para seu codec/testes; a adaptação de dados novos não pode inventar sessão/sequência ou validade por campo.

## Critérios de conclusão

Contrato e tipos necessários disponíveis, com exemplos e limites explícitos. Formato de transporte UART, representação interna e formato do arquivo têm responsabilidades distintas; nenhuma decisão sobre JSON/binário persistido será presumida.

## Evidências

Implementação local e verificações executadas em 01/11/2026, sobre a base Git `e734ebb` e o trabalho local de DAD-002. Alterações ainda não commitadas. A tarefa permanece **Em andamento**, pois os produtores ainda não deram aceite ao contrato proposto.

### Entrega local

- [Contrato OBC v1 proposto](../../../../common/protocol/obc-data.md), [tipos portáteis](../../../headers/models/ObcData.h), [validadores](../../../src/ObcData.cpp) e [teste nativo](../../../tests/ObcDataCheck.cpp).
- Modelos de atualização de missão, TM local, eventos/falhas, estatísticas e boots; reutilização de `EVENT_STAMP`/`TIME_REFERENCE` de DAD-002. Nenhuma dependência nova, gerador, aquisição, fila, codec ou driver nesta entrega.
- Payload proposto: somente campos atualizados, disponibilidade explícita, par sessão/sequência e preservação separada do horário do Pi. O `val_flags` legado não é reinterpretado; o código atual do Payload não foi alterado. As escolhas foram aprovadas pelo responsável no planejamento; aceite dos demais produtores ainda pendente.
- Arquivo proposto: little-endian, IEEE 754, assinatura FCP1, versão/tipo/comprimento, boot e monotônico de 64 bits, corpo tipado e CRC-32/ISO-HDLC. Total máximo 512 bytes; tipos v1 têm tamanhos fixos de 36 a 105 bytes. CRC e codificação/interpretação serão implementados em DAD-003; nenhum ensaio de arquivo corrompido foi declarado executado aqui.
- Orçamento de armazenamento atualizado: missão v1 = 105 bytes por atualização; a 120/s por 600 s, 7,560 MB, mais provisão auxiliar e TM. Os 64 bytes anteriores eram estimativa, não entrada/resultado esperado de teste. O orçamento de rádio e os critérios de validação existentes foram preservados.

### Procedimento e resultados

Executado dentro de `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iheaders src/ObcData.cpp tests/ObcDataCheck.cpp -o /tmp/fcp01-data-check
/tmp/fcp01-data-check
g++ -std=c++11 -Wall -Wextra -Werror -Iheaders src/ObcTime.cpp tests/ObcTimeCheck.cpp -o /tmp/fcp01-time-check
/tmp/fcp01-time-check
g++ -std=c++11 -Wall -Wextra -Werror tests/I2cSchedulingCheck.cpp -o /tmp/fcp01-i2c-check
/tmp/fcp01-i2c-check
pio run -e obc
```

Todos os comandos terminaram com código zero. Resultados esperados e observados coincidiram:

- Sete tipos aceitos com tamanhos canônicos 105/99/96/96/84/40/36 bytes; versão/tipo desconhecidos, boot zero e comprimentos divergentes rejeitados, inclusive 512/513 quando não correspondem ao tipo.
- Exemplos de posição e velocidade aceitos, sem reutilizar campos antigos. Zero disponível aceito; mensagem sem dado de voo, latitude/longitude isolada e bits reservados rejeitados. Latitude/longitude nos extremos aceitas; fora dos limites, velocidade negativa, rumo 360 e reais disponíveis NaN/Inf rejeitados. Altitude negativa finita aceita, sem alterar o critério legado do produtor.
- Timestamp local opcional separado da referência de voo; datas bissextas válidas aceitas e datas impossíveis/formatos inválidos rejeitados. Texto sem NUL na RAM rejeitado.
- TM documentada aceita, disponibilidade independente das cinco temperaturas verificada e modos/bits desconhecidos rejeitados. Os campos e unidades de TT&C continuam preservados; não houve conversão implícita A/mA.
- Eventos/falhas com origem e severidade válidas aceitos; severidade incompatível, origem inválida e descrição sem terminação rejeitadas. Descrição de 64 bytes aceita. Contadores inteiros máximos aceitos. Relação boot/contagem e coincidência entre cabeçalho/referência temporal verificadas.
- Testes existentes de tempo e I2C recompilados e aprovados, sem alteração de suas entradas/asserts. Oito exemplos JSON do contrato também foram lidos com `json` da biblioteca padrão Python; sete tamanhos de layout foram conferidos com `struct` em little-endian sem padding. Orçamento `600 * 120 * 105 = 7560000` e TM `60 * 99 = 5940` conferidos.
- Build ESP32 aprovado com PlatformIO 6.2.0, Espressif32 6.6.0, Arduino-ESP32 2.0.14 e Xtensa 8.4.0+2021r2-patch5. O módulo novo compilou sem avisos. Permanece o aviso externo já conhecido em `esp32-hal-uart.c:153`, retorno sem valor em `uartSetPins`, que retorna bool; framework não alterado.

SHA-256 dos artefatos verificados (caminhos relativos à raiz do repositório):

```text
4ed4f1cf140bdc186228de3662216cbcf039d94b9a962fda2c4f51efd1607d46  obc-dh/headers/models/ObcData.h
d9096dc7f92751a2f5d34da8584353a0cf06eb8008081f07129e2c2d00c5633d  obc-dh/src/ObcData.cpp
c7fef5a7aab0f3af20259f6d656e78514baad7363b71fed2308f6fadf6be8a98  obc-dh/tests/ObcDataCheck.cpp
d3e12c9f6c89737b22512f498f8a752dbcb5014383d93b460958d56e800ef296  common/protocol/obc-data.md
```

Revisão documental posterior de 01/11/2026: o texto de `common/protocol/obc-data.md` passou a indicar que o codec/leitor de DAD-003 já estão implementados, com links para uso e verificações. Layouts, versão, exemplos e critérios não mudaram. O hash acima identifica a proposta inicial verificada nesta entrega; o hash da revisão documental é `654ce38825eeee13840ed2d7ce0221fadc0af07867fd2b9dff883d1635410f3b`. Os três artefatos de código/teste acima permanecem iguais.

### Pendências registradas em 01/11/2026

- Payload: aceitar campos/unidades, mensagem por atualização, sessão e ciclo da sequência, disponibilidade e migração explícita do modelo legado. Alinhar enquadramento, solicitações/respostas e geração da sessão em COM-004, sem presumir que o layout do SD será o quadro UART.
- TT&C, ADCS, EPS e Térmico: confirmar mapeamento/validade das leituras, unidades, posições térmicas e códigos por origem. A máscara local de TM não modifica o pacote TT&C; adaptação de horário desconhecido no downlink permanece na integração.
- Confirmar as definições das estatísticas com os produtores/consumidores. Não há aceite externo registrado apenas por aprovação deste plano local.

Sem upload ou execução física. Não há evidência de UART, integridade do SD, latência ou interoperabilidade dos setores. `setup()`/`loop()`, módulos temporais/I2C, bibliotecas dos setores e validações existentes foram preservados. Não mover esta tarefa para `done/` antes do aceite e dos critérios completos atendidos.

### Adaptação para snapshots — 05/11/2026

Implementação e verificações sobre a base Git `28504b1`, com alterações locais ainda não commitadas. O responsável aprovou o plano de modelos e contrato, com campos padronizados e extras textuais. A tarefa permanece **Em andamento**: esta entrega local não comprova aceite de Payload/TT&C/ADCS/EPS/Térmico nem encerra seu escopo completo. As pendências de Payload abaixo atualizam as registradas em 01/11; o histórico anterior e seus hashes foram conservados.

**Reutilização verificada:** busca nos módulos OBC, Payload, TT&C, common, ADCS e EPS encontrou `PAYLOAD_DATA`, `ttc::AIRCRAFT_DATA`, `AircraftTelemetry` do produtor Python e `EVENT_STAMP`. Os três primeiros não representam o novo snapshot: atualização individual, downlink e cache legado têm contratos distintos e não comportam os novos extras/ausências. Foram reutilizados `EVENT_STAMP`, `obc_data::STATUS`, os bits de disponibilidade de posição/altitude e o módulo existente `ObcData`, sem dependência nova.

**Adições:** `PAYLOAD_SNAPSHOT`, `AIRCRAFT_STATE`, `EXTRA_FIELD`, máscara `SNAPSHOT_FIELD`, limites fixos e duas sobrecargas de `Validate`. As coleções são arrays sem alocação dinâmica. Validam boot, contagens, ICAO, disponibilidade, domínios numéricos, textos e duplicidade de ICAOs/chaves de extras; não interpretam JSON ou datas. Até 20 aeronaves, 32 extras por aeronave, chaves/valores de 32/64 bytes, identificação de 16 bytes e `tick` de 40 bytes, excluindo NUL. Limites são locais, ainda sujeitos ao aceite do produtor.

O [contrato atualizado](../../../../common/protocol/obc-data.md#0-snapshot-do-payload--modelo-em-memória) documenta o mapeamento para o futuro parser, preservação de textos não decodificados, ausência por máscara, `id` opaco, `tick` original e publicação de snapshots completos. As referências de arquitetura/pinagem/guias, o painel e COM-004 foram sincronizados. DAD-003 registra que os novos modelos ainda não têm formato persistido. Não houve remoção ou substituição dos tipos/validadores anteriores, do layout v1, do código do codec/leitor, do contrato TT&C, do script Payload, de `main.cpp` ou dos testes anteriores. Entradas, asserts, resultados esperados e requisitos permanecem intactos.

**Procedimento de verificação**, executado em `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcData.cpp tests/PayloadSnapshotCheck.cpp -o /tmp/fcp01-snapshot-check
/tmp/fcp01-snapshot-check
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcData.cpp tests/ObcDataCheck.cpp -o /tmp/fcp01-data-check
/tmp/fcp01-data-check
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcData.cpp src/ObcRecordCodec.cpp tests/ObcRecordCodecCheck.cpp -o /tmp/fcp01-codec-check
/tmp/fcp01-codec-check
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcData.cpp src/ObcTime.cpp src/ObcRecordCodec.cpp tools/ReadRecords.cpp -o /tmp/fcp01-read-records-check
python3 tests/ReadRecordsCheck.py /tmp/fcp01-read-records-check
pio run -e obc
```

Todos terminaram com código zero. Não houve diagnóstico do UBSan. O [novo teste nativo](../../../tests/PayloadSnapshotCheck.cpp) constrói manualmente exemplos com coordenadas válidas e textos `not decoded` extraídos da amostra; não é um parser. Verificou ausência versus zero, ICAO sem dado de voo e somente com identificação, coordenadas extremas/inválidas, altitude negativa/Inf, latitude NaN, máscara/par de posição inválidos, limites de strings/extras, terminação NUL, chave vazia/duplicada, distinção de caixa, boot inválido, lista vazia, limite de 20 aeronaves e ICAOs repetidos. Strings disponíveis vazias são permitidas; extras de valor vazio também. Slots/campos indisponíveis são ignorados. O validador não trunca nem publica estado global.

Os testes anteriores de dados/codec/leitor passaram sem edição. O leitor informou `Leitor C++: layouts, recuperação, JSON, referências e preservação da origem aprovados.` Uma inspeção adicional da amostra com `json` da biblioteca padrão Python confirmou sete aeronaves, até 26 extras necessários, chaves/valores textuais dentro dos limites e concordância do ICAO da chave/atributo. O arquivo de entrada foi apenas lido. Essa inspeção não comprova conversão JSON pelo firmware.

Build ESP32 aprovado com PlatformIO 6.2.0, Espressif32 6.6.0, Arduino-ESP32 2.0.14 e Xtensa 8.4.0+2021r2-patch5. Resultado: RAM estática 21500/327680 bytes, flash 246553/1310720 bytes. O firmware central ainda não instancia o snapshot; esses valores não incluem a futura reserva de memória para recepção/publicação.

**Tamanho em memória:** `EXTRA_FIELD` = 98 bytes, `AIRCRAFT_STATE` = 3232 bytes e `PAYLOAD_SNAPSHOT` = 64672 bytes. O teste nativo imprimiu esses valores. A toolchain ESP32 confirmou os mesmos tamanhos compilando apenas objetos temporários, sem alterar o firmware. Procedimento executado na raiz do repositório:

```bash
/home/o-grande-kayky/.local/share/platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-g++ -std=c++11 -Wall -Wextra -Werror -Iobc-dh/headers -x c++ -c -o /tmp/fcp01-snapshot-sizes-esp32.o - <<'CPP'
#include "models/ObcData.h"
char extra_field_bytes[sizeof(obc_data::EXTRA_FIELD)];
char aircraft_state_bytes[sizeof(obc_data::AIRCRAFT_STATE)];
char payload_snapshot_bytes[sizeof(obc_data::PAYLOAD_SNAPSHOT)];
CPP
/home/o-grande-kayky/.local/share/platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm -S --radix=d /tmp/fcp01-snapshot-sizes-esp32.o
```

O snapshot deve ficar em armazenamento do chamador fora da pilha padrão de uma tarefa FreeRTOS. A publicação/concorrência e qualquer buffer adicional pertencem à implementação futura; não há cópia dupla ou buffer UART criado nesta entrega.

SHA-256 dos artefatos verificados nesta revisão:

```text
333c05c58f58505edc8b190bf998ff7589d99804c7b70b2f1f65137a2758bfa3  obc-dh/headers/models/ObcData.h
3c1dc0a2f350bae0b3c3e01fd435767a88e65799aa0e11b9e08a3b6e6e9dd12f  obc-dh/src/ObcData.cpp
72fb69f251cf617873659f0f81b228bee160e5b6f8b76ffc6394cb45938feb42  obc-dh/tests/PayloadSnapshotCheck.cpp
2b1b702b975fa8c14362a96c641117263301c3e02d5ac7d4ba18f66993e7acbf  obc-dh/payload.json
a29717ff0b458431a7029217b9a53e78d5810f6ee1507be76e60a757c649493d  common/protocol/obc-data.md
```

**Pendências atuais:** confirmar com Payload significado de `id`, unidades das velocidades, idade dos campos e aceite do mapeamento/limites/tipos dos extras; definir enquadramento, integridade, perdas/repetição e migração do produtor em COM-004. Snapshots não conservam necessariamente cada mensagem ADS-B intermediária: conciliar essa diferença com os requisitos existentes, sem alterar sua validação. Consolidar persistência dos snapshots e extras em DAD-003 com versionamento explícito. Permanecem também os aceites dos demais produtores e das estatísticas registrados acima.

Sem parser, upload, recepção UART, acesso ao SD ou execução física. Não concluir COM-004, DAD-003 ou TES por estes resultados. DAD-001 e DAD-003 permanecem em andamento no painel e em seus arquivos; nenhum arquivo foi movido para `done/`.

Extensão posterior de 05/11/2026 em DAD-003: snapshots e extras passaram a ter representação persistida v2 própria (`FCP2`, versão 2/tipo 8, 37–64497 bytes), sem alteração dos modelos/validadores desta tarefa ou dos layouts v1. A lacuna de persistência descrita acima foi atendida por codec e leitor localmente verificados; não há recepção/gravação automática integrada. O documento de contrato foi estendido, portanto seus hashes anteriores identificam suas revisões históricas, enquanto os fontes/testes de DAD-001 permanecem iguais. Aceites dos produtores, semântica do Payload e critérios por mensagem ADS-B continuam pendentes; ver evidências atuais em [DAD-003](DAD-003-registros-e-interpretacao.md).

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-OBC-009, REQ-OBC-010, REQ-NF-OBC-002, REQ-NF-OBC-003. Implementação não equivale a requisito validado.
- [payload/README.md](../../../../payload/README.md)
- [common/protocol/ttc-json.md](../../../../common/protocol/ttc-json.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-DADOS.
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
