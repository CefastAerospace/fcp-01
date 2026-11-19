# INT-007 — Comandos, modos e coordenação da missão

**Estado:** Em andamento
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos contratos:** coordenação, ADCS, EPS, Térmico e TT&C

## Objetivo

Coordenar comandos e modos conforme o ConOps, validando argumentos e condições antes de executar ações.

## Redistribuição de 14/11/2026

O escopo anterior reunia modos, proteção energética/térmica, Burn Wire e retorno após reset. As responsabilidades passam a ser:

| Entrega | Destino |
|---|---|
| Comandos, transições e coordenação da missão | Esta INT-007 |
| Proteções energéticas/térmicas, prioridades e retorno | [INT-010](INT-010-protecoes-energia-termico.md) |
| Burn Wire, autorização, interlock, duplicatas e estados persistentes recebidos de DAD-003 | [INT-011](INT-011-burn-wire-persistencia.md) |
| `CMD_RESET_OBC` | [INT-012](done/INT-012-watchdog-reset-obc.md) |
| Retomada operacional de modos/missão após reset | [INT-013](INT-013-retomada-operacional.md) |

Todas as obrigações permanecem pendentes nas destinatárias. A revisão não implementa modos nem altera critérios dos ensaios.

## Entrega

Validar argumentos, modo e condições antes de executar cada TC; responder com ACK/rejeição pelo consumidor existente. Integrar transições, início/fim da missão e coordenação do detumbling. Definir no núcleo de modos as condições e a coordenação que as INT-010/011 consomem. Essas destinatárias integram suas proteções/atuações ao núcleo e verificam os efeitos, sem criar dependência circular de encerramento. Desabilitar motor fora do detumbling conforme a interface ADCS; consumo residual é medido pelo setor de testes, sem presumir zero.

## Pré-requisitos

- **Tarefas/capacidades:** consumidor/ACK da INT-004 e estado/fluxo da INT-006, já disponíveis nos recortes registrados. Definir a lógica de modos a partir do contrato sem exigir conclusão de escopos independentes dessas INT.
- **Integração por ação:** comandos ADCS consomem a interface da INT-005; decisões de energia/temperatura consomem INT-009/010; atuação Burn Wire consome INT-011. Ausência de recurso não autoriza indicar que a ação foi executada.
- **Condições externas:** ConOps, estado inicial, transições, comandos permitidos, duração/perfil de missão e condições de atuação aprovados pelos responsáveis.

## Definições e condições

- Reutilizar `common/protocol/ttc-json.md` e `tt-c/include/TtcProtocol.h`: envelope TC no TT&C, argumentos/condições no OBC e `SendAck()` existente. Não presumir PUS; satélite e estação precisam da mesma versão.
- A biblioteca deduplica oito TCs por 30 s; isso não substitui persistência de atuações críticas, transferida à INT-011.
- Integrar modos/transições e estado inicial do README geral/contrato TT&C, conferindo ConOps vigente. Preservar LOW-POWER com rádio em RX e beacon periódico somente em safe-mode.
- Definir se alinhamento do motor no boot é permitido e em quais condições com INT-003/005. Confirmar “tumbling” nos requisitos versus “detumbling” nas demais referências, sem alterar o requisito silenciosamente.
- Detalhar comandos permitidos e prioridades por modo. Proteção térmica e watchdog de tarefas são mecanismos distintos, com INT-010 e INT-012 proprietárias.

## Verificação de desenvolvimento

Cobrir transições válidas e recusadas, argumentos inválidos sem efeitos, recurso indisponível, falha da ação e coordenação de início/fim de missão. Casos novos acompanham a implementação; não alterar as expectativas dos comandos ainda não implementados em roteiros históricos.

## Critérios de conclusão

Comandos e transições do próprio escopo implementados conforme contratos, efeitos/recusas integrados, build e regressões pertinentes aprovados. A interface ADCS deve estar disponível para concluir seus comandos. O núcleo não exige encerrar INT-010/011: elas implementam e verificam as próprias extensões ao núcleo já entregue. Comando ainda não implementado não pode receber sucesso fictício; não criar módulos vazios. Comprovação física de modos, energia e atuações continua nas TES-008/009/010; sua aprovação não é condição de encerramento desta implementação.

## Encaminhamento DAD-003

A obrigação de estados duráveis de atuações/Burn Wire, autorização, confirmação e eventual extensão versionada de registros foi transferida integralmente à INT-011. A retomada geral que consome esses estados está na INT-013. Eventos existentes não substituem estado de retomada.

## Evidências

### Recorte de desenvolvimento

