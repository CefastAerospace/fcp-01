# COM-004 — Transporte UART e recepção validada do Payload

**Estado:** Concluída
**Responsáveis:** OBC-DH e Payload

## Encerramento do escopo próprio — 09/11/2026

Receptor FCPU v1, adaptador UART2, contrato externo ao JSON e verificação local do transporte estão implementados e comprovados. A suíte nativa e a autoverificação ESP32 passaram; o loopback original foi preservado.

Por orientação do responsável, esta entrega é encerrada e arquivada em
`done/` no limite de sua responsabilidade. Aceites externos, integração e
ensaios restantes continuam obrigatórios nas destinatárias abaixo; não são
declarados aprovados por este encerramento. Os critérios, vetores, entradas
e resultados esperados originais permanecem preservados.

Menções a “em andamento” ou “não mover para done” no histórico abaixo
registram decisões anteriores a este encerramento. Se uma integração
revelar defeito na entrega, registrar correção ou reabrir a tarefa com
rastreabilidade, sem adaptar a validação para forçar aprovação.

### Encaminhamentos registrados no encerramento — 09/11/2026

A tabela conserva as obrigações e a situação registradas no encerramento. O andamento posterior é acompanhado nas destinatárias e contextualizado na seção de situação atual abaixo.

| Destinatária | Responsabilidade ainda aberta | Situação |
|---|---|---|
| [INT-006](../../03-integracao/INT-006-fluxo-de-dados.md) | Acordar com Payload o FCPU v1, ponto focal/ponta Pi, porta/configuração, taxa nominal, rajadas e retenção; acompanhar o produtor e preservar a pendência de cada atualização ADS-B e a semântica dos campos. | Pendente na destinatária |
| [TES-005](../../04-testes/TES-005-uart-hardware.md) | Comprovar troca ESP32–Pi, GPIOs/console, desconexões, resets e alimentação unilateral; conciliar explicitamente o cenário binário histórico com FCPU sem reescrever entradas/resultados esperados. | Pendente na destinatária |
| [TES-009](../../04-testes/TES-009-falhas-e-recuperacao.md) | Executar os cenários físicos de desconexão/excesso UART com taxas acordadas e verificar recuperação e perdas. | Pendente na destinatária |

## Situação atual da integração — 10/11/2026

A primeira etapa INT-006 já inicia o receptor e liga snapshots ao diário e à TM. O produtor Pi, aceite do enquadramento/tráfego e preservação de cada atualização ADS-B permanecem na destinatária. Em 10/11 o responsável reafirmou JSON e descartou payload binário compacto; a TES-005 registra a substituição documental e conserva os cenários de falha. FCPU inclui bytes binários no fio e ainda precisa de aceite do Payload. As descrições de firmware sem UART e as proibições de encerramento abaixo são histórico da entrega original. [Situação da integração](../../03-integracao/INT-006-fluxo-de-dados.md).

## Objetivo

Receber mensagens completas e válidas do Payload e entregar dados utilizáveis à integração.

## Entrega

Implementar transporte, delimitação, interpretação e validação no ESP32, com buffers limitados e tratamento de perdas. Acordar e acompanhar a implementação da ponta Raspberry Pi pelo setor Payload. O receptor deve poder ser exercitado com entrada simulada antes de integrar o main.

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Fechar enquadramento e parâmetros com Payload; identificar responsável pela ponta Raspberry Pi.

## Definições e condições

