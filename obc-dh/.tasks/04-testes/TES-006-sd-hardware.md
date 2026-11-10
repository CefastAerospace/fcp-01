# TES-006 — SD: operações e falhas em hardware

**Estado:** Pendente
**Responsáveis:** OBC-DH e EPS

A execução dos novos ensaios será encaminhada ao setor de testes; os responsáveis técnicos acima apoiam interfaces, condições e diagnóstico.

## Objetivo

Comprovar leitura, gravação e extração de arquivos na montagem real.

## Entrega

Exercitar escrita/leitura, sincronização/fechamento, extração, cartão cheio/indisponível e limites de buffer com conteúdo conhecido. Registrar perdas e efeitos sobre arquivos/FAT32. Interrupções durante escrita serão exercitadas em [TES-009](TES-009-falhas-e-recuperacao.md); a interpretação dos registros de missão está em [TES-007](done/TES-007-dados-e-tempo.md)/[TES-008](TES-008-integracao.md).

## Pré-requisitos

- **Tarefas:** [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md)
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
| [COM-005](../01-comunicacao/done/COM-005-armazenamento-sd.md) | Conferir módulo, montagem e alimentação com EPS, frequência/latência e operações do SD na montagem real, incluindo cheio/indisponível e extração. | Pendente |

## Prontidão e preparação para o setor de testes

COM-005 está concluída. Encaminhar preparação; execução depende de cartão/módulo, montagem, alimentação e critérios com EPS. A primeira etapa INT-006 já grava registros, mas a bancada com backend simulado não comprova cartão físico. Para esta tarefa, extração pode usar `Read` de arquivo fechado ou cópia pelo leitor físico; `request_log` ainda não implementado não deve ser presumido disponível.

Usar cartão de ensaio e conteúdo conhecido, preservando dados anteriores. Registrar capacidade, formatação FAT32, módulo, alimentação, pinagem HSPI e frequência. A frequência inicial de 4 MHz é configurada, não uma garantia de funcionamento da montagem. Interrupção durante escrita permanece na TES-009.

## Checklist de execução

- [ ] **Montagem nominal e ausência:** energizar a montagem aprovada com cartão FAT32 e repetir sem cartão. Esperar disponibilidade ou erro explícito conforme a condição, sem formatação, reparo ou exclusão automática.
- [ ] **Criação exclusiva:** gravar conteúdo conhecido em nome novo e tentar reutilizá-lo. Esperar recusa do nome existente e preservação byte a byte do arquivo anterior.
- [ ] **Escrita, sincronização e fechamento:** gravar blocos conhecidos, conferir cada `RESULT` e fechar. Esperar sucesso apenas após escrita completa/sincronização no runtime; não tratar `transferred == tamanho` como confirmação em status de erro.
- [ ] **Leitura e limites:** ler arquivo fechado em blocos, incluindo offsets internos, EOF e offset após EOF. Esperar bytes corretos, EOF com zero bytes e recusa de offset inválido; leitura do arquivo ainda aberto para escrita deve retornar `BUSY`.
- [ ] **Extração reproduzível:** obter cópia estável e comparar tamanho e todos os bytes com o conteúdo conhecido. Para registros de missão, usar o leitor atualizado e registrar os resultados sem sobrescrever a origem; interpretação de missão completa continua em TES-008.
- [ ] **Cartão cheio/indisponível:** aplicar procedimento aprovado e registrar erros/tentativas. Esperar indisponibilidade observável e preservação dos arquivos anteriores, sem apagar dados para liberar espaço automaticamente.
- [ ] **Falhas de escrita/sincronização:** quando houver método físico aprovado, observar tentativas e cauda do arquivo. O serviço admite três tentativas totais no mesmo offset confirmado; falha final deve impedir novas escritas. Uma falha real não injetável permanece sem execução física, mesmo que o teste POSIX tenha passado.
- [ ] **Retomada explícita do serviço:** após falha terminal, verificar que repetir `Init` não a oculta. No procedimento de bancada, usar `End` + `Init` e nome novo, preservando o arquivo anterior. Recuperação automática de missão não está implementada e pertence à INT-008/TES-009.
- [ ] **Acesso concorrente e duração:** exercitar a disputa permitida na bancada, esperando `BUSY` sem acesso intercalado ao driver. Medir latências com a montagem real; limites de aplicação dependem dos acordos de integração.

Registrar falhas, perdas e efeitos observados nos arquivos/FAT32 no formato de [evidências](../README.md#registro-por-cenário). Fechamento normal e `fsync` não aprovam integridade em corte de energia.

## Evidências

Nenhuma evidência de conclusão registrada. Ao executar, registrar versão, procedimento, entradas, resultados observados e limitações; não marcar esta tarefa por decisão documental.

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/REQUISITOS.md](../../.agents/REQUISITOS.md)
- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
