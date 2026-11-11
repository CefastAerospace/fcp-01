# Entregas OBC INT-006 e preparação TT&C

## Implementação local

Cada mudança na projeção numérica de uma aeronave recebe `(boot, sequence)`.
Boot vem da reserva confirmada existente. Sequence é u64 crescente a partir
de um. Retentativas e leitura após reset conservam esse par e o instante
monotônico original. A comparação usa ICAO, disponibilidade, latitude,
longitude e altitude. Uma sequência A, B, A conserva três entregas.

Latitude e longitude usam graus e altitude usa pés, conforme o modelo
validado. Velocidade, rumo, contagem e UTC individual ficam indisponíveis.
Identificação, tick e extras continuam no snapshot histórico completo.
O conteúdo disponível na interface de transmissão é uma cópia de valor.

O cache tem 32 posições e atende por ID crescente. Uma entrega fica
aguardando confirmação do solo por vez. O chamador escolhe o timeout e
informa a conclusão local da tentativa. O ACK do solo tem operação própria.
O firmware normal publica essas APIs em `ObcDataFlowEsp32.h`. Desde a
integração de 11/11/2026, `ObcTtc` liga essas operações a `SendAircraft`,
`ReceiveTransmitEvent` e `ReceiveConfirmation`. Recusa imediata do envio
finaliza a tentativa local como falha; somente `cf` confirma a entrega.
O timeout começa na reserva da tentativa e vale 10 s provisórios, incluindo
a espera na fila TT&C. O valor operacional depende de medição conjunta.

Enquanto houver consulta pendente, uma posição fica reservada à recuperação.
A admissão usa até 31 posições e pode substituir cópias duráveis disponíveis,
conservando a posição reservada. Ao retirar uma cópia durável de um cache
cheio, a admissão libera também o espaço necessário à consulta. Saturação
sem cópia durável substituível contabiliza perda. A conclusão da consulta
libera a admissão para usar novamente as 32 posições.

O escritor existente serializa DATA e CONFIRM com os registros históricos.
Após cada registro de entrega, fecha o segmento e cria o próximo segmento
exclusivo. Isso permite recuperar cópias duráveis usando a leitura existente
de arquivos fechados. Essa escolha aumenta a quantidade de arquivos e de
sincronizações. Medir seu custo com cartão e perfil de tráfego reais.

Um ACK recebido durante gravação fica em RAM até o DATA terminar. Seu
CONFIRM é escrito posteriormente pelo mesmo escritor. Falha persistindo
CONFIRM permite repetição após reset, com o ID original. Cópias duráveis
podem sair do cache. A próxima consulta ao histórico precede novo envio.
Sob saturação de cópias voláteis, as perdas são contabilizadas. Falhas do SD
conservam a oportunidade de transmitir as cópias disponíveis em RAM.

A recuperação consulta arquivos fechados por blocos de até 256 bytes,
valida CRC e conteúdo de FCD1 e procura pendências por ID. Confirmações
órfãs e caudas incompletas recebem diagnóstico. A busca usa passagens
adicionais para cruzar DATA e CONFIRM, com memória constante e custo
quadrático por consulta no pior caso. O escritor cede CPU entre leituras.
Uma leitura falha deixa a recuperação durável pendente de diagnóstico e
libera as cópias disponíveis em RAM para transmissão.

## Formato persistido FCD1

Inteiros usam little-endian. Reais usam IEEE 754 binary64. CRC é
CRC-32/ISO-HDLC existente, cobrindo todos os bytes anteriores ao próprio CRC.
Os registros compartilham os segmentos `boot_<boot hex16>_<segmento hex8>.bin`
com FCP1/FCP2/FCP3. O leitor atualizado interpreta os quatro formatos.

| Offset | Bytes | Conteúdo |
|---|---|---|
| 0 | 4 | ASCII FCD1 |
| 4 | 1 | Versão 1 |
| 5 | 1 | Tipo 1 DATA ou 2 CONFIRM |
| 6 | 2 | Tamanho total, 68 DATA ou 36 CONFIRM |
| 8 | 8 | Boot do ID original, maior que zero |
| 16 | 8 | Sequence do ID original, maior que zero |
| 24 | 8 | Monotônico original em DATA, zero em CONFIRM |
| 32 | 4 | Em CONFIRM, CRC e fim do registro |
| 32 | 4 | Em DATA, ICAO de 24 bits |
| 36 | 1 | Máscara DATA, bits 0 latitude, 1 longitude, 2 altitude |
| 37 | 3 | Reservados zero |
| 40 | 8 | Latitude |
| 48 | 8 | Longitude |
| 56 | 8 | Altitude |
| 64 | 4 | CRC DATA |

Posição exige os dois bits juntos. Campos indisponíveis têm valor numérico
zero e máscara desligada. A projeção JSON usa `null`. Disponíveis exigem
valores finitos e os limites geográficos do modelo. Decodificação só publica
após validação integral. Recuperação conserva registros legíveis posteriores
a um trecho danificado.

