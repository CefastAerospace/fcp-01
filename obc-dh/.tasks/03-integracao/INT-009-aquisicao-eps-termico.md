# INT-009 — Aquisição e integração EPS/Térmico

**Estado:** Em andamento — recortes INA219 e LM75A de bancada concluídos e verificados no ESP32/USB; contratos operacionais, publicação TM e persistência completa pendentes
**Responsável pela implementação:** OBC-DH
**Responsáveis pelos contratos:** EPS e Térmico

## Recorte LM75A de bancada — 17/11/2026

O responsável aprovou bancada isolada e a referência **NXP LM75A provisória**,
sem sensores reais, publicação operacional, persistência ou proteção térmica.
O [datasheet LM75A_4, revisão 04 de 10/07/2007](https://www.nxp.com/docs/en/data-sheet/LM75A.pdf)
foi conferido; SHA-256
`fc76b747cd9e78c40d60dfe086aee4aafa8e92b2cf185789714eebae50c68de4`.
Fabricante dos módulos reais, montagem e mapa térmico continuam pendentes.

### Implementação e limites

- Novo driver em [LM75A_Standard](../../../eps-tc/LM75A_Standard/README.md),
  namespace `CEFAST::thermal`: Conf de um byte, Temp de dois bytes,
  diagnóstico Wire e conversor separado do I/O. O driver não inicializa o
  barramento, não espera conversão e não escreve Tos/Thyst.
- Conf preserva bits OS 4–1, zera reservados 7–5 e limpa SHUTDOWN. O
  conversor ignora cinco LSB, estende explicitamente o sinal dos onze bits e
  aceita −55 a +125 °C, com resolução digital de 0,125 °C. Falha preserva
  saída anterior; valores fora da faixa não são limitados artificialmente.
- `ObcThermal` mantém cadência/validade por canal.
  `ObcThermalEsp32` usa exclusivamente `ExecuteChecked`, reutiliza o
  classificador Wire portátil existente e converte fora do callback.
  Configuração de outra geração é recusada dentro do executor; o getter
  também revalida recuperação posterior à publicação.
- O histórico fica em RAM, separado de disponibilidade. O diagnóstico
  `temperature_C` só expõe leitura disponível e `last_valid_C` identifica
  histórico; ambos ficam `null` antes da primeira leitura válida.
- Metadados precedem a criação da tarefa; `starting` não simula operação.
  Cópias pequenas usam lock curto, sem I/O ou logs dentro dele. Progresso
  independe da presença dos dispositivos e não alimenta watchdog próprio.
- `main.cpp` recebeu chamadas somente sob `OBC_THERMAL_BENCH`; o novo
  ambiente `int009-lm75a-check` herda `obc` e acrescenta somente a biblioteca
  local térmica. Firmware normal e bancada INA219 não iniciam LM75A.
  Não houve remoção, novo TC, mudança em TM/codec ou alteração dos testes
  anteriores, calibração EPS ou contratos compartilhados.

### Parâmetros provisórios

| Item | Valor e finalidade |
|---|---|
| Canais / endereços | thermal_1 a thermal_5 / 0x48, 0x49, 0x4A, 0x4B, 0x4C; sem posição física atribuída |
| Leitura / espera inicial | 1.000 ms / 1.000 ms após configurar |
| Retentativa offline / validade | 5.000 ms / amostra antiga após idade superior a 3.000 ms |
| Pedido I2C | Prioridade 0, espera na fila de 100 ms |
| Execução / diagnóstico | Core 1, prioridade 1, pilha inicial 4.096 bytes, rodada de 10 ms / USB a cada 1.000 ms |

O datasheet informa conversão típica de 100 ms, sem máximo nessa
especificação. A espera inicial é provisória, não garantia física.
EPS/Térmico e OBC definirão fabricante, mapa final, periodicidade e validade
operacionais. TES-003/008/009 avaliarão os respectivos cenários físicos.
Leituras podem reconhecer OS em modo interrupt; OS não é usado pela bancada.
Esta entrega não estabelece limiares de proteção nem condições de atuação.

### Verificação, reprovação e ajuste autorizado

`ObcThermalCheck.cpp` executou o driver real com registradores Wire
exclusivos em `tests/thermal_native/`. Cinco grupos cobrem conversão
positiva/negativa/zero/fronteiras, protocolo e preservação dos limites,
erros/saídas preservadas, cadência/validade/geração e cinco canais
independentes. O teste não simula tempo físico, sensores ou executor
FreeRTOS. A guarda portátil testada é a mesma usada pelo callback real.

O novo teste e `ObcEpsCheck.cpp` original passaram com C++11,
`-Wall -Wextra -Werror`, ASan/UBSan e LeakSanitizer ativo fora de ptrace.
Comandos e saídas: [nativo LM75A](../../examples/int009-lm75a-2026-11-17-152917/02-native-check.txt)
e [regressão EPS](../../examples/int009-lm75a-2026-11-17-152917/04-eps-regression-check.txt).

O primeiro build conjunto aprovou `obc` e `int009-ina219-check`, mas
reprovou LM75A: `'Serial' was not declared in this scope`.
`Wire.h` inclui Stream, sem declarar Serial; o adaptador novo não incluía
Arduino diretamente. A implementação foi interrompida conforme AGENTS.md.
O responsável autorizou adicionar somente `Arduino.h` no header
`ObcThermalEsp32.h` e retomar o plano. Nenhum teste ou critério foi alterado.
O mesmo ambiente compilou após esse include. [Reprovação preservada](../../examples/int009-lm75a-2026-11-17-152917/05-firmware-builds.txt)
e [recompilação](../../examples/int009-lm75a-2026-11-17-152917/06-lm75a-build-after-include.txt).
Permanece apenas o aviso externo conhecido de `uartSetPins` no framework.

### Bancadas e estado final

ESP32-D0WD-V3 rev. 3.1/CH340 em `/dev/ttyUSB0`, somente USB.
Uploads tiveram hash verificado pelo esptool; nenhum apagamento integral,
reparo de core dump ou limpeza de NVS foi solicitado. A reserva normal de
boot continua usando a NVS existente.

| Recorte | Resultado observado |
|---|---|
| LM75A, 30,013 s | 54 ACKs, 27 TMs; cinco canais indisponíveis e sem medições fictícias. Sete tentativas por canal, intervalos de 5.002–5.012 ms; 31 amostras de saúde READY com progresso; menor margem de pilha térmica 2.448 bytes |
| Tempos LM75A | Maior `q` 180 ms; resposta máxima 224,653 ms; limites originais de 200 ms/2 s/255 bytes preservados; `tp` permanece `null` |
| INA219 original, 30 s | Roteiro inalterado aprovado: 54 ACKs, 27 TMs, maior `q` 146 ms, resposta máxima 184,645 ms e margem mínima EPS 2.656 bytes |
| Firmware normal restaurado | `ObcStationCheck.py` original aprovado: 64 TCs diretos, 37 TMs, maior `q` 116 ms, RTT máximo 156,5 ms e `no_sd` esperado sem cartão |
| Builds | `obc`: RAM estática 32.704 bytes/flash 432.925; INA219: 33.072/444.661; LM75A: 33.200/442.881. RAM estática não mede pico de execução |

Evidências novas, sem sobrescrever históricos:
[LM75A USB](../../examples/int009-lm75a-2026-11-17-152917/08-lm75a-usb.json),
[INA219 original](../../examples/int009-lm75a-2026-11-17-152917/10-ina219-regression.json),
[obc restaurado](../../examples/int009-lm75a-2026-11-17-152917/12-obc-restored.json)
e [manifesto](../../examples/int009-lm75a-2026-11-17-152917/manifest.json).
O manifesto conserva revisão, alterações anteriores desta sessão,
comandos, versões e hashes, inclusive a comparação dos arquivos que deveriam
permanecer intactos. Procedimento no [guia](../../tools/README.md#lm75a-de-bancada--17112026).

O aviso de core dump inválido diagnosticado na INT-004 permanece nos logs,
sem nova intervenção; a imagem não foi apagada ou reparada. Essa ressalva,
reprovações anteriores e limites de evidência são preservados.

**Recorte LM75A de desenvolvimento concluído.** A placa terminou em `obc`,
sem aquisição térmica operacional. A INT-009 permanece **Em andamento**:
contratos finais, integração/publicação operacional, persistência completa
do canal Geral e comprovação com sensores reais continuam nos seus escopos.
Nenhuma TES ou requisito foi aprovado; não mover esta task para `done/`.

## Objetivo

Integrar os dados de energia e temperatura ao OBC, com disponibilidade e erros explícitos.

## Origem e limites

Escopo extraído da [INT-005](INT-005-sensores-e-adcs.md) na revisão documental de 14/11/2026. Aquisição não implementa proteção térmica ou energética; as ações pertencem à [INT-010](INT-010-protecoes-energia-termico.md). A revisão não implementou drivers nem executou ensaios.

## Entrega

- Reutilizar EPS_INA219_Standard e integrar os 3x INA219 pelo executor I2C. A biblioteca INA219 não implementa aquisição/proteção dos LM75A.
- Integrar aquisição dos 5x LM75A conforme o contrato do setor, sem criar valores fictícios quando um dispositivo estiver indisponível.
- Definir com os produtores unidades, validade, localização das temperaturas, origens/códigos e publicação em registros/TM, reutilizando modelos existentes.
- Mapear erros dos drivers em `ExecuteChecked`, concluir transações antes do retorno do callback e integrar reconfiguração após recuperação do barramento.
- Conciliar medição solar por ADC nos PDFs versus sensores digitais propostos com EPS/ADCS; não reservar ADC silenciosamente. Confirmar se o BMS oferece interface de aquisição, sem presumir endereço.

## Pré-requisitos

- **Tarefas/capacidades:** COM-001/002/003, modelos DAD-001 e coordenação de boot existente da INT-003. O encerramento do boot de voo inteiro não impede preparar a aquisição contra contratos disponíveis.
- **Condições externas:** inventário, montagem, endereços finais de sete bits, grandezas/unidades, posições térmicas, periodicidade e limites dos callbacks fornecidos por EPS/Térmico. `0x40`/`0x41` e `0x48` são propostas anteriores, não mapa completo aprovado.
- Parâmetros de desenvolvimento provisórios seguem o [registro comum](../README.md#parâmetros-provisórios); não estabelecem limites de proteção.

## Verificação de desenvolvimento

OBC mantém regressões para sucesso, dispositivo indisponível, leitura incompleta, erro/timeout, validade dos campos e reconfiguração após recuperação. Reutilizar o serviço e os testes existentes, sem duplicar drivers ou algoritmos dos setores.

## Critérios de conclusão

Aquisição integrada, acesso I2C coordenado, mapeamentos documentados, build e regressões pertinentes aprovados. Contrato necessário ainda indefinido mantém a entrega correspondente aberta. Medições por dispositivo e convivência física pertencem à TES-003/TES-008; sua aprovação não é condição de encerramento desta implementação.

## Encaminhamento para comprovação

- [TES-003](../04-testes/TES-003-i2c-hardware.md): operações reais, frequência, duração e falhas por dispositivo.
- [TES-008](../04-testes/TES-008-integracao.md): dados de energia/temperatura na TM e convivência integrada.
- [TES-009](../04-testes/TES-009-falhas-e-recuperacao.md): recuperação física e efeito sobre os consumidores.

## Evidências

Nenhuma implementação ou aprovação registrada por esta revisão. Registrar versão, contratos, parâmetros, regressões e limitações quando executar.

### Recorte INA219 — 16/11/2026, implementação parcial

Plano autorizado: bancada isolada, três canais em RAM/USB, extensão compatível
do diagnóstico I2C e nenhuma publicação EPS na TM operacional. Canal Geral
permanece sem novo registro persistente. LM75A, proteções e contrato final
continuam fora deste recorte.

Implementação presente, ainda sem aceite de conclusão:

- `ObcEps`: cadência/validade por canal e classificação da informação exposta
  por Wire. `ObcEpsEsp32`: tarefa proprietária do driver, operações no executor
  existente, cópias protegidas e diagnóstico USB.
- Driver EPS: sobrecarga `begin(wire, channel)` e diagnóstico adicional da
  operação. APIs anteriores, calibração e cálculos preservados.
- Ambiente `int009-ina219-check`: biblioteca EPS local e aquisição habilitada
  somente por `OBC_EPS_BENCH`. Boot existente reutilizado; firmware normal
  não inicia aquisição EPS.
- Verificação nova `tests/ObcEpsCheck.cpp`, com registradores Wire de ensaio
  exclusivos em `tests/eps_native/`. Não modifica testes anteriores nem
  comprova executor FreeRTOS, sensores físicos ou concorrência na placa.

Parâmetros provisórios: Solar/Bateria/Geral `0x40`/`0x41`/`0x44`; período
1.000 ms por canal; prioridade de pedido 0; espera na fila 100 ms; tentativa
offline a cada 5.000 ms; amostra antiga após 3.000 ms; espera de conversão
arredondada para 2 ms; tarefa Core 1/prioridade 1/pilha inicial 4.096 bytes.
Finalidade: desenvolvimento de aquisição, não limiares de proteção ou aceite
da montagem. EPS/OBC definirão valores operacionais; comprovação física
permanece TES-003/008/009. Corrente mantém VIN+ para VIN− positiva; as duas
tensões são identificadas separadamente.

Verificação nativa executada:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=undefined -Iobc-dh/tests/eps_native -Iobc-dh/headers -Ieps-tc/EPS_INA219_Standard/src obc-dh/src/ObcEps.cpp eps-tc/EPS_INA219_Standard/src/EPS_INA219.cpp obc-dh/tests/ObcEpsCheck.cpp -o /tmp/fcp01-eps-check
/tmp/fcp01-eps-check
```

Compilação aprovada. Execução fora do sandbox aprovada com ASan/UBSan:
`INA219: driver real, disponibilidade parcial, erros, cadencia e retomada aprovados`.
Tentativa dentro do sandbox encontrou a restrição conhecida de LeakSanitizer
sob ptrace; não houve desativação de sanitizers nem mudança de expectativas.

Build em `obc-dh/`: `pio run -e obc -e int009-ina219-check`.

| Ambiente | Resultado |
|---|---|
| `obc` | Aprovado; RAM estática 32.704 bytes, flash 432.925 bytes. Aviso externo já conhecido em `esp32-hal-uart.c:153`. |
| `int009-ina219-check` | Reprovado: macro Xtensa `EPS`, definida como `192` em `xtensa/config/specreg.h:109`, colide com o namespace existente `CEFAST::EPS`. |

Erro decisivo: `error: expected identifier before numeric constant`, na
declaração `namespace EPS`. Os demais erros sobre tipos ausentes decorrem
dessa colisão. O teste nativo não inclui os headers Xtensa e não revela o
conflito. Implementação interrompida conforme AGENTS.md, sem corrigir o
namespace ou modificar o framework por iniciativa própria.

Próxima decisão proposta: usar namespace sem colisão, como `CEFAST::eps`,
atualizando consumidores/exemplo. Isso muda um nome público que o plano
previa preservar; depende de ajuste explícito com o responsável OBC/EPS.

Sem upload ou execução na placa nesta operação. Regressões anteriores ainda
não foram reexecutadas; entradas e expectativas permanecem intactas.
Nenhuma aprovação física ou encerramento da INT-009 foi registrado.

### Continuação autorizada — 16/11/2026

O responsável aprovou o namespace `CEFAST::eps` e o plano de conclusão do
recorte. Driver, consumidor, exemplo, README e referências de tipos do teste
INA219 foram atualizados. Não houve alteração de entradas, asserts ou
resultados esperados; a macro Xtensa e o framework permanecem intactos.
O diagnóstico da reprovação anterior é conservado acima como histórico.

Novo roteiro `tests/ObcEpsBenchCheck.py`: observa 30 s com ESP32/USB sem
sensores, registra os três canais/valores indisponíveis, cadência offline,
progresso/margem de pilha e continuidade TC/TM. Mantém os limites existentes
de entrega ≤200 ms e resposta ≤2 s; usa relatório exclusivo e conserva
resultados parciais em falha. Sintaxe e `--help` aprovados com o Python do
PlatformIO e pyserial 3.5. Não houve execução do roteiro na placa.

| Verificação executada | Resultado |
|---|---|
| Driver/política INA219, ASan/UBSan | Aprovado; disponibilidade parcial, causas, cadência e retomada. |
| Mesmo teste com `-DEPS=192`, ASan/UBSan | Aprovado; sem colisão com a macro. |
| Boot, UBSan | Aprovado; ordem e oito combinações preservadas. |
| Escalonamento I2C, UBSan | Aprovado. |
| Saúde I2C, ASan/UBSan | Aprovado; limites, recuperação, wrap e eventos. |
| Watchdog, ASan/UBSan | Aprovado. |
| Build `obc` | Aprovado; RAM estática 32.704 bytes, flash 432.925 bytes. |
| Build `int009-ina219-check` | Aprovado; RAM estática 33.072 bytes, flash 444.405 bytes. |
| Inspeção dos símbolos ELF | Inicializador/worker EPS presentes somente na bancada. |

Comandos, saídas, revisão-base, hashes e indicação explícita de USB não
executado estão em [obc-eps-2026-11-16-native.json](../../examples/obc-eps-2026-11-16-native.json).
As verificações existentes de boot, escalonamento/saúde I2C, watchdog e
downlink foram comparadas com HEAD e permanecem byte a byte intactas.
ASan/LeakSanitizer foram executados fora do sandbox, sem desativar sanitizers.

Frames estáticos do adaptador: callback 208 bytes, worker 752 bytes e
diagnóstico no loop 1.200 bytes. Não são margem de pilha medida ou soma
completa das cadeias de chamada. O aviso externo de `esp32-hal-uart.c:153`
permanece, sem alteração do framework.

Pendência para concluir o recorte: conectar ESP32, executar o roteiro USB
novo e restaurar/verificar `obc`, conforme o [guia](../../tools/README.md#ina219-de-bancada--16112026).
Nenhuma porta ttyUSB/ttyACM estava disponível nesta operação; não houve
upload. Não declarar execução na placa ou mover esta INT para `done/`.
Contrato final EPS, TM operacional, persistência completa do Geral e LM75A
continuam pendentes nos seus escopos.

### Primeira execução USB — 16/11/2026, após conexão da placa

ESP32 identificado fora do sandbox em `/dev/ttyUSB0`, ponte USB `1A86:7523`,
ESP32-D0WD-V3 revisão v3.1. Upload de `int009-ina219-check` aprovado, com
hashes da flash conferidos pelo esptool. Nenhum código ou validação foi
alterado nesta execução.

O roteiro aprovado reprovou durante a coleta inicial, antes da janela de
observação de 30 s. Primeiro diagnóstico de canal: `channel=SOLAR
address=0x00 available=0 has_valid=0 attempted_ms=0 driver=OK result=0`.
O esperado permanece `address=0x40`, conforme a configuração aprovada.
Não modificar o teste para aceitar endereço zero.

Diagnóstico da causa: `published` inicia zerado; os metadados corretos são
preparados somente no snapshot local de `RunWorker`. A publicação compartilhada
acontece após as operações dos três canais. `ReportDiagnostics` usa como
referência inicial o instante zero do boot e pode executar antes dessa primeira
publicação. No log da falha, o boot já havia ultrapassado 1 s e o executor I2C
ainda estava em callback. Assim, o relatório expõe estado inicial zerado como
se fosse uma amostra publicada. Não é evidência de sensor no endereço `0x00`.

Proposta para discussão, ainda não implementada: inicializar os metadados
compartilhados antes de criar a tarefa, distinguir inicialização de primeira
rodada concluída e emitir diagnósticos de amostra somente após publicação
real. Não acrescentar atraso fixo nem alterar entrada, timeout ou resultado
esperado do roteiro. Interrupção conforme regra 7 de AGENTS.md.

Conforme o procedimento aprovado, `obc` foi restaurado e os hashes da flash
conferidos. `ObcDownlinkCheck.py` original passou: seis ACKs, `q` máximo
11 ms e maior resposta 67,133 ms. A placa ficou com o firmware normal.

Evidências preservadas:

- [Bancada reprovada](../../examples/obc-eps-2026-11-16-usb.json): linhas originais e erro, sem sobrescrever o resultado.
- [Firmware normal restaurado](../../examples/obc-eps-2026-11-16-restored.json): roteiro original aprovado.
- [Manifesto USB](../../examples/obc-eps-2026-11-16-usb-manifest.json): versão da placa, hashes, resultados e limites desta execução.

O recorte e a INT-009 continuam abertos. Nenhuma TES foi aprovada ou mudou
de estado; não houve conclusão por upload, compilação ou diagnóstico.

### Ajuste autorizado e segunda execução USB — 16/11/2026

O responsável autorizou corrigir a inicialização/publicação. Alterações
restritas ao adaptador `ObcEpsEsp32` e à sua documentação de API:

- `MakeInitialSnapshot` concentra canal/endereço e estados `NotInitialized`,
  sem duplicar a preparação entre Init e worker.
- Init publica esses metadados sob proteção antes de criar a tarefa.
- `active` confirma a primeira rodada realmente publicada. Até ela,
  `ReportDiagnostics` informa `starting`, sem emitir amostras ou progresso
  fictícios. Nenhum atraso fixo foi acrescentado.

Os dois builds passaram. `obc` conserva RAM estática 32.704 bytes e flash
432.925 bytes; bancada conserva RAM estática 33.072 bytes e usa flash
444.661 bytes. Upload da bancada e hashes da flash aprovados.

O mesmo `ObcEpsBenchCheck.py`, byte a byte intacto, avançou além da falha
anterior: `starting` foi observado e os canais apareceram em `0x40`, `0x41`
e `0x44`, indisponíveis com medições `null`. Isso verifica o ajuste de
publicação no caso antes reprovado, mas não conclui o roteiro.

Nova reprovação, ainda na coleta inicial: `[EPS-HEALTH] active=1 service=3
progress_ms=2350 age_ms=7 operations=1 failures=1 stack_free_bytes=2736`.
O esperado do roteiro permanece serviço READY (`1`), não UNAVAILABLE (`3`).

Evidência de diagnóstico:

- Primeira configuração Solar: Wire `status=5`, resultado
  `TRANSFER_TIMEOUT`; código do driver `I2C_ERROR`.
- Executor: `cause=5 failure=5 cycles=1 recovered=0 exhausted=1`.
  Os códigos correspondem a `SCL_LOW`, conforme `I2cRecovery.h`.
- Recuperação esgotou com SCL lido baixo. O callback iniciou, ultrapassou o
  limite diagnóstico existente e retornou; a tarefa EPS continuou progredindo.
- Bateria e Geral não foram configurados porque o serviço ficou indisponível.

Ainda não há confirmação da causa elétrica: solicitar informação sobre
jumpers, sensores e pull-ups nos GPIOs 21/22. O framework habilita pull-ups
internos ao inicializar o controlador; a recuperação existente libera as
linhas em dreno aberto e depende dos pull-ups da montagem. Esses fatos não
comprovam, sozinhos, qual ligação ou configuração causou a falha inicial.
Não mudar níveis, recuperação, timeout ou expectativa do roteiro por inferência.

Execução interrompida para diagnóstico conforme AGENTS.md. Não foi aplicada
segunda correção. `obc` foi restaurado com hashes conferidos e o roteiro
original passou: seis ACKs, `q` máximo 31 ms e resposta máxima 72,826 ms.
Não houve mudança dos dois roteiros de validação.

Novas evidências, preservando a reprovação anterior:

- [Bancada USB 02](../../examples/obc-eps-2026-11-16-usb-02.json).
- [obc restaurado 02](../../examples/obc-eps-2026-11-16-restored-02.json).
- [Manifesto USB 02](../../examples/obc-eps-2026-11-16-usb-02-manifest.json).

O recorte continua aberto pelo barramento indisponível; a janela de 30 s não
foi concluída. Nenhuma comprovação física ou conclusão de TES foi inferida.

### Recorte de bancada concluído — execução USB 03, 16/11/2026

O responsável informou que removeu todas as ligações externas e deixou o
ESP32 livre, somente com USB. A mesma bancada e o mesmo roteiro foram
repetidos, sem qualquer alteração de código ou validação desde a execução
02. Hashes de fontes e dos dois firmwares foram comparados e são idênticos.

Upload da bancada aprovado e hashes da flash conferidos. O roteiro
`ObcEpsBenchCheck.py` passou integralmente. Resultados observados:

| Medida | Resultado |
|---|---|
| Janela de observação | 30,038820127 s |
| TC/ACK e TM | 54 ACKs e 27 TMs; campos EPS operacionais continuam `null` |
| Maior `q` | 94 ms, dentro do limite existente de 200 ms |
| Maior resposta | 171,717540001 ms, dentro do limite existente de 2 s |
| Canais | Solar `0x40`, Bateria `0x41`, Geral `0x44`; indisponíveis sem sensores, sem medições fictícias |
| Novas tentativas | Sete instantes por canal; intervalos de 5.002 a 5.011 ms |
| Saúde EPS | 32 amostras, serviço sempre READY (`1`) e progresso contínuo |
| Menor margem de pilha EPS observada | 2.656 bytes; não representa pior caso com sensores reais |

A falha `SCL_LOW` anterior não se repetiu com o ESP32 livre. A comparação
mostra que a condição externa mudou e o software permaneceu igual; não
identifica qual ligação externa produzia o comportamento anterior nem
comprova a montagem integrada. As duas reprovações anteriores são preservadas.

Ao terminar, `obc` foi restaurado com hashes da flash conferidos. O roteiro
original `ObcDownlinkCheck.py` passou: seis ACKs, `q` máximo 10 ms e maior
resposta 97,340 ms. A placa ficou com o firmware normal, sem aquisição EPS
operacional habilitada.

Evidências finais:

- [Bancada USB 03 aprovada](../../examples/obc-eps-2026-11-16-usb-03.json).
- [obc restaurado 03](../../examples/obc-eps-2026-11-16-restored-03.json).
- [Manifesto USB 03](../../examples/obc-eps-2026-11-16-usb-03-manifest.json): métricas, hashes, condições e comparação com a execução 02.

O recorte autorizado de implementação/bancada INA219 está concluído. A
INT-009 inteira permanece Em andamento e não é movida para `done/`: ainda
inclui os contratos finais, publicação operacional, persistência completa
do canal Geral e aquisição LM75A. Ensaios independentes com sensores reais
continuam nas TES-003/008/009, sem alteração de critérios ou estados.
