# Payload–OBC UART — FCPU v1

**Estado em 09/11/2026:** desenvolvimento e verificação local do OBC
concluídos em [COM-004](../../obc-dh/.tasks/01-comunicacao/done/COM-004-uart-payload.md).
Aceite do setor Payload, parâmetros de tráfego e acompanhamento da ponta Pi
continuam em [INT-006](../../obc-dh/.tasks/03-integracao/INT-006-fluxo-de-dados.md);
troca física entre placas permanece em TES-005/TES-009. O encerramento
não declara interoperabilidade nem altera critérios de validação anteriores.

## Conteúdo e enlace

UART2 do ESP32: **115200 baud, 8N1, RX GPIO16 e TX GPIO17**, níveis de 3,3 V.
Na ligação ao Pi, cruzar TX/RX e compartilhar GND; porta, GPIOs e console
serial da ponta Pi devem ser conferidos pelo Payload.

O corpo DATA conserva o objeto JSON existente, com `id` e `messages`, conforme
[payload.json](../../obc-dh/payload.json) e [obc-data.md](obc-data.md#0-snapshot-do-payload--modelo-em-memória).
Metadados de transporte ficam fora do JSON. Whitespace e quebras de linha
podem estar no corpo; uma linha não equivale a um quadro. Nenhuma alteração
do script `payload/telemetry.py` faz parte desta entrega.

`id` é inteiro u64 opaco, inclusive zero. Não é sessão ou sequência do enlace.
`messages` é a lista completa indexada por ICAO, não uma atualização parcial.
O parser aceita os dois campos obrigatórios da raiz e os tipos textuais/null
dos campos padronizados. Extras são textuais; outros tipos não são convertidos
silenciosamente. ICAOs têm seis hexadecimais e são comparados sem distinção de
caixa. O produtor deve emitir nomes de propriedades únicos.

Após validação integral, a lista nova substitui a anterior. Lista vazia limpa
o estado ativo. Erro ou quadro incompleto preserva o estado anterior. Ausentes
não herdam disponibilidade. Textos conhecidos não decodificados permanecem nos
extras, incluindo o par de coordenadas quando somente uma é interpretável.
Altitude usa o sufixo estrito ` feet`; velocidades não recebem unidades novas.
`tick` é conservado como texto separado do boot/tempo monotônico do OBC.

## Quadro binário

Todos os inteiros são **little-endian**, sem padding de estruturas C++.

| Offset | Campo | Bytes | Definição |
|---:|---|---:|---|
| 0 | assinatura | 4 | ASCII `FCPU` |
| 4 | versão | 1 | `1` |
| 5 | tipo | 1 | DATA = 1; ACK = 2; NACK = 3 |
| 6 | sessão | 8 | Identificador u64 não nulo do transmissor |
| 14 | sequência | 4 | u32 não nulo; origem inicia em 1 |
| 18 | comprimento | 4 | Quantidade de bytes do corpo |
| 22 | corpo | variável | JSON DATA ou controle abaixo |
| 22 + comprimento | CRC | 4 | CRC-32/ISO-HDLC do cabeçalho e corpo |

Cabeçalho tem 22 bytes. CRC tem polinômio refletido `0xEDB88320`, inicialização
e XOR final `0xFFFFFFFF`; é compatível com `zlib.crc32(cabecalho + corpo)`.
O próprio CRC não entra no cálculo. O OBC reutiliza `obc_record::ComputeCrc32`.
Não é um mecanismo de autenticação.

DATA comporta até **65536 bytes de JSON**, totalizando no máximo **65562 bytes**.
Esse limite é independente do tamanho do modelo ou do registro persistido v2.
Excesso não autoriza truncar lista, campo ou mensagem.

ACK tem corpo vazio, total de 26 bytes, e repete sessão/sequência do DATA.
Confirma aceitação em RAM; não confirma SD, downlink ou recepção no solo.

NACK repete sessão/sequência do DATA recusado e tem corpo de cinco bytes:
motivo u8 seguido da próxima sequência esperada u32. Total de 31 bytes.

| Motivo | Valor | Significado |
|---|---:|---|
| INVALID_JSON | 1 | Sintaxe, profundidade ou conteúdo adicional após o JSON |
| INVALID_VALUE | 2 | Tipo, identidade ou conteúdo incompatível com o modelo |
| LIMIT_EXCEEDED | 3 | Contagem ou texto acima da capacidade |
| NO_MEMORY | 4 | Parser sem espaço no orçamento disponível |
| SEQUENCE_ERROR | 5 | Gap, sequência antiga ou reutilização incompatível |

Antes de estabelecer uma sessão, a sequência esperada no NACK é a do próprio
quadro recusado. Para sessão diferente após uma aceitação, espera-se 1. Na
mesma sessão, espera-se a última aceita + 1; zero indica sequência esgotada e
necessidade de nova sessão. Só há resposta após verificar um quadro completo
com CRC válido. Cabeçalho/CRC inválido não fornece metadados confiáveis para
NACK: o receptor ressincroniza e o transmissor depende de timeout.

## Sequência, tempo e repetição

O produtor cria uma sessão nova e não nula em cada execução, sem reutilizar a
anterior, e inicia a sequência em 1. Ao esgotar u32, inicia sessão nova; não
transmite sequência zero. Isso não altera o `id` do snapshot.

Há **um quadro pendente por vez**. O Pi conserva seus bytes até receber ACK
válido com a sessão/sequência correspondentes. Um controle também deve ter
assinatura, versão, comprimento e CRC conferidos; controle antigo ou corrompido
não libera o quadro pendente.

Após terminar o TX, aguarda ACK por **1000 ms**. NACK/timeout permite repetição
do mesmo quadro, até **três tentativas totais**. Esgotado o limite, conserva o
quadro, sinaliza falha e não avança silenciosamente. Se NACK exigir um quadro
anterior que não está retido, sinaliza dessincronização; não fabrica conteúdo
para preencher o gap. A política de backlog da aquisição pertence ao Payload.

O receptor confirma novamente uma repetição do último DATA aceito com mesma
sessão, sequência e CRC, sem republicar ou alterar seu timestamp. Gap não
avança a referência. Reutilizar a última sequência com CRC diferente é erro.
Após observar uma sessão, uma sessão diferente só começa com sequência 1.

Após reset/Init do OBC, o primeiro DATA válido estabelece a referência, mesmo
quando o Pi está repetindo uma sequência superior a 1. Perdas anteriores são
desconhecidas. Não há deduplicação durável entre boots nem promessa de entrega
única ao SD. O protocolo pressupõe um único produtor e bytes UART ordenados.

Quadro parcial expira após **250 ms sem bytes atendidos** ou **8000 ms desde o
primeiro byte atendido**. O núcleo recebe tempo monotônico de 64 bits e rejeita
tempo decrescente. Seus instantes correspondem ao atendimento do receptor,
não a timestamps físicos individuais fornecidos pelo periférico UART.
Após ruído ou CRC inválido, procura a próxima assinatura e só publica um
candidato integralmente validado. Conserva prefixos parciais de assinatura.

## Uso, capacidade e memória

`obc_payload::Receiver` é o núcleo portátil. `Init(boot_id)` exige boot não zero
e reserva uma vez dois blocos independentes: **65562 bytes** para o quadro e
**64672 bytes** para o snapshot na configuração validada. Falha retorna false
sem publicar estado; uma reserva parcial é liberada. Chamadas posteriores
reutilizam as reservas. `unique_ptr` conserva a propriedade e libera os blocos
na destruição. O contexto não pode ser copiado ou movido.

`Feed(bytes, size, now_ms)` retorna status, quantidade consumida e eventual
resposta binária. Para no primeiro evento; repassar os bytes não consumidos.
Também admite size zero para processar candidatos retidos. Somente READY
publica. `GetSnapshot()` é nulo antes disso e permanece estável até o próximo
READY/Init. `CheckTimeout` atende ausência de dados; estatísticas saturam em u64.
Fonte não deve sobrepor o armazenamento do receptor ou seu snapshot.

`obc_payload::esp32::Init(receiver, boot_id)` usa Serial2 com RX 4096 bytes e TX
256 bytes. O chamador conserva o contexto e fornece boot confirmado. `Poll()`
atende até 256 bytes e um evento por chamada; deve ser chamado ao menos a cada
10 ms. Respostas parciais são retidas sem espera de espaço de TX. Consumir
READY antes da próxima chamada. Uso exclusivo e sequencial em uma tarefa,
fora de ISR/FOC; não acessar Serial2 por outra API em paralelo.

O parser tem orçamento de **98304 bytes**, incluindo seus prefixos de alocação;
não são 96 KiB reservados permanentemente. Buffers do transporte e modelo são
adicionais. Chaves/valores com NUL embutido não cabem no modelo e são rejeitados
antes da desserialização. Não há validação adicional de calendário de `tick`
ou alteração das regras de UTF-8 dos modelos existentes.

No ESP32-WROOM-32, a autoverificação da amostra observou contexto de 128 bytes,
pico do parser de 7190 bytes, heap livre após inicialização de 205900 bytes e
mínima pilha livre de 3164 bytes na tarefa Arduino. São medidas dessa amostra,
não limites garantidos do firmware integrado. Com `-fstack-usage`, o quadro
individual de `ParseSnapshot` foi de 3648 bytes; somar chamadas internas e
demais consumidores ao dimensionar a tarefa, inicialmente com pelo menos
8 KiB de pilha e nova medição na integração.

## Verificação e pendências externas

Procedimentos e comandos estão em [examples/README.md](../../obc-dh/examples/README.md).
Os testes nativos usam quadros independentes `struct`/`zlib`, o receptor C++
real, `-Werror`, ASan e UBSan. Exercitam a amostra, limites, campos ausentes,
fragmentação, mensagens consecutivas, erros, CRC, timeout, repetição e reset.
O teste ESP32 injeta a amostra em RAM e mede memória/pilha; o loopback antigo
comprova somente a UART2 local, preservando seus vetores originais.

Na primeira etapa da INT-006, o firmware normal inicia o receptor após os
serviços do boot e TT&C. O script do Pi e o protocolo FCPU permanecem
preservados. ArduinoJson fica fixado em 7.0.4, com
patch reproduzível de inicialização de `nextId_`, guardado por versão e SHA256.
Esse patch não reduz avisos nem altera entradas/resultados dos testes.

### Atendimento separado no fluxo integrado

`StartTransportTask()` é chamado após `Init` e antes do consumidor. A tarefa
`payload_uart` é a única proprietária de Serial2: Core 1, prioridade 3,
pilha de 3 KiB e atendimento a cada 1 ms ocioso. Transfere bytes por um ring
fixo de 1 KiB e envia respostas sem esperar espaço de TX. A tarefa de
processamento, prioridade 2/pilha de 8 KiB, é a única que chama `Poll`,
acessa `Receiver` e consome o snapshot. O escritor SD usa tarefa separada.

Sem `StartTransportTask`, `Init/Poll` conservam o caminho sequencial das
bancadas anteriores. Não misturar inicialização/consumo concorrentes nem
acessar Serial2 por outro código após transferir sua propriedade.

`poll_gap_ms` observa o atendimento do transporte; `process_gap_ms` observa
as chamadas do consumidor e inclui o custo de validação/codificação. Um
parser mais demorado não redefine o limite de 10 ms da UART. Ring cheio
faz o transporte esperar espaço e é diagnosticado em `uart_full`; essa
observação não é uma contagem de mensagens perdidas. Sobrefluxo físico e
tráfego nominal continuam exigindo os ensaios com a ponta Pi.

As estatísticas antigas são preservadas. `complete_candidates` conta
candidatos completos, inclusive CRC/conteúdo inválido e repetição;
`missing_positions` conta posições ausentes distintas observadas na sessão,
sem descontar recuperação posterior. O antigo `gaps` continua contando
ocorrências. A integração usa as observações corretas para o registro de
estatísticas, sem reinterpretar os contadores antigos.

Na preparação de INT-006, registrar aceite do Payload, responsável pela
ponta Pi, sua porta/configuração, implementação e parâmetros nominais de
tráfego/rajadas/retenção. Esses itens foram encaminhados no encerramento
do desenvolvimento de COM-004 e continuam pendentes. Não há taxa de missão
validada; 1 quadro/s do beta não é taxa nominal de snapshots. Um quadro
máximo já exige cerca de 5,69 s de UART contínua, antes de processamento
e confirmação.

A preservação de toda atualização ADS-B intermediária permanece como pendência
explícita acordada com o responsável: snapshots acumulados não a demonstram.
Não alterar requisitos ou resultados esperados para declarar esse atendimento.
TES-005 ainda contém o cenário histórico de binário compacto; sua conciliação
com este transporte precisa ser discutida, sem reescrever o critério antigo.
