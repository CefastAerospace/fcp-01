# TES-006 — SD: operações e falhas em hardware

**Estado:** Concluída
**Responsável pela execução e comprovação:** Setor de testes
**Responsáveis técnicos de apoio:** OBC-DH e EPS
**Responsável pelas ferramentas e regressões de desenvolvimento:** OBC-DH

A execução e o relato dos ensaios cabem ao setor de testes; a análise e as correções de firmware cabem ao responsável OBC-DH, com apoio dos setores envolvidos. Seguir as [responsabilidades e regras de reexecução](../../README.md#responsabilidades-correção-e-reexecução).


## Encerramento da bancada — 19/11/2026

Ficha movida para `done/` a pedido do OBC-DH após aprovação do teste de
cartão cheio, da conferência pelo leitor e da retomada. Os resultados
se aplicam à montagem USB em bancada. A conferência da alimentação
integrada com EPS e o aceite independente pelo setor de testes seguem
pendentes. A injeção física isolada de falha de sincronização depende
de método aprovado.

## Prontidão vigente — 14/11/2026

`request_log` e retomada local limitada do escritor estão implementados nos recortes das INT-006/008. Retomada completa de missão continua pendente na INT-013; não confundir essas entregas. Os casos abaixo, inclusive retomada explícita End/Init, preservam integralmente suas entradas e expectativas. Esta atualização corrige disponibilidade de ferramentas, sem aprovar cartão, FAT32 ou corte de energia.

## Objetivo

Comprovar leitura, gravação e extração de arquivos na montagem real.

## Entrega

Exercitar escrita/leitura, sincronização/fechamento, extração, cartão cheio/indisponível e limites de buffer com conteúdo conhecido. Registrar perdas e efeitos sobre arquivos/FAT32. Interrupções durante escrita serão exercitadas em [TES-009](../TES-009-falhas-e-recuperacao.md); a interpretação dos registros de missão está em [TES-007](TES-007-dados-e-tempo.md)/[TES-008](../TES-008-integracao.md).

## Pré-requisitos

- **Tarefas:** [COM-005](../../01-comunicacao/done/COM-005-armazenamento-sd.md)
- **Condições externas:** Cartão, montagem, alimentação e critérios de integridade definidos.

## Critérios de conclusão

Conteúdo extraído corresponde ao conteúdo de teste conforme os critérios acordados, falhas são observáveis e limites estão registrados. Não concluir integridade em queda de energia apenas por fechamento normal.

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [COM-005](../../01-comunicacao/done/COM-005-armazenamento-sd.md) | Conferir módulo, montagem e alimentação com EPS, frequência/latência e operações do SD na montagem real, incluindo cheio/indisponível e extração. | Operações e latências verificadas em bancada USB. Integração com EPS pendente. |

## Prontidão e preparação para o setor de testes

COM-005 está concluída. Encaminhar preparação; execução depende de cartão/módulo, montagem, alimentação e critérios com EPS. A primeira etapa INT-006 já grava registros, mas a bancada com backend simulado não comprova cartão físico. Para esta tarefa, extração pode usar `Read` de arquivo fechado, cópia pelo leitor físico ou `request_log`, já integrado no OBC. O recorte USB com backend simulado não comprova extração do cartão real.

Usar cartão de ensaio e conteúdo conhecido, preservando dados anteriores. Registrar capacidade, formatação FAT32, módulo, alimentação, pinagem HSPI e frequência. A frequência inicial de 4 MHz é configurada, não uma garantia de funcionamento da montagem. Interrupção durante escrita permanece na TES-009.

## Checklist de execução

- [x] **Montagem nominal e ausência:** energizar a montagem aprovada com cartão FAT32 e repetir sem cartão. Esperar disponibilidade ou erro explícito conforme a condição, sem formatação, reparo ou exclusão automática.
- [x] **Criação exclusiva:** gravar conteúdo conhecido em nome novo e tentar reutilizá-lo. Esperar recusa do nome existente e preservação byte a byte do arquivo anterior.
- [x] **Escrita, sincronização e fechamento:** gravar blocos conhecidos, conferir cada `RESULT` e fechar. Esperar sucesso apenas após escrita completa/sincronização no runtime; não tratar `transferred == tamanho` como confirmação em status de erro.
- [x] **Leitura e limites:** ler arquivo fechado em blocos, incluindo offsets internos, EOF e offset após EOF. Esperar bytes corretos, EOF com zero bytes e recusa de offset inválido; leitura do arquivo ainda aberto para escrita deve retornar `BUSY`.
- [x] **Extração reproduzível:** obter cópia estável e comparar tamanho e todos os bytes com o conteúdo conhecido. Para registros de missão, usar o leitor atualizado e registrar os resultados sem sobrescrever a origem; interpretação de missão completa continua em TES-008.
- [x] **Cartão cheio/indisponível:** aplicar procedimento aprovado e registrar erros/tentativas. Esperar indisponibilidade observável e preservação dos arquivos anteriores, sem apagar dados para liberar espaço automaticamente.
- [ ] **Falhas de escrita/sincronização:** quando houver método físico aprovado, observar tentativas e cauda do arquivo. O serviço admite três tentativas totais no mesmo offset confirmado; falha final deve impedir novas escritas. Uma falha real não injetável permanece sem execução física, mesmo que o teste POSIX tenha passado.
- [x] **Retomada explícita do serviço:** após falha terminal, verificar que repetir `Init` não a oculta. No procedimento de bancada, usar `End` + `Init` e nome novo, preservando o arquivo anterior. Recuperação automática de missão não está implementada e pertence à INT-008/TES-009.
- [x] **Acesso concorrente e duração:** exercitar a disputa permitida na bancada, esperando `BUSY` sem acesso intercalado ao driver. Medir latências com a montagem real; limites de aplicação dependem dos acordos de integração.

Registrar falhas, perdas e efeitos observados nos arquivos/FAT32 no formato de [evidências](../../README.md#registro-por-cenário). Fechamento normal e `fsync` não aprovam integridade em corte de energia.

## Evidências

### Bancada USB com microSD de 8 GB — 19/11/2026

Usado o ambiente `tes006-sd-check` sobre `9731b65` com alterações locais.
O perfil chama o `ObcStorageEsp32` real. ESP32 DevKit V1 ligado por USB,
com SCK 15, MISO 35, MOSI 32 e CS 13. O serviço configura 4 MHz para as
transferências. O leitor externo confirmou FAT32 e capacidade de
8.032.092.160 bytes. Modelo do módulo ESP32 pendente de registro.
[Procedimento](../../../tools/README.md#bancada-sd-física-da-tes-006).

| Caso | Resultado e evidência |
|---|---|
| Montagem nominal `b6a1` | Duas tentativas retornaram `MOUNT_FAILED` (`status=11`). O driver reportou `Card Failed! cmd: 0x00` e `f_mount failed: (3) The physical drive cannot work`. `End` retornou `OK` em ambas. [Execução 01](../../../examples/tes006-2026-11-19/01-nominal.json), [execução 02](../../../examples/tes006-2026-11-19/02-nominal-repeat.json). |
| Operações de arquivo e concorrência | Bloqueadas pela montagem no perfil TES-006. Nas duas execuções, o roteiro parou antes de `OpenNew`. |
| Ausência | Passou com o cartão no leitor externo. `Init` retornou `MOUNT_FAILED` e `End` retornou `OK`. [Execução 04](../../../examples/tes006-2026-11-19/04-absent.json). |
| Leitor externo | O leitor USB `SD/MMC/MS PRO` reconheceu o FAT32 e permitiu listar `obc/`, vazio. O kernel reportou `Write Protect is on`, com `RO=1` no dispositivo. A escrita de conferência ficou bloqueada pela montagem somente leitura. |
| Cartão cheio, falha terminal e retomada | Aguardam a verificação nominal e o preparo do espaço livre no leitor. |

Após desconectar e reconectar a alimentação USB, o perfil respondeu e
voltou a registrar `MOUNT_FAILED` em `CMD0`, com `End = OK`. O cartão
permaneceu no módulo do ESP32 durante essa tentativa.
[Execução 03](../../../examples/tes006-2026-11-19/03-power-cycle.json).

Após liberar a trava no adaptador do leitor, o kernel informou
`Write Protect is off` e `RO=0`. Um arquivo temporário novo recebeu
`ABCDEF`, foi sincronizado e releu os seis bytes `414243444546`.
O arquivo temporário foi removido e o leitor desmontado. O resultado
se refere ao cartão no PC. Após recolocar o cartão, o perfil isolado
reproduziu a falha em `CMD0` antes de abrir arquivos. O `obc` restaurado
reproduziu `MOUNT_FAILED` no boot e após `reset_obc`. Em ambos os boots,
`ping` passou e `request_log` retornou `no_sd`. A confirmação de alimentação
e montagem do módulo microSD permanece pendente. [Execução 05](../../../examples/tes006-2026-11-19/05-nominal-reseat.json), [execução 06](../../../examples/tes006-2026-11-19/06-obc-restored.json).

Com a pinagem informada mantida e após desligar e religar pela USB, o log
capturou `POWERON_RESET`. O `obc` respondeu a `ping`, mas falhou novamente
em `CMD0` com `MOUNT_FAILED` e devolveu `no_sd` na listagem.
[Execução 07](../../../examples/tes006-2026-11-19/07-obc-power-cycle.json).

O responsável confirmou a pinagem informada e a compatibilidade do módulo
com alimentação de 3,3 V. Com a placa ligada e o cartão instalado, a
leitura informada entre `VCC` e `GND` no módulo foi de aproximadamente
3 V. O responsável informou a mesma leitura entre `3V3` e `GND` do ESP32
na mesma condição. A continuidade dos quatro fios SPI permanece pendente.

### Verificação funcional na montagem atual

O roteiro original de telecomandos e extração passou no cenário `no_sd`.
Na bancada física, o caso nominal `b6a2` voltou a falhar em `CMD0` antes de
`OpenNew`, com `MOUNT_FAILED` e `End = OK`. Gravação, leitura, extração e
concorrência no ESP32 permanecem sem comprovação. [Telecomandos](../../../examples/tes006-2026-11-19/08-functional-downlink.json), [nominal físico](../../../examples/tes006-2026-11-19/09-functional-nominal.json).

O ESP32 ficou com `obc` restaurado. O roteiro original da estação passou
com 64 ACKs e 37 TMs, `q` máximo de 84 ms, RTT máximo de 134,9 ms e maior
intervalo de poll de 2 ms. A listagem retornou `no_sd` conforme o cenário
executado. Os binários nativos de armazenamento, modos e proteção também
passaram. [Estação via USB](../../../examples/tes006-2026-11-19/10-functional-station.json).

### Reteste com montagem bem-sucedida

O `obc` montou o cartão (`status=0`). O escritor ficou inativo porque a
reserva do buffer de snapshots falhou. O roteiro de telecomandos passou
no cenário `no_sd`. [Execução 11](../../../examples/tes006-2026-11-19/11-retest-obc.json).

No perfil `tes006-sd-check`, o caso `b6a3` montou o cartão e gravou
`ABC` e `DEF` em `t6b6a3n.bin`, com `Append` e `Close` retornando `OK`.
Reabrir o nome retornou `ALREADY_EXISTS`. A leitura em blocos recuperou
`ABCDEF`, com EOF correto, offset além do EOF recusado e `BUSY` ao ler o
arquivo aberto. A comparação independente pelo leitor permanece pendente.

Na disputa, todas as 20 escritas de 4.096 bytes foram confirmadas e
reverificadas. As 20 leituras concorrentes devolveram `BUSY`. O roteiro
exige também ao menos uma leitura bem-sucedida durante a disputa e
terminou com `passed: false`. A execução foi preservada. `End` retornou
`OK`. O perfil de bancada permanece no ESP32 para os casos restantes.
[Execução 12](../../../examples/tes006-2026-11-19/12-retest-nominal.json).

Na verificação anterior com `obc`, o cartão montou após `reset_obc`. Nesse
boot o escritor ficou inativo após falhar a reserva do buffer de snapshots.
O `no_sd` da extração em `obc` e a falha de `CMD0` nas execuções da bancada
são observações distintas. O `CMD0` antecede a leitura do sistema de
arquivos. A causa elétrica ou do cartão permanece em investigação.

Builds `obc` e `tes006-sd-check` concluídos. A regressão POSIX original
`ObcStorageCheck` passou com UBSan e os vetores originais. O SHA-256 do
binário de bancada foi
`6dec2e2333561ee5301b1e2803ede8e31bbf6a57fe9ee5b82519510b318f8dd2`.

### Reteste de concorrência na bancada USB

O perfil `tes006-sd-check` agora pareia uma leitura e uma escrita por
notificações FreeRTOS. A cada rodada, uma tarefa começa sem espera e a
outra aguarda dois ticks. A precedência alterna, e ambos os resultados
são coletados antes da próxima rodada. Permanecem as 20 chamadas por
tarefa, os blocos de 4.096 bytes e os critérios da execução `b6a3`.

Os builds `obc` e `tes006-sd-check` e a regressão POSIX `ObcStorageCheck`
com UBSan passaram. SHA-256 do novo binário de bancada:
`3300776f48dc39986ea643c92c98486079a1ba7df4d9a5150bd298008e436a9c`.

O nominal `b6a4` passou no ESP32-D0WD-V3. A rodada registrou 10 leituras
válidas, 10 escritas confirmadas e 20 retornos `BUSY`. Os 40.960 bytes
gravados foram conferidos após `Close`. As leituras concorrentes válidas
levaram 6,228–11,761 ms e as escritas confirmadas, 27,031–29,265 ms.
O roteiro do PC levou cerca de 2,5 s incluindo a abertura da porta e a
coleta serial. A leitura por USB retornou `414243444546` e `End` retornou
`OK`. [Execução 13](../../../examples/tes006-2026-11-19/13-paired-nominal.json).

Com o cartão no leitor, o reteste de ausência `b6a5` retornou
`MOUNT_FAILED` em `Init` e `OK` em `End`. A extração independente de
`t6b6a3n.bin` e `t6b6a4n.bin` no volume `AC31-7988` confirmou, nos dois
arquivos, os seis bytes `ABCDEF` e os mesmos blocos registrados por USB.
Os tamanhos e hashes dos quatro arquivos anteriores em `obc/` foram
registrados para a comparação após o ensaio de cartão cheio.
[Ausência](../../../examples/tes006-2026-11-19/14-paired-absent.json),
[extração pelo leitor](../../../examples/tes006-2026-11-19/15-card-reader-extraction.json).

### Cartão cheio e retomada

O leitor verificou o volume removível `AC31-7988`, FAT32, com
8.032.092.160 bytes. O preenchimento temporário da rodada `b6a6` ficou
em `T6B6A6/`, fora de `obc/`, deixando uma unidade de alocação de
4.096 bytes e espaço para duas entradas curtas no diretório `obc/`.
O volume foi desmontado antes da transferência para o ESP32.
[Preparo](../../../examples/tes006-2026-11-19/16-full-card-prepared.json).

O cenário `full` da rodada `b6a6` parou em `Init`, com `MOUNT_FAILED`
(`status=11`) e `f_mount` retornando `3` (`FR_NOT_READY`). `End`
retornou `OK`. A chamada `OpenNew` não chegou a executar, portanto a
rodada não exercitou o limite de espaço, as tentativas ou a retomada.
O erro de montagem também ocorreu antes do preenchimento. A causa da
intermitência permanece sem confirmação. O preenchimento temporário
permaneceu no cartão ao terminar a rodada.
[Execução 17](../../../examples/tes006-2026-11-19/17-full.json).

O reteste autorizado `b6a7`, após religar a placa e registrar
`POWERON_RESET`, repetiu `FR_NOT_READY` em `Init` e `OK` em `End`.
As duas rodadas permanecem como evidência de falha de montagem.
[Execução 18](../../../examples/tes006-2026-11-19/18-full-power-cycle.json).

No leitor, `obc/` continha os mesmos quatro arquivos, com os tamanhos e
hashes originais. As rodadas `full` não criaram arquivo nem cauda para
análise. A remoção exclusiva de `T6B6A6/` devolveu as 1.956.827 unidades
livres anteriores ao preparo. O cartão foi desmontado.
[Conferência e limpeza](../../../examples/tes006-2026-11-19/19-full-card-cleanup.json).

Após recolocar o cartão, `recovered` (`b6a8`) montou, escreveu `ABCDEF`
em nome novo, confirmou a leitura por USB e encerrou com `OK`. O resultado
comprova operação após liberar espaço. A retomada no mesmo boot após
falha terminal de escrita continua pendente, pois as rodadas `full`
pararam na montagem.
[Reteste após liberar espaço](../../../examples/tes006-2026-11-19/20-after-space-restored.json).

O firmware `obc` foi restaurado. O SD montou no boot e `ping` passou.
O escritor permaneceu inativo com a falha de reserva de RAM já observada,
e `request_log` retornou `no_sd`. O resultado pertence à integração do
escritor. O cartão ficou no módulo do ESP32.
[Verificação do `obc`](../../../examples/tes006-2026-11-19/21-obc-restored-smoke.json).

### Cartão cheio com montagem bem-sucedida

O reteste `b6a9` usou o mesmo binário de bancada e o volume `AC31-7988`,
preparado com uma unidade de 4.096 bytes livre. `Init` montou o cartão,
`OpenNew` criou `t6b6a9f.bin` e a gravação de `ABC` retornou `OK`.
A primeira gravação de 4.096 bytes falhou com `WRITE_FAILED` (`errno=5`),
4.093 bytes transferidos na última tentativa e três tentativas totais.
O escritor retornou `WRITER_FAILED` nas novas escritas, inclusive após
repetir `Init`. `End` retornou `WRITER_FAILED`, o `Init` seguinte remontou
no mesmo boot e o perfil criou e fechou `t6b6a9r.bin` com `OK`.
[Preparo do reteste](../../../examples/tes006-2026-11-19/22-full-retest-prepared.json),
[resultado em hardware](../../../examples/tes006-2026-11-19/23-full-retest.json).

O leitor confirmou `ABC` em `t6b6a9f.bin`, seguido de 4.093 bytes `0x30`
da cauda não confirmada. O nome novo estava vazio. Os cinco arquivos
anteriores conservaram seus tamanhos e hashes. A limpeza removeu somente
o preenchimento `T6B6A9/` e devolveu o espaço esperado, com uma unidade
ocupada pelo arquivo de ensaio preservado.
[Conferência e limpeza do reteste](../../../examples/tes006-2026-11-19/24-full-retest-reader-cleanup.json).

Depois de liberar espaço, `recovered` (`b6aa`) montou o volume, criou
um nome novo, gravou e releu `ABCDEF` por USB e encerrou com `OK`.
O firmware `obc` foi restaurado e o esptool verificou a gravação.
O microSD ficou no módulo.
[Retorno após liberar espaço](../../../examples/tes006-2026-11-19/25-full-retest-recovered.json).

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../../.agents/REQUISITOS.md)
- [obc-dh/.agents/PINAGEM.md](../../../.agents/PINAGEM.md)
- [Painel de tarefas](../../../.agents/TODO.md); [organização e rastreabilidade](../../README.md).
