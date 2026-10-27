# INT-003 — Inicialização central e organização da execução

**Estado:** Pendente
**Responsáveis:** OBC-DH e setores responsáveis pelas saídas

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

## Critérios de conclusão

Boot central implementado, falhas de inicialização tratadas e estado inicial documentado. Compilar o firmware; comprovar boot/reset físico em [TES-008](../04-testes/TES-008-integracao.md). Não considerar a existência de setup() suficiente para REQ-OBC-008.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-008, preparação de REQ-OBC-007 e REQ-OBC-011. Implementação não equivale a requisito validado.
- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- [obc-dh/.agents/ARCHITECTURE.md](../../.agents/ARCHITECTURE.md)
- [README.md](../../../README.md)
- Origem no planejamento anterior: P-EPS-ELETRICA.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
