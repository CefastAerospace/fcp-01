# INT-003 — Inicialização central e organização da execução

**Estado:** Em andamento
**Responsáveis:** OBC-DH e setores responsáveis pelas saídas

## Estado vigente — 10/11/2026

O coordenador de bancada continua responsável apenas por tempo/NVS, I2C e montagem SD. Depois dele, `main.cpp` já inicia TT&C e o fluxo INT-006; o diário pode criar/gravar arquivos. `loop()` atende comandos/diagnósticos, sem o `delay(100)` original. O escopo aprovado em 05/11 e os resultados abaixo descrevem a entrega inicial, não limitam as integrações posteriores.

As bancadas posteriores identificaram WROOM-32/ESP32-D0WD-V3 e exercitaram firmware por USB, mas não confirmam o circuito completo ou estados seguros de EN/PWM/Burn Wire. A tarefa permanece aberta pelo próprio boot de voo. Com ESP32/USB, preparar o levantamento de esquema e condições com EPS/ADCS/Estruturas; não declarar segurança de reset apenas por observar logs. Ver [INT-006](INT-006-fluxo-de-dados.md) para o fluxo atual e TES-008 para o aceite físico.

## Objetivo

Inicializar automaticamente os recursos disponíveis, mantendo estados seguros e falhas observáveis.

## Entrega

Construir a coordenação de setup()/loop(), ordem de inicialização e organização de tarefas. Manter processamento nos módulos; não criar implementações vazias para recursos futuros. Cada integração posterior acrescenta seus recursos e informa suas falhas. A observação por UART0 usa o contrato umbilical em [INT-004](INT-004-ttc-e-umbilical.md).

## Pré-requisitos

- **Tarefas:** [INT-001](done/INT-001-ambiente-obc.md)
- **Condições externas:** Confirmar placa, circuito, estado inicial do ConOps e condições de boot com EPS, ADCS e Estruturas.

## Definições e condições

- Confirmar variante DevKit/módulo, conectores, USB/UART, disponibilidade dos GPIOs e flash/PSRAM. GPIOs 16/17 estão alocados à UART do Payload; conferir disponibilidade física antes da ligação.
- Conferir alimentação, GND, níveis, resistores e estados em reset de CS, RST, EN, PWM e Burn Wire. GPIOs 5, 12 e 15 são strapping pins; GPIO 35 não possui pulls internos.

### Escopo de bancada aprovado em 05/11/2026

- O responsável escolheu boot de bancada e continuidade do diagnóstico mesmo quando algum serviço falha. A entrega parcial inicializa somente USB/serial, tempo/boots, I2C e tenta montar SD, sem integrar rádio, Payload, sensores, atuadores ou missão.
- Ordem: `Serial.begin(115200)`, reserva de boot/NVS, inicialização I2C, montagem SD e relatório. Não aguardar conexão do computador. Não criar arquivos ou gravar registros. O I2C inicializa seu controlador/executor existente sem varredura de sensores.
- Falhas não provocam retorno antecipado entre os três serviços, repetição automática ou reset. Boot temporal indisponível mantém ID zero no relatório e texto `indisponivel` no log; não apagar NVS ou fabricar UTC. A indisponibilidade de SD é compatível com terminar o diagnóstico sem cartão.
- Os logs são linhas com prefixo `[OBC]`, sem envelope JSON, comandos ou TM. O protocolo umbilical continua em INT-004. O `loop()` cede execução com `delay(100)`; nenhuma nova tarefa é criada pelo coordenador.
- Não programar EN, PWM ou Burn Wire, nem afirmar segurança elétrica por manter essas saídas fora do escopo. Boot de voo, placa e níveis de saída continuam dependentes de EPS/ADCS/Estruturas e do ConOps.

## Critérios de conclusão

Boot central implementado, falhas de inicialização tratadas e estado inicial documentado. Compilar o firmware; comprovar boot/reset físico em [TES-008](../04-testes/TES-008-integracao.md). Não considerar a existência de setup() suficiente para REQ-OBC-008.

## Encaminhamentos recebidos — 09/11/2026

As origens abaixo encerraram seu escopo próprio. Estes itens continuam
abertos nesta tarefa, com os setores responsáveis, antes do aceite da
integração ou do ensaio correspondente. Não substituem os critérios e
cenários originais e não mudam seu estado por conclusão da origem.