O coordenador portátil é atendido pelo loop OBC. O consumidor TT&C valida
`args`, aplica as transições e publica o mesmo modo na TM JSON e no diário
TM v3. `set_mode safe`, `detumb` e `mission` seguem as mesmas condições dos
comandos diretos. `exit_safe` recupera `LOW-POWER` para `SAFE/INIT` mediante
autorização. `sun`, `bench` e `lowpwr` são destinos recusados por TC.

A janela ADS-B abre após os checks de boot, fluxo ativo, snapshot recente,
UTC no boot atual e admissão do evento de início. Encerra por parada, prazo
monotônico de 600 s, perda de Payload/diário ou proteção. O código OBC 267
registra modo e causa no diário v1. O fluxo de snapshots continua fora da
janela. A falha de parada ADCS bloqueia novos inícios até a confirmação do
motor desabilitado.

O [procedimento](../../tools/README.md#modos-e-janela-ads-b-da-int-007) usa
`int007-modes-check`, com ADCS/proteção simulados e entrada/SD da INT-006.
Frescor de 4 s é provisório e exclusivo da bancada. Em voo, falta a seleção
de perfil da INT-003, a confirmação do Payload contínuo e de sua cadência,
o aceite da matriz pela coordenação e a interface real da INT-005. O ADCS
deve definir eixo, validade, duração da condição de término, alinhamento e
prazo de confirmação de parada. SUN-POINTING aguarda acordo de ADCS, EPS e
coordenação sobre a atuação da roda. A INT-010 fornece proteção e recuperação.

### Verificações de 19/11/2026

Base `9731b65` com as alterações locais da INT-007. Builds `obc`,
`int006-ttc-check` e `int007-modes-check` concluídos. `ObcModesCheck` e
`ObcDataFlowCheck` passaram com ASan/UBSan. A regressão da TM v3 passou
com os vetores originais e o leitor C++.

A [execução USB 01](../../examples/int007-2026-11-19/01-modes-usb.json)
parou após 26 ACKs, com `q` máximo de 54 ms e RTT máximo de 116,64 ms.
Confirmou abertura/parada da janela, duplicata de início sem reexecução,
snapshots escritos antes e depois da janela, e `mission` na TM JSON e no
diário. Confirmou o início e a parada ADCS simulados, a transição para
`lowpwr` e a recusa de `exit_safe` durante a proteção.

O marcador `detumb>lowpwr protection` recebeu `queued=0` a 7.068 ms. O
escritor simulado continuou disponível e sem falhas de escrita. A fila
compartilhada de oito registros curtos recebia TM, eventos e metadados com
o escritor atrasado em 100 ms por chamada. A saturação é a causa provável.
O roteiro esperava o marcador no diário e expirou. O caso de recuperação
por `exit_safe` ficou pendente na placa. A execução preserva `passed: false`.

O ESP32 voltou ao perfil `obc`. O roteiro original `ObcStationCheck.py`
passou com 64 ACKs, 37 TMs, `q` máximo de 116 ms e RTT máximo de 156,4 ms.

A [execução USB 02](../../examples/int007-2026-11-19/02-modes-usb.json)
passou com o mesmo binário. O roteiro aguardou a gravação da parada ADCS
antes de enviar o comando que dispara a proteção. O diário registrou a
entrada em `lowpwr` e o retorno a `safe` por `exit_safe`. Foram 28 ACKs e
7 TMs, com `q` máximo de 54 ms e RTT máximo de 137,58 ms. Os telecomandos
e suas respostas esperadas foram os mesmos da execução 01.

Após a execução 02, `obc` foi restaurado novamente. `ObcStationCheck.py`
passou com 64 ACKs, 37 TMs, `q` máximo de 116 ms e RTT máximo de 156,9 ms.

- SHA-256 `int007-modes-check` `8b58622e784a9b65fb06546fc732538f81b75ee929e551ac92563a5ee8ed1e84`
- SHA-256 `obc` restaurado `e8dbf12d1d7f7bc362c884bcdda8c45d9994b30b7660c96fd6198afe5c0f3e51`

## Referências

- REQ-OBC-001/002/004/012 e REQ-NF-OBC-001 conservam implementações complementares nas novas destinatárias; [matriz](../README.md#cobertura-dos-requisitos).
- [Contrato TT&C](../../../common/protocol/ttc-json.md), [pinagem](../../.agents/PINAGEM.md), [TES-008](../04-testes/TES-008-integracao.md), [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md), [TES-010](../04-testes/TES-010-missao-e-cobertura.md).
- Origens anteriores preservadas na correspondência: P-ADCS, P-BURN-WIRE, P-DADOS, P-EPS-ELETRICA, P-MISSAO, P-TERMICO.
- [Painel](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
