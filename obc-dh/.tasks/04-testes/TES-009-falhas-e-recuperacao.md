# TES-009 — Falhas, recuperação e integridade

**Estado:** Pendente
**Responsáveis:** OBC-DH, EPS e demais setores envolvidos

A execução dos novos ensaios será encaminhada ao setor de testes; os responsáveis técnicos acima apoiam interfaces, condições e diagnóstico.

## Objetivo

Comprovar recuperação e efeitos sobre dados/estados após falhas.

## Entrega

Executar os cenários abaixo separadamente, incluindo limites de detecção/recuperação e tratamento de tentativas esgotadas. Preservar entradas e resultados esperados existentes. Não alterar firmware para acomodar resultados sem diagnóstico e discussão.

## Pré-requisitos

- **Tarefas:** Por cenário, conforme tabela abaixo; não exigem conclusão conjunta para iniciar.
- **Condições externas:** Por cenário: mecanismo de recuperação disponível, montagem e critérios acordados.

## Cenários independentes

| Cenário | Pré-requisitos | Condições e verificação | Estado |
|---|---|---|---|
| SDA/SCL presos e retorno do I2C | [COM-003](../01-comunicacao/COM-003-recuperacao-i2c.md) | Método de injeção aprovado com EPS/ADCS; conferir tentativas, timeouts, pedidos pendentes e retorno do serviço. | Pendente |
| Desconexão e excesso UART | [COM-004](../01-comunicacao/done/COM-004-uart-payload.md) | Usar taxas e limites acordados; conferir recuperação do receptor e sinalização das perdas. | Pendente |
| Interrupção durante escrita no SD | [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Método acordado com EPS; avaliar arquivos incompletos e FAT32, não apenas fechamento normal ou conteúdo de um registro. | Pendente |
| Travamento, perda de comunicação e reinicialização | [INT-008](../03-integracao/INT-008-supervisao-e-retomada.md) | Medir detecção/recuperação, conferir falhas, horários e contagem de reinicializações; ausência ADS-B não significa enlace indisponível. | Pendente |
| Retomada persistente de missão, tempo e atuações | [INT-006](../03-integracao/INT-006-fluxo-de-dados.md), [INT-007](../03-integracao/INT-007-comandos-modos-e-protecoes.md), [INT-008](../03-integracao/INT-008-supervisao-e-retomada.md) | Conferir boots, referências, registros, saídas e Burn Wire após reset/interrupção, sem repetir atuação indevida nem inventar intervalo desligado. | Pendente |

## Critérios de conclusão

Todos os cenários aprovados, com versão, procedimento, entradas, resultados, tempos e análise dos registros/sistema de arquivos. Recuperação de um periférico não comprova recuperação completa do OBC.

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [COM-004](../01-comunicacao/done/COM-004-uart-payload.md) | Executar os cenários físicos de desconexão/excesso UART com taxas acordadas e verificar recuperação e perdas. | Pendente |
| [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Exercitar interrupção durante escrita e avaliar arquivos incompletos e FAT32; fsync lógico não comprova integridade física. | Pendente |
| [DAD-003](../02-dados-e-tempo/done/DAD-003-registros-e-interpretacao.md) | Comprovar retomada e integridade sob reset/interrupção real, incluindo estados operacionais; recuperação lógica do leitor não é reparo de FAT32. | Pendente |
| [TES-004](done/TES-004-uart-software.md) | Executar o cenário físico de excesso/desconexão UART; a saturação simulada não o substitui. | Pendente |
| [TES-007](done/TES-007-dados-e-tempo.md) | Comprovar boots, persistência temporal e integridade de registros/estados em resets e interrupções reais, preservando os cenários existentes. | Pendente |

## Prontidão por cenário — 10/11/2026

| Cenário original | Implementação prévia | Condição ainda necessária |
|---|---|---|
| SDA/SCL presos | COM-003 ainda pendente | Política, implementação e injeção aprovada por EPS/ADCS |
| Desconexão/excesso UART | COM-004 concluída localmente | Aceite do enquadramento, produtor/gerador compatível, taxas e montagem |
| Interrupção durante escrita SD | COM-005 concluída; diário INT-006 parcial disponível | Cartão, alimentação, método de corte e critérios com EPS |
| Travamento/perda de comunicação/reset | INT-008 ainda pendente | Supervisão, limites de saúde/recuperação e montagem |
| Retomada de missão/tempo/atuações | INT-006 aberta; INT-007/INT-008 pendentes | Estados duráveis, autorização, integração e critérios de retomada |

Encaminhar preparação dos cenários UART/SD. Simulações existentes ajudam a preparar o procedimento, mas não aprovam os casos físicos. Medições auxiliares de boot/NVS em ESP32 isolado não encerram retomada de missão ou segurança de atuações.

## Checklist de execução por cenário

- [ ] **Condições da injeção:** registrar falha aplicada, instante, duração, método seguro aprovado, limites de detecção/recuperação e efeito esperado sobre dados/saídas. Sem política ou limite aprovado, registrar impedimento e responsável; não inventar timeout de aceitação.
- [ ] **I2C preso e liberado:** após COM-003, aplicar SDA/SCL presos pelo método acordado; conferir sinalização, pedidos ativos/pendentes, tentativas e estado após esgotamento. Liberar a falha e verificar retorno segundo a política, sem acesso concorrente a `Wire`. Timeout por transferência não comprova recuperação do barramento ou callback travado.
- [ ] **UART desconectada:** interromper/reconectar o enlace com tráfego conhecido; conferir preservação da última lista válida, ressincronização e indicação de falha/perdas conforme o contrato aceito. Ausência de aeronaves ADS-B não deve ser classificada automaticamente como perda do enlace.
- [ ] **Excesso UART:** exceder o perfil acordado com gerador/ponta compatível e medir transporte, buffers, perdas e retorno ao tráfego nominal. `uart_full` conta observações de ring cheio, não mensagens perdidas; a fonte simulada que retém bytes não comprova tolerância ao sobrefluxo físico.
- [ ] **Corte durante escrita:** em cartão de ensaio e procedimento EPS aprovado, interromper em pontos identificados da gravação/sincronização. Preservar imagem/cópia antes de reparo; analisar arquivos, metadados FAT32 e prefixos recuperáveis. Um snapshot partido só é completo se ambas as partes e seu CRC forem válidos.
- [ ] **Reinício após falha de armazenamento:** verificar preservação dos arquivos anteriores, nome novo por boot/segmento, caudas incertas e contagens. Não exigir recuperação automática inexistente na primeira etapa INT-006; o aceite dessa recuperação depende de INT-008. Não apagar caudas para forçar sucesso.
- [ ] **Travamento e watchdog:** após INT-008, injetar o travamento acordado e medir detecção/ação/retorno. Esperar supervisão por progresso e estado previsto no ConOps; tarefa supervisora viva ou espera legítima por dados não devem ocultar ou fabricar falha.
- [ ] **Perda de comunicação com subsistemas:** usar critérios de saúde definidos por setor e verificar registro, tempos, tentativas e ação final. Distinguir sem dados de missão, sem TC recente e transporte realmente indisponível.
- [ ] **Boots e tempo:** conferir contagem NVS confirmada, novo boot e registros de falha. Não inferir intervalo desligado ou UTC do boot novo a partir da referência anterior; falha de reserva NVS deve ser explícita. Não confundir teste temporal simulado com resistência física da NVS ao corte.
- [ ] **Retomada de missão e atuações:** após as integrações correspondentes, verificar estado durável, modo e saídas após reset/corte, inclusive interrupção de atuação e comando repetido. Esperar ausência de nova atuação indevida; evento histórico ou flag volátil não autorizam Burn Wire.
- [ ] **Tentativas esgotadas:** para cada mecanismo implementado, conferir limite, diagnóstico, dados preservados e estado final definido, sem loop infinito de recuperação ou retorno fictício a saudável.

Registrar [evidências](../README.md#registro-por-cenário) com tempos, arquivos/imagens, esperado e observado por falha. Rejeição ou indisponibilidade prevista pode aprovar um caso; divergência exige diagnóstico antes de alteração de firmware ou critérios. Resultados parciais não concluem a task.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-007, REQ-OBC-009, REQ-OBC-010, REQ-OBC-011, REQ-NF-OBC-003, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
