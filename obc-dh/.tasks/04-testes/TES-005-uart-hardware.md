# TES-005 — UART entre ESP32 e Raspberry Pi

**Estado:** Em andamento
**Responsáveis:** OBC-DH e Payload

A execução e o relato dos ensaios cabem ao setor de testes; a análise e as correções de firmware cabem ao responsável OBC-DH, com apoio dos setores envolvidos. Seguir as [responsabilidades e regras de reexecução](../README.md#responsabilidades-correção-e-reexecução).

## Objetivo

Comprovar troca física bidirecional e comportamento de falhas do enlace UART.

## Entrega

Validar snapshots JSON completos com detecção de perdas e repetição conforme o contrato aceito, em 115,2 kbps 8N1. Conferir TX/RX cruzados, GND, 3,3 V, GPIOs e console/Bluetooth do Pi. Exercitar desconexão, reset de uma ponta, mensagens parciais e rajadas dentro dos parâmetros aprovados, incluindo comportamento com uma ponta desligada.

## Pré-requisitos

- **Tarefas:** [COM-004](../01-comunicacao/done/COM-004-uart-payload.md)
- **Condições externas:** Ponta UART do Payload implementada; placas, montagem e porta serial disponíveis.

## Critérios de conclusão

Registrar montagem, firmware/script, tráfego e perdas/recuperação contra o contrato. Teste com dados simulados não comprova aquisição ADS-B completa.

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [COM-004](../01-comunicacao/done/COM-004-uart-payload.md) | Comprovar troca ESP32–Pi, GPIOs/console, desconexões, resets e alimentação unilateral; usar snapshots JSON conforme a decisão de 10/11; preservar os cenários de falha e obter aceite do enquadramento com Payload antes do ensaio. | Pendente |
| [TES-004](done/TES-004-uart-software.md) | Executar a interoperabilidade e falhas nas duas pontas reais; os casos nativos aprovados não encerram o ensaio físico. | Pendente |

## Formato vigente e rastreabilidade — 10/11/2026

O responsável reafirmou snapshots JSON completos para recebimento/reenvio e descartou o payload binário compacto. A frase anterior da entrega era: “Validar binário compacto com SeqID/NACK em 115,2 kbps 8N1.” Ela identifica o formato substituído, não um ensaio adicional a executar. Sequência, perdas, repetição, reset, mensagens parciais e rajadas continuam no escopo.

O receptor local usa [FCPU v1](../../../common/protocol/payload-uart.md): JSON no corpo, com cabeçalho, CRC e ACK/NACK binários também transmitidos pelo cabo. Aceitar JSON como conteúdo não declara aceite desse enquadramento pelo Payload. A decisão de representação binária interna do SD não resolve a interface UART. Confirmar produtor, enquadramento e tráfego com Payload na INT-006 antes do ensaio conjunto. Não modificar os testes arquivados para adaptá-los a uma ponta incompatível.

## Prontidão e preparação para o setor de testes

COM-004 está concluída no escopo local. Pode-se encaminhar preparação de bancada, mas faltam ponta Pi, aceite, taxa/rajadas/retenção e montagem. O loopback já registrado testa somente UART2 local com o beta; a injeção em RAM e `int006-check` também não substituem a troca física do novo protocolo.

## Checklist de execução

- [ ] **Contrato e montagem:** registrar versões das duas pontas, porta Pi, console/Bluetooth, GPIOs, TX/RX cruzados, GND e 3,3 V. Confirmar ausência do jumper de loopback e método seguro de alimentação unilateral. Não iniciar aceitação enquanto o enquadramento estiver sem acordo.
- [ ] **Snapshot nominal e retorno:** enviar conteúdo conhecido e comparar a lista aceita, campos/extras e resposta no fio. No FCPU local, ACK deve corresponder à sessão/sequência e confirmar apenas RAM; verificar que o transmissor interpreta essa resposta.
- [ ] **Substituição e lista vazia:** aumentar/reduzir a lista e enviar `messages: {}` válido. Esperar substituição integral da lista ativa; nenhum campo ausente deve herdar validade. O histórico já gravado não deve ser apagado.
- [ ] **Fragmentação e quadros consecutivos:** enviar o mesmo conteúdo em divisões diferentes e quadros consecutivos no perfil permitido; esperar a mesma interpretação e uma publicação por DATA novo aceito, sem tratar cada leitura ou quebra de linha como quadro completo.
- [ ] **Inválidos e limites:** exercitar conteúdo inválido, CRC incorreto e excesso de capacidade, seguido de snapshot válido. Esperar preservação da lista anterior e recuperação para o próximo quadro válido. Sob FCPU, cabeçalho/CRC inválidos não devem gerar NACK com metadados não confiáveis; conteúdo recusado com quadro íntegro segue o motivo do contrato.
- [ ] **Perdas, sequência e repetição:** perder DATA/ACK e provocar gap/reutilização incompatível. Sob FCPU, repetição idêntica do último aceito deve reenviar ACK sem republicar nem mudar timestamp; gap não avança a referência. Verificar timeout de ACK de 1000 ms após TX, três tentativas totais e falha explícita no esgotamento, sem avanço silencioso do produtor.
- [ ] **Quadro parcial e silêncio:** interromper o envio e conferir expiração/ressincronização. Se FCPU aceito, usar fronteiras de 250 ms sem bytes atendidos e 8000 ms desde o início, preservando os resultados do contrato local.
- [ ] **Desconexão e reset de cada ponta:** testar separadamente Pi, OBC e cabo. Esperar comportamento conforme o contrato aceito, sem aceitar lixo como snapshot ou usar referência temporal de outro boot. FCPU não promete deduplicação durável após reset do OBC.
- [ ] **Rajadas e tamanho máximo:** usar taxas, tamanho e duração acordados; medir atendimento UART, memória, perdas e recuperação. No FCPU local, DATA admite até 65536 bytes de JSON/65562 bytes totais; não substituir o tráfego nominal pela frequência do beta.
- [ ] **Uma ponta desligada:** executar somente a montagem aprovada e registrar efeitos elétricos e de comunicação. Retorno do enlace deve ser observável; dados simulados não comprovam aquisição ADS-B completa.

As expectativas específicas de FCPU acima descrevem a implementação local. Mudança acordada do enquadramento exige análise e regressão próprias antes de atualizar esses casos. Registrar [evidências por cenário](../README.md#registro-por-cenário); resultados do beta ou simuladores não preenchem os casos físicos.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

### Bancada local — 09/11/2026

O responsável identificou ESP32-WROOM-32 e os pinos G16/G17. Foi executado o
ambiente isolado `uart-loopback`, com jumper G17–G16 confirmado pelo responsável,
115200 8N1 e USB em `/dev/ttyUSB0` via CH340/CH341. A porta não aparece no
sandbox, mas foi identificada no kernel e acessada no host. Upload e reset
foram executados, sem alterar os vetores ou o parser do beta.

[Log completo](../../examples/uart-loopback-2026-11-09.txt): 60 envios e 60
recepções com ICAO E4953A, altitude 39000 pés, velocidade 489 nós e sequência
`1, 2, 4, ... 61`. Somente o gap proposital esperado=3/recebido=4 e seu NACK
simulado. Sem erros de JSON, excesso de buffer ou reinício inesperado. Novo
reset voltou a `1, 2, 4`. O NACK desse beta é apenas uma mensagem de log.

Separadamente, `uart-receiver-check` injetou a amostra real em RAM no ESP32 e
aprovou o novo receptor, repetição e CRC, com heap/pilha medidos. Seu
[log](../../examples/uart-receiver-check-split-heap-2026-11-09.txt) não é ensaio
físico do protocolo novo: não há transmissor Pi ou quadros enviados pelo fio.
Procedimentos e retorno ao firmware normal no [guia](../../examples/README.md).

Permanecem sem execução a troca bidirecional ESP32–Pi, desconexões, reset de
uma ponta, rajadas e alimentação unilateral. A divergência de conteúdo registrada na bancada foi resolvida documentalmente pela decisão de 10/11 em favor de snapshots JSON; o texto anterior está preservado na seção de rastreabilidade acima. O aceite do enquadramento FCPU continua pendente e os testes executáveis anteriores permanecem intactos. A tarefa
continua em andamento e não foi movida para `done/`.

## Referências

- Requisitos relacionados: REQ-OBC-011. Implementação não equivale a requisito validado.
- [payload/README.md](../../../payload/README.md)
- [payload/raspberry-pi-runbook.md](../../../payload/raspberry-pi-runbook.md)
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