- Alocação de projeto decidida: ESP32 TX 17 / RX 16, 115,2 kbps 8N1, conforme Payload e mapa compartilhado. Confirmar GPIOs/porta no Pi, variante das placas, disponibilidade de 16/17, TX/RX cruzados, GND, 3,3 V e comportamento com um lado desligado; decisão não comprova montagem.
- Habilitar a UART no Raspberry Pi e verificar console serial/Bluetooth ou outro uso da porta, preservando configuração do runbook do Payload.
- Em 05/11/2026, o responsável confirmou snapshots JSON completos diretamente pela UART, conforme [payload.json](../../../payload.json), substituindo a decisão de binário compacto. O script atual do repositório não produz esse formato nem implementa transporte. Os modelos internos ficam em [DAD-001](../../02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md) e a bancada em [TES-005](../../04-testes/TES-005-uart-hardware.md).
- Definir campos/tipos/unidades com o produtor, versão, tamanho máximo em bytes, delimitação, timeout, integridade, taxa nominal/rajadas, buffers/retenção, controle de fluxo e política de repetição/perdas. Os limites das structs não definem o tamanho do quadro. O exemplo compacto tem 4782 bytes para sete aeronaves: pelo menos 0,415 s em 115,2 kbps 8N1, sem pausas ou overhead adicional. Dimensionar com o máximo acordado, não apenas com a amostra.
- Não interpretar `id` como SeqID sem confirmação. Sequência/gaps, NACK e repetição constam dos cenários existentes e precisam de compatibilização explícita; esta revisão não altera entradas/resultados esperados de TES-004/TES-005. Resolver também como preservar cada mensagem ADS-B quando o produtor envia estados acumulados.
- Publicar a lista somente após o snapshot completo e válido; lista vazia limpa o estado ativo, enquanto erro preserva o estado anterior. Campos ausentes não herdam validade do snapshot anterior. Conferir ICAO da chave/atributo, strings numéricas completas, unidades e tipos de extras conforme o contrato. Excesso de capacidade exige falha explícita, sem truncamento.
- Memória bruta de uma struct não é representação portável. Preservar contrato TT&C e significado dos campos do Payload. JSON por linha é o enquadramento umbilical; o OBC implementa agora FCPU v1 conforme o contrato abaixo, ainda aguardando aceite do setor Payload.

## Critérios de conclusão

Contrato documentado e receptor implementado, com tratamento de mensagens parciais/consecutivas, inválidas e acima do limite. Verificar o comportamento de software. A troca física ESP32–Pi é comprovada em [TES-005](../../04-testes/TES-005-uart-hardware.md).

## Evidências

Os relatos, comandos, hashes e resultados a seguir descrevem as revisões datadas de cada entrega. Afirmações de ausência de integração ou de estado ainda em andamento nesse histórico não substituem o encerramento e os encaminhamentos atuais acima.

### Entrega local — 09/11/2026

O responsável OBC aprovou controle de transporte externo ao JSON, stop-and-wait
com um quadro pendente e manutenção explícita da pendência de preservação de
cada atualização ADS-B intermediária. O conteúdo `id`/`messages`, campos
ausentes, crescimento/redução da lista, unidades e extras foram preservados.

- [Contrato FCPU v1](../../../../common/protocol/payload-uart.md): cabeçalho,
  versão, comprimento, sessão/sequência, CRC-32, ACK/NACK, timeout e repetição.
  DATA até 65536 bytes de JSON; quadro até 65562 bytes; silêncio em cabeçalho
  ou CRC inválido e ressincronização limitada pelo buffer.
- `PayloadReceiver` interpreta, valida e publica usando os modelos existentes,
  sem I/O. `PayloadReceiverEsp32` adapta Serial2 com RX 4096/TX 256 bytes,
  tratamento de respostas parciais e atendimento sequencial por `Poll`.
- Duas reservas únicas verificadas no heap: quadro 65562 bytes e snapshot
  64672 bytes. Contexto não copiável/movível; novas chamadas de Init reutilizam
  memória. Parser limitado a 98304 bytes, sem alterar a lista em falha.
- O beta e seus vetores permanecem intactos em `PayloadUart`; só o ambiente
  isolado de loopback os compila. `main`, produtor Pi, modelos, codecs, SD e
  TT&C não foram alterados funcionalmente. O firmware normal não inicia UART.

### Verificações executadas

