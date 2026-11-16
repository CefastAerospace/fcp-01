# TODO — OBC-DH

**Conclusão INT-012 — 15/11/2026:** watchdog por progresso e reset em 1.500 ms
implementados no escopo ESP32/USB; 14 casos novos, memória/esperas armados,
regressões e 21 builds aprovados. `obc` restaurado/verificado, incluindo reset
comandado. Reprovação inicial e dois ajustes autorizados só na bancada
preservados; 48 validações anteriores intactas. TES-009 recebe versão e
procedimentos; seus critérios/estado permanecem. Retomada de voo INT-013.

**Conclusão de implementação — 15/11/2026:** INT-008 encerrada após diagnóstico
TT&C/memória, correção autorizada da suíte nova, sete verificações nativas,
45 casos TT&C, sete builds e bancadas/regressões USB aprovadas. `obc`
restaurado e verificado. Procedimentos/evidências na task; TES mantêm seus
estados e critérios. Watchdog/reset e retomada continuam nas INT-012/013.

**Revisão documental: 14/11/2026.** Implementação, contratos e comprovação independente têm critérios distintos. OBC mantém ferramentas e regressões; o setor de testes executa os ensaios. INT-009 a INT-013 e TES-011/012 foram criadas como **Pendente**. Nenhum estado anterior foi alterado, nenhum código/teste foi implementado ou executado e nenhum requisito foi aprovado por esta revisão.