| Origem concluída | Obrigação recebida | Situação |
|---|---|---|
| [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Confirmar módulo/cartão, alimentação e condições de boot do SD com EPS antes do uso integrado; preservar indisponibilidade sem formatação/reparo automático. | Pendente |

A auditoria mantém INT-003 em andamento: o coordenador de bancada está
implementado, mas o estado inicial de voo e os níveis seguros ainda
precisam de definição e implementação no próprio boot integrado. A
identificação WROOM-32 e o loopback não comprovam esse estado.

## Evidências

Entrega parcial de bancada implementada e verificada em 05/11/2026, sobre a base Git `28504b1` e os trabalhos locais anteriores de DAD-001/COM-005. Sem commit, upload ou execução física. A tarefa permanece **Em andamento** até confirmar placa, circuito, estado inicial do ConOps e níveis elétricos do boot integrado. Não foi movida para `done/`.

### Implementação e preservação

- [ObcStartup.h](../../headers/ObcStartup.h) e [ObcStartup.cpp](../../src/ObcStartup.cpp): coordenador portátil com `INITIALIZERS`, `STARTUP_REPORT` e `Initialize`. Reutiliza `obc_time::STATUS` e `obc_storage::RESULT`, sem novo catálogo de resultados ou modo de voo. Três ponteiros para as funções existentes permitem verificar a mesma coordenação no computador sem simular drivers no código de produção.
- O relatório conserva identificação do boot, estado temporal, disponibilidade do controlador/executor I2C e resultado completo do SD. Se a reserva temporal falha, limpa o ID mesmo quando o inicializador forneceu valor não zero em falha. Ponteiro nulo invalida a composição: nenhuma função é chamada, estados temporal/SD são `INVALID_ARGUMENT`, ID zero e I2C falso.
- [main.cpp](../../src/main.cpp): os corpos vazios de `setup()`/`loop()` foram substituídos pela sequência aprovada e por espera de 100 ms. Chamadas reais são `obc_time::esp32::StartBoot`, `i2c::Init` e `obc_storage::esp32::Init`. Relatório emitido uma vez por USB/UART0 com status/errno do SD, ID válido ou indisponível e tempo monotônico separado de `UTC=desconhecido`.
- `Pinout.h` deixou de ser incluído diretamente pelo `main`, pois os drivers continuam responsáveis pela pinagem. Não houve remoção de funções ou alteração nos drivers, políticas, frequências, filas, prioridades, codec, modelos, testes anteriores ou contratos. Não há tarefa adicional de inicialização, fila de logs, gerenciador de modos ou supervisão de recuperação.
- `i2c::Init` cria a tarefa de executor já existente; o coordenador não introduz outra tarefa. O relatório é local, sem novo estado global ou API de saúde contínua. A coordenação pressupõe que os inicializadores retornam; timeout de transferência não é garantia de duração de todo o boot.

### Procedimento e resultados

Executado dentro de `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcStartup.cpp tests/ObcStartupCheck.cpp -o /tmp/fcp01-startup-check
/tmp/fcp01-startup-check
pio run -e obc
```

Todos os comandos terminaram com código zero. Não houve diagnóstico do UBSan. Resultado do [teste nativo](../../tests/ObcStartupCheck.cpp): `Boot de bancada: ordem, oito combinacoes e falhas independentes aprovadas.`

- As oito combinações de sucesso/falha dos três inicializadores preservaram a ordem tempo/NVS, I2C e SD, com uma chamada por serviço. Falhas anteriores não impediram as chamadas seguintes.
- `UINT64_MAX` foi conservado como ID em sucesso; falha temporal retornou ID zero mesmo quando o inicializador de teste forneceu esse valor não zero. Estado temporal e disponibilidade I2C coincidiram com cada cenário.
- Estado, quantidade, errno e tentativas do resultado de SD foram conservados integralmente usando valores sentinela no teste; isso verifica encaminhamento dos campos, não afirma que uma falha real de montagem transfere bytes.
- Ausência de cada um dos três ponteiros foi rejeitada sem chamar qualquer serviço e sem produzir ID ou indicação de disponibilidade fictícios.
- O teste exercita o coordenador real com funções finitas de teste. Não executa Serial, FreeRTOS, montagem SD ou reserva NVS física. Não houve mudança nos testes existentes, entradas ou resultados esperados.

Build ESP32 aprovado com PlatformIO 6.2.0, Espressif32 6.6.0, Arduino-ESP32 2.0.14 e Xtensa 8.4.0+2021r2-patch5. Novo coordenador e `main` compilaram sem avisos. Permanece o aviso externo conhecido em `esp32-hal-uart.c:153`, retorno sem valor em `uartSetPins`; framework não alterado. RAM estática 23776/327680 bytes; flash 347333/1310720 bytes. A RAM estática não inclui todas as alocações de execução de tarefas/framework, nem representa medição de consumo em bancada.

SHA-256 dos artefatos verificados:

```text
c7cf65313a24ebb55e0144b6f1efb3fdadbe202b7d2d858da58e9611a749a560  obc-dh/headers/ObcStartup.h
4a51f19b5cb236b2c8020c4ad12f1a42f41d40268f3f4316d44962fd4bc8fa16  obc-dh/src/ObcStartup.cpp
ee4f872b8a4c46208267fd320a47126cf3e3af1e631112e25bac092ab6456a37  obc-dh/src/main.cpp
cc8495804a5a1db7d0ea837bca041333198cdb58244d0dd3e8707dbe48f7d91d  obc-dh/tests/ObcStartupCheck.cpp
```

### Pendências para concluir

Confirmar variante da placa, circuito/alimentação, ConOps e estados de CS/RST/EN/PWM/Burn Wire com os responsáveis. O boot de bancada não define os níveis seguros de voo, não comprova comportamento durante reset e não integra sensores, rádio ou atuação. Conservar o cenário físico de TES-008 e seus critérios; build e teste portátil não o encerram. Sem upload nesta entrega. Integrações TT&C/umbilical, ADCS, Payload, gravação de registros e supervisão permanecem em suas tarefas.

## Referências

- Requisitos relacionados: REQ-OBC-008, preparação de REQ-OBC-007 e REQ-OBC-011. Implementação não equivale a requisito validado.
- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [README.md](../../../README.md)
- Origem no planejamento anterior: P-EPS-ELETRICA.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