## Proposta histórica request_log — 10/11/2026

Esta proposta antecede o contrato adotado de [ttc-json.md](ttc-json.md#request_log).
Seu conteúdo é conservado para rastreabilidade; não descreve o despacho atual.

- `list` recebe cursor de nome opcional e retorna próximo nome, tamanho,
  indicador de arquivo aberto e cursor seguinte. Nome vazio encerra a lista.
- `read` recebe nome fechado, offset em bytes e limite de bloco positivo.
  Resposta conserva nome, offset solicitado, total, bytes e offset seguinte.
  EOF corresponde a offset igual ao total e zero bytes. Offset maior é erro.
- As respostas precisam de identificação da requisição e destino USB
  explícito. O cliente avança apenas após validar a resposta correspondente.
- A codificação dos bytes, tamanho máximo do bloco e envelope JSON serão
  acordados medindo o corpo completo contra 245 bytes e o quadro contra 255.
  O limite local de bloco de 256 bytes é uma capacidade de leitura do SD.
- Erros precisam distinguir argumento inválido, arquivo ausente, arquivo
  aberto/ocupado, falha de leitura e indisponibilidade do SD. Em ocupação ou
  resposta perdida, o cliente repete o mesmo nome/offset sem avançar.
- A extração inicial usa arquivos fechados. O cliente confere tamanho,
  sequência dos offsets e CRC dos registros após reconstruir o arquivo.

## Encaminhamentos históricos para TT&C — 10/11/2026

- Transportar o ID estável de entrega, independente do contador `n` do enlace.
- Definir ACK dos dados pelo solo, contendo esse ID e sua semântica de aceite.
- Deduplicar processamento por ID e repetir ACK para duplicatas. Definir a
  persistência dessa deduplicação após reinício da estação.
- Associar enfileiramento, TX concluído e falha local à entrega. `Ttc::Send`
  atualmente informa enfileiramento.
- Ampliar BuildAircraft e a estação para campos indisponíveis, com `null`
  conservado inclusive no mapa. Definir unidades e origem de cada campo.
- Acordar tráfego, janela, timeout, retentativas e prioridade dos comandos.
  Medir a capacidade de preservar cada alteração durante congestionamento.
- Medir o pior quadro incluindo ID e confirmações nos limites 245/255 bytes.
- Acordar request_log completo e seleção USB nas respostas da biblioteca.

## Integração OBC e aceite do contrato — 11/11/2026

O responsável OBC aceitou `[boot,seq]`, `cf`, blocos de até 90 bytes e
listagem paginada com `end`. A resposta `read` não transmite offset seguinte:
ele é derivado dos bytes decodificados. O contrato da biblioteca e da estação
está em [ttc-json.md](ttc-json.md); esse aceite não comprova ensaio físico nem
encerra o aceite conjunto da INT-004.

O OBC implementa o despacho de `request_log`, incluindo erros e seleção USB.
O proprietário existente do SD executa as leituras/listagens por uma caixa
fixa de um pedido. O atendimento TC não espera I/O. Ocupação ou prazo local
de 1 s esgotado produzem `busy`; falta do serviço produz `no_sd`. Trabalho
que expira em execução conserva a caixa até terminar e seu resultado é
descartado. ACK `ok` confirma enfileiramento de `lg`, inclusive de erro;
recusa da fila produz `state_rejected`. ACK retorna pela origem do TC e
`lg` pelo destino solicitado.

`ObcDownlink` adapta disponibilidade e unidades sem inventar UTC individual,
velocidade, rumo ou contagem. ICAO acima de 24 bits e altitude não
representável no `float` do TT&C recusam o envio; o DATA original não muda.
Arquivo fora do limite de nomes do contrato não é truncado na listagem.

Verificações portáteis, builds e roteiro de placa estão na
[INT-006](../../obc-dh/.tasks/03-integracao/INT-006-fluxo-de-dados.md).
`int006-ttc-check` exercita a integração real da biblioteca por USB com
Payload, SD e solo simulados. As bancadas antigas conservam consumidor
simulado próprio. Após corrigir a admissão de memória, o roteiro original
passou na bancada USB e no firmware normal. Relatórios e histórico da
reprovação estão na INT-006. O parser mantém teto de 98.304 bytes e pode
rejeitar por NO_MEMORY; esse teto não é reserva permanente na inicialização.

Na revisão da estação sobre `f7af605`, foram identificadas pendências de
durabilidade do conteúdo antes de `cf`, continuação após cauda truncada no
registro de IDs e continuidade da recepção em erro de log. A garantia do
contrato continua exigida; correções pertencem ao TT&C. Mensagens preparadas,
sem envio externo, estão nos [encaminhamentos OBC](../../obc-dh/encaminhamentos-2026-11-11.md).

As condições do Payload incluem granularidade, unidades, validade, tráfego,
rajadas e retenção. A conservação de atualizações ADS-B intermediárias exige
o contrato do produtor. A INT-006 conserva seus aceites e ensaios pendentes.