As [regras e a correspondência de obrigações](../.tasks/README.md#revisão-vigente--14112026) definem os escopos redistribuídos. Estados: **Pendente** = não iniciada; **Em andamento** = iniciada; **Bloqueada** = impedimento registrado durante execução; **Concluída** = critérios próprios atendidos e arquivo em `done/`. Disponibilidade de uma entrega parcial não encerra sua task nem os ensaios.

Cada ID aparece uma vez nas seções 1–4. A coluna de capacidades identifica o que a entrega consome, não uma sequência obrigatória de encerramento integral das tasks. Preparação pode avançar com contratos disponíveis; integração/atuação mantém suas condições de engenharia. Resultados datados estão no histórico e nas tasks proprietárias.

## 1. Comunicação e armazenamento

| ID | Entrega | Capacidades prévias | Condições/encaminhamentos | Estado |
|---|---|---|---|---|
| COM-001 | [Serviço I2C: inicialização, executor e fila](../.tasks/01-comunicacao/done/COM-001-servico-i2c.md) | — | Comprovação concorrente/física em TES-002/003 | Concluída |
| COM-002 | [Prioridade dinâmica e ordenação I2C](../.tasks/01-comunicacao/done/COM-002-prioridade-dinamica-i2c.md) | COM-001 | Política nativa verificada; executor em TES-002 | Concluída |
| COM-003 | [Recuperação do barramento I2C após falhas](../.tasks/01-comunicacao/done/COM-003-recuperacao-i2c.md) | COM-001 | Consumidores INT-005/009; supervisão INT-008; watchdog INT-012; físico TES-009 | Concluída |
| COM-004 | [Transporte UART e recepção validada do Payload](../.tasks/01-comunicacao/done/COM-004-uart-payload.md) | — | Aceite/tráfego/ponta Pi INT-006; físico TES-005/009 | Concluída |
| COM-005 | [SD: gravação, leitura, extração e recuperação](../.tasks/01-comunicacao/done/COM-005-armazenamento-sd.md) | — | Boot INT-003; fluxo INT-006; recuperação local INT-008; retomada INT-013; físico TES-006/009 | Concluída |

## 2. Dados e tempo

| ID | Entrega | Capacidades prévias | Condições/encaminhamentos | Estado |
|---|---|---|---|---|
| DAD-001 | [Modelos de dados e formatos persistidos](../.tasks/02-dados-e-tempo/done/DAD-001-modelos-e-formatos.md) | — | Aceites/mapeamentos INT-004/005/006/009 | Concluída |
| DAD-002 | [Tempo monotônico, boots e referências absolutas](../.tasks/02-dados-e-tempo/done/DAD-002-tempo-e-boots.md) | — | Precisão física TES-008; retomada INT-013/TES-009 | Concluída |
| DAD-003 | [Codificação e interpretação dos registros](../.tasks/02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) | DAD-001/002 | Fluxo INT-006; estados de atuação INT-011; retomada INT-013; físico TES-009 | Concluída |

## 3. Integração e operação

| ID | Entrega | Capacidades prévias | Condições de engenharia / comprovação destinatária | Estado |
|---|---|---|---|---|
| INT-001 | [Configuração base do ambiente OBC](../.tasks/03-integracao/done/INT-001-ambiente-obc.md) | — | — | Concluída |
| INT-002 | [Conciliação de EN do motor no GPIO 12](../.tasks/03-integracao/done/INT-002-pinagem-en-motor.md) | — | Restrita às constantes; não aprova estado elétrico | Concluída |
| INT-003 | [Inicialização central e organização da execução](../.tasks/03-integracao/INT-003-inicializacao-central.md) | INT-001 | Bancada disponível; faltam definições/implementação do boot de voo. Comprovação TES-008 | Em andamento |
| INT-004 | [Integração de TT&C e umbilical](../.tasks/03-integracao/INT-004-ttc-e-umbilical.md) | Boot de bancada INT-003 | Adaptador disponível; aceite conjunto TT&C pendente. Comprovação RF/tempos TES-008 | Em andamento |
| INT-005 | [Integração ADCS, encoder e FOC](../.tasks/03-integracao/INT-005-sensores-e-adcs.md) | COM-001/002/003, INT-002 e boot INT-003 | Inventário, aquisição/FOC e política I2C ADCS. Comprovação TES-003/008/009 | Pendente |
| INT-006 | [Fluxo Payload–tempo–SD–telemetria](../.tasks/03-integracao/INT-006-fluxo-de-dados.md) | COM-004/005, DAD-003 e adaptador INT-004 | Fluxo/extração USB disponíveis; contratos operacionais/semântica pendentes. Comprovação TES-005/006/008/009 | Em andamento |
| INT-007 | [Comandos, modos e coordenação da missão](../.tasks/03-integracao/INT-007-comandos-modos-e-protecoes.md) | Consumidor INT-004 e estado/fluxo INT-006; ADCS INT-005 para suas ações | ConOps e comandos permitidos; proteções/atuações conectadas por INT-010/011. Comprovação TES-008/010 | Pendente |
| INT-008 | [Supervisão de recursos e recuperação local](../.tasks/03-integracao/done/INT-008-supervisao-e-retomada.md) | COM-003, boot INT-003, métricas INT-004 e diário INT-006 | Escritor, tarefas, I2C, TT&C e memória/pilhas concluídos; regressões preservadas, obc restaurado e pacote de ensaio disponível. Comprovação física TES-009; watchdog INT-012 e retomada INT-013 | Concluída |
| INT-009 | [Aquisição e integração EPS/Térmico](../.tasks/03-integracao/INT-009-aquisicao-eps-termico.md) | COM-001/002/003, DAD-001 e boot INT-003 | Drivers/contratos, endereços, unidades e aquisição dos setores. Comprovação TES-003/008/009 | Pendente |
| INT-010 | [Proteções energéticas e térmicas](../.tasks/03-integracao/INT-010-protecoes-energia-termico.md) | Dados INT-009, controle INT-005 e modos INT-007 | Cota EPS, limiares, prioridades e retorno aprovados. Comprovação TES-008/010 | Pendente |
| INT-011 | [Atuação e persistência do Burn Wire](../.tasks/03-integracao/INT-011-burn-wire-persistencia.md) | Boot INT-003, interlock INT-004, modos INT-007 e proteção INT-010 | Autorização, montagem, duração, confirmação e estados duráveis. Comprovação TES-008/009/010 | Pendente |
| INT-012 | [Watchdog e reinicialização do OBC](../.tasks/03-integracao/done/INT-012-watchdog-reset-obc.md) | Boot de bancada INT-003, consumidor INT-004 e diagnósticos INT-008 | Mecanismo/TC concluídos no ESP32/USB; prazos provisórios documentados, regressões aprovadas e obc restaurado. Comprovação TES-009; efeitos elétricos TES-008 e retorno operacional INT-013 | Concluída |
| INT-013 | [Retomada operacional após reset](../.tasks/03-integracao/INT-013-retomada-operacional.md) | INT-003/006/007/008/011/012; INT-010 nas decisões energéticas | ConOps e estados duráveis/seguros. Comprovação TES-008/009/010 | Pendente |

## 4. Testes e regressões

| ID | Entrega | Capacidades prévias | Executor / recurso ou definição necessária | Estado |
|---|---|---|---|---|
| TES-001 | [Teste nativo do escalonamento I2C](../.tasks/04-testes/done/TES-001-escalonamento-i2c.md) | COM-002 | Histórico preservado; não reabrir por esta revisão | Concluída |
| TES-002 | [Executor I2C: concorrência, fila e erros](../.tasks/04-testes/TES-002-executor-i2c.md) | COM-001/002 | OBC completa ferramenta sobre bancada existente; testes executa FreeRTOS/USB sem sensores | Pendente |
| TES-003 | [Comunicação I2C em hardware](../.tasks/04-testes/TES-003-i2c-hardware.md) | COM-001/002 | Testes; dispositivos, montagem e instrumentação por caso | Pendente |
| TES-004 | [Recepção UART: validação e limites](../.tasks/04-testes/done/TES-004-uart-software.md) | COM-004 | Histórico preservado; interoperabilidade TES-005, excesso físico TES-009 | Concluída |
| TES-005 | [UART entre ESP32 e Raspberry Pi](../.tasks/04-testes/TES-005-uart-hardware.md) | COM-004 | Testes; ponta Pi, aceite FCPU, tráfego e montagem | Em andamento |
| TES-006 | [SD: operações e falhas em hardware](../.tasks/04-testes/TES-006-sd-hardware.md) | COM-005 | Testes; cartão/módulo, alimentação e critérios EPS | Pendente |
| TES-007 | [Formatos e tempo: interpretação e limites](../.tasks/04-testes/done/TES-007-dados-e-tempo.md) | DAD-003 | Histórico preservado; precisão/retomada física TES-008/009 | Concluída |
| TES-008 | [Boot, comandos, telemetria e convivência](../.tasks/04-testes/TES-008-integracao.md) | Por cenário; correspondência atual no arquivo | Testes; condições/montagem e limites por caso | Pendente |
| TES-009 | [Falhas, recuperação e integridade](../.tasks/04-testes/TES-009-falhas-e-recuperacao.md) | Por cenário; watchdog INT-012, retomada INT-013 | Testes; política, injeção e montagem por caso | Pendente |
| TES-010 | [Missão completa e cobertura dos requisitos](../.tasks/04-testes/TES-010-missao-e-cobertura.md) | Integrações de missão, incluindo INT-009 a INT-013; TES-001 a TES-009 | Testes; referências/limites vigentes e montagem integrada | Pendente |
| TES-011 | [Execução reproduzível das regressões OBC](../.tasks/04-testes/TES-011-regressoes-reproduziveis.md) | Testes e comandos existentes | OBC; compilador/Python e dependências locais; sem placa | Pendente |
| TES-012 | [Regressão do consumidor de telecomandos OBC](../.tasks/04-testes/TES-012-consumidor-telecomandos.md) | Consumidor atual INT-004/006 | OBC; reutilizar roteiros atuais, ESP32/USB quando aplicável | Pendente |

## 5. Trabalho OBC-DH com ESP32 e USB

| Prioridade | Entrega prática | Limite e condição de encerramento |
|---|---|---|
| 1 | TES-011: organizar execução das regressões nativas existentes | Sem placa; preservar vetores/expectativas, detectar falhas e dependências ausentes. É ferramenta de desenvolvimento OBC. |
| 2 | TES-002: completar ferramenta do executor; TES-012: cobrir lacunas do consumidor atual | OBC implementa ferramentas e regressões; setor de testes mantém a comprovação formal TES-002/008/009. |
| 3 | INT-012 concluída: entregar mecanismo de watchdog/reset à TES-009 | Versão, parâmetros provisórios, injeções e relatórios disponíveis. Não declara retomada operacional ou saída elétrica segura. |
| 4 | INT-004/006: fechar contratos operacionais e implementar ajustes acordados | Aceites e semântica continuam engenharia; não transferir decisões ao setor de testes nem inventar contrato unilateralmente. |
| 5 | INT-003/005/007/009/010/011/013: implementar após suas entradas disponíveis | A indisponibilidade de hardware não bloqueia toda lógica; contratos elétricos/FOC/energia/atuação indispensáveis continuam condições reais. |

A ordem orienta trabalho, não cria dependências novas. Os recortes anteriores de memória, cliente real, extração, estresse, escritor e diagnóstico não precisam ser recriados. Procedimentos atuais estão no [guia](../tools/README.md); mensagens já preparadas em [encaminhamentos de 11/11](../encaminhamentos-2026-11-11.md), sem envio externo.

## 6. Encaminhamento ao setor de testes

OBC entrega versão, ferramenta, regressões pertinentes e instruções compatíveis. Testes executa e registra acertos, falhas e impedimentos; OBC diagnostica/corrige, e testes repete casos afetados mantendo os critérios. O executor formal de cada TES aparece em seu cabeçalho; TES-011/012 são desenvolvimento OBC.

| Cenário | Prontidão / condição de execução |
|---|---|
| TES-001/004/007 | Históricos concluídos preservados; regressões novas não reabrem ou aprovam ensaios físicos por inferência. |
| TES-002 | Preparação liberada, sem sensores; OBC completa a ferramenta e testes executa o checklist original. |
| TES-003/005/006 | Preparar por dispositivo/interface; executar com montagem, ponta Pi/cartão, contratos e instrumentos necessários. |
| TES-008: USB/fluxo/tempo | Recortes locais disponíveis; manter seus limites e a distinção em relação à montagem real. |
| TES-008: demais casos | Boot INT-003, ADCS INT-005, aquisição INT-009, modos INT-007, proteção INT-010 e Burn Wire INT-011 conforme o caso. |
| TES-009: recuperação local | INT-008 concluída, com ferramentas/evidências no guia; comprovação por caso mantém montagem, método de injeção e critérios necessários. |
| TES-009: watchdog e retomada | Watchdog/reset INT-012 disponível com versão/roteiros; montagem e comprovação por caso. Retomada INT-013 e atos críticos INT-011 continuam pendentes. |
| TES-010 | Preparar procedimento/rastreabilidade; missão completa ainda não liberada pelas integrações e ensaios pendentes. |

Nenhuma linha aprova ensaio ou altera entradas/resultados esperados. A [matriz dos 16 requisitos](../.tasks/README.md#cobertura-dos-requisitos) identifica separadamente implementação e comprovação.

## Histórico anterior à redistribuição de 14/11/2026

Os registros abaixo preservam os escopos e pendências da época. Watchdog e retomada operacional citados como partes da INT-008 passam às INT-012/013; não são pendências duplicadas nem evidências novas.

**I2C, recorte local concluído em 14/11/2026:** diagnóstico de callback
>100 ms e recuperação >200 ms implementado. Teste portátil, regressões
nativas, cinco builds e bancada USB passaram. Após duas correções autorizadas
na bancada (namespace e espera monotônica mínima), expectativas permanecem
intactas. Regressões USB originais de I2C, supervisão e downlink aprovadas;
`obc` restaurado e verificado. Reprovação inicial e reexecução preservadas
na INT-008, ainda aberta por watchdog, demais recursos e retomada operacional.

**Atualização local: 14/11/2026.** Supervisão diagnóstica das três tarefas
do fluxo implementada: suspeita após exceder 100 ms na UART, 2 s no
processamento e 5 s por operação SD, sem reset/recuperação automática de
tarefas. Verificação nativa com ASan/UBSan, cinco builds e bancada ESP32/USB
passaram. Cada tarefa produziu um par suspect/ok; varredura de 6.465 ms e
espera de recuperação SD sem falso alerta. Regressões originais preservadas
e aprovadas; `obc` restaurado e verificado sem cartão. Recorte local concluído;
INT-008 permanece Em andamento pelos demais recursos, watchdog e operação.
Os recortes concluídos e as evidências de 13/11 são preservados.

**Atualização local: 13/11/2026, base `777b5f8`.** Recorte autorizado da
INT-008: retomada do escritor SD implementada e verificada em arquivos POSIX
e ESP32/USB com backend simulado. Até três tentativas por episódio, intervalo
de 5 s, renovação após escrita normal e nenhum reboot automático. A bancada
nova aprovou duas retomadas e um esgotamento; regressões originais preservadas.
A revisão reproduziu bloqueio de downlink ao reconsultar com 32 entregas
não persistidas. Após autorização, uma reserva durável adicional corrigiu
o caso sem reduzir admissões ou descartar cópias. A mesma verificação passou
no PC/ESP32, com regressões preservadas. Recorte local concluído; INT-008
mantém supervisão/modos e ensaios físicos pendentes. Evidências na tarefa.

**Atualização: 11/11/2026, alterações locais sobre `f7af605`.** O firmware central integra entregas identificadas, eventos TX, confirmações do solo e extração `request_log` pelo TT&C. Após corrigir a admissão de memória, a bancada USB de entregas/extração e o firmware normal passaram com o roteiro original. Testes nativos, quatro builds, relatórios e histórico da reprovação estão na INT-006. Aceites, operação, hardware restante, sensores, modos e supervisão continuam abertos.

**Limites e versão:** Espressif32 fixado em 6.6.0; dez ambientes compilados
e regressões nativas/USB preservadas e aprovadas. Saturação na placa:
32 admissões, oito perdas contabilizadas, FIFO confirmado e nova admissão
após esvaziamento. Memória: JSON de 65.536 bytes e snapshot denso aceitos;
NO_MEMORY preservou o snapshot e permitiu retomada. Heap interno mínimo
de 1.040 bytes no esgotamento. Evidências na INT-006; obc restaurado.

**Estresse sem periféricos:** os 17 testes TT&C existentes passaram no
ESP32 com transportes falsos. A bancada USB adicional `int006-stress-check`
passou com timeout/busy/erro parcial, confirmação desconhecida/duplicada/
atrasada e NO_SPACE simulado. Sete entregas confirmadas, ACK máximo de
1.060,223 ms e q máximo de 32 ms. obc restaurado e verificado. Procedimento
e evidências na INT-006; não encerra ensaios físicos nem durabilidade.

**Admissão de memória:** a correção aprovada distingue o teto do parser
(98.304 bytes, com NO_MEMORY previsto) das pilhas e margem de inicialização
(23.552 bytes). A admissão usa heap interno acessível por byte e maior bloco.
O parser, os buffers e os testes não mudaram. A placa ficou com `obc` restaurado.

**Cliente real e TM/tempo:** `ObcStationCheck.py` passou com um ESP32/USB
na bancada e no firmware normal. APIs reais da estação confirmaram sete
entregas e baixaram três arquivos interpretados pelo leitor OBC. UTC
desconhecido, época zero, referência atual, rejeições e ac=7/depois 0 foram
verificados. Evidências na INT-006; durabilidade da estação e periféricos
reais continuam pendentes. A placa permaneceu com `obc`.