Com GCC 16.2.1, `-std=c++11 -Wall -Wextra -Werror -O1`, ASan e UBSan, a suíte
`PayloadReceiverCheck.cpp`/`.py` passou com vetores Python `struct`/`zlib`
independentes. Procedimento exato no [guia](../../../examples/README.md#teste-nativo-sem-raspberry-pi).
O LeakSanitizer foi executado no host devido à restrição de ptrace do sandbox;
não houve remoção de sanitizadores ou alteração de resultados esperados.

Foram verificados amostra real, u64 exato, campos opcionais, lista vazia,
limites, texto/ICAO/tipos inválidos, NUL, consumo fragmentado byte a byte,
quadros consecutivos, CRC/versão inválidos, ruído, candidatos retidos,
preservação integral da lista anterior, gaps, duplicatas, ACK perdido,
mudança de sessão, reset e fronteiras de timeout de 250/8000 ms.

As regressões existentes de modelos v1/snapshots, codec v1, codec v2 e leitor
v1/v2/misto passaram com UBSan e `-Werror`, sem alteração de vetores/asserts.

Builds ESP32 `obc`, `uart-loopback` e `uart-receiver-check` aprovados na
plataforma Espressif32 6.6.0, Arduino-ESP32 2.0.14 e Xtensa 8.4.0. ArduinoJson
fixado em 7.0.4 com patch aprovado de inicialização de `nextId_`; o hook
`tools/PatchArduinoJson.py` confere versão e SHA256 antes da alteração.

O [loopback físico](../../../examples/uart-loopback-2026-11-09.txt) no WROOM-32,
G17 ligado a G16, registrou 60 transmissões e 60 recepções corretas; somente
o gap proposital da sequência 3 e retorno a `1, 2, 4` após reset. Não é teste
do novo protocolo ou da ponta Pi.

A [autoverificação na placa](../../../examples/uart-receiver-check-split-heap-2026-11-09.txt)
injetou a amostra em RAM e aprovou aceitação, duplicata e CRC inválido. Usou
boot sintético 1, sem NVS, aquisição ou atuadores. Contexto: 128 bytes;
parser: pico de 7190 bytes; heap livre após Init: 205900 bytes; mínima pilha
livre observada: 3164 bytes. RAM estática do firmware de teste: 30056 bytes;
flash: 291333 bytes. SHA256 de `uart-receiver-check/firmware.bin`:
`a11cf743754b56c487ed124ba5285961e800055a77a5ab4fe2e4eadef9edd353`.
As medidas não garantem o consumo de todas as entradas ou da integração.

O dimensionamento foi corrigido após falha de DRAM estática e uma reserva
monolítica maior que o bloco contíguo livre de 110580 bytes. O
[log da falha tratada](../../../examples/uart-receiver-check-2026-11-09.txt) foi
preservado. Os limites do modelo e os resultados esperados não foram reduzidos.

### Condições externas para encerramento

- Registrar aceite do FCPU pelo Payload e identificar formalmente a
  responsável pela ponta Pi, acompanhando sua implementação. O script atual
  do repositório permanece sem UART e sem o novo produtor de snapshots.
- Confirmar porta/configuração Pi, taxa nominal/rajadas e retenção/backlog.
  Não tomar a frequência do loopback ou capacidade teórica como taxa de missão.
- Manter explícita a pendência de cada atualização ADS-B e conciliar o cenário
  histórico binário de TES-005 sem alterar suas entradas/resultados em silêncio.
- A troca física ESP32–Pi continua em TES-005; integração no main/SD/downlink
  continua nas tarefas INT. Não são evidências desta entrega local.

COM-004 permanece em andamento até consolidar essas condições. Não mover para
`done/` por aprovação do loopback ou existência do código, isoladamente.

## Referências

- Requisitos relacionados: REQ-OBC-005, preparação de REQ-OBC-006 e REQ-OBC-011. Implementação não equivale a requisito validado.
- [payload/README.md](../../../../payload/README.md)
- [payload/telemetry.py](../../../../payload/telemetry.py)
- [payload/raspberry-pi-runbook.md](../../../../payload/raspberry-pi-runbook.md)
- [obc-dh/.agents/PINAGEM.md](../../../.agents/PINAGEM.md)
- Origem no planejamento anterior: P-DADOS, P-PAYLOAD.
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
