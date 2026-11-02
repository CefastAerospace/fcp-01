# DAD-001 — Modelos de dados e formatos persistidos

**Estado:** Em andamento
**Responsáveis:** OBC-DH e produtores dos dados

## Objetivo

Definir representações documentadas para dados recebidos e registros armazenados.

## Entrega

Definir tipos concretos, unidades, validade, versão, tamanho máximo, enquadramento e leitura posterior. Cobrir missão, TM, eventos, falhas, estatísticas, reinicializações e referências de tempo. Usar headers/models/ apenas quando houver tipos efetivamente implementados. Não persistir memória bruta de structs nem alterar o contrato TT&C.

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Acordar campos, unidades e limites com Payload, TT&C, ADCS, EPS e Térmico.

## Definições e condições

- Definir status, temperatura, solicitações/respostas, significado de `val_flags`, validade de dados acumulados em instantes diferentes e comportamento de `seq_id` após reset. Manter distinção entre timestamp do Payload e recepção no OBC.

## Critérios de conclusão

Contrato e tipos necessários disponíveis, com exemplos e limites explícitos. Formato de transporte UART, representação interna e formato do arquivo têm responsabilidades distintas; nenhuma decisão sobre JSON/binário persistido será presumida.

## Evidências

Implementação local e verificações executadas em 01/11/2026, sobre a base Git `e734ebb` e o trabalho local de DAD-002. Alterações ainda não commitadas. A tarefa permanece **Em andamento**, pois os produtores ainda não deram aceite ao contrato proposto.

### Entrega local

- [Contrato OBC v1 proposto](../../../common/protocol/obc-data.md), [tipos portáteis](../../headers/models/ObcData.h), [validadores](../../src/ObcData.cpp) e [teste nativo](../../tests/ObcDataCheck.cpp).
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

### Pendências para concluir

- Payload: aceitar campos/unidades, mensagem por atualização, sessão e ciclo da sequência, disponibilidade e migração explícita do modelo legado. Alinhar enquadramento, solicitações/respostas e geração da sessão em COM-004, sem presumir que o layout do SD será o quadro UART.
- TT&C, ADCS, EPS e Térmico: confirmar mapeamento/validade das leituras, unidades, posições térmicas e códigos por origem. A máscara local de TM não modifica o pacote TT&C; adaptação de horário desconhecido no downlink permanece na integração.
- Confirmar as definições das estatísticas com os produtores/consumidores. Não há aceite externo registrado apenas por aprovação deste plano local.

Sem upload ou execução física. Não há evidência de UART, integridade do SD, latência ou interoperabilidade dos setores. `setup()`/`loop()`, módulos temporais/I2C, bibliotecas dos setores e validações existentes foram preservados. Não mover esta tarefa para `done/` antes do aceite e dos critérios completos atendidos.

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-OBC-009, REQ-OBC-010, REQ-NF-OBC-002, REQ-NF-OBC-003. Implementação não equivale a requisito validado.
- [payload/README.md](../../../payload/README.md)
- [common/protocol/ttc-json.md](../../../common/protocol/ttc-json.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-DADOS.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
