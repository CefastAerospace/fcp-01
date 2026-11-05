# COM-005 — SD: gravação, leitura, extração e recuperação

**Estado:** Em andamento
**Responsáveis:** OBC-DH e EPS

## Objetivo

Oferecer armazenamento de arquivos com acesso coordenado e falhas observáveis.

## Entrega

Reutilizar SD e uma instância HSPI própria. Implementar abertura, escrita, leitura, extração, sincronização/fechamento e tratamento de cartão cheio/indisponível. Definir recuperação de arquivos incompletos e do sistema de arquivos. O formato do conteúdo pertence a [DAD-001](../02-dados-e-tempo/DAD-001-modelos-e-formatos.md)/[DAD-003](../02-dados-e-tempo/DAD-003-registros-e-interpretacao.md); não duplicar esse contrato no driver. O rádio é integrado em [INT-004](../03-integracao/INT-004-ttc-e-umbilical.md).

## Pré-requisitos

- **Tarefas:** Nenhuma.
- **Condições externas:** Definir parâmetros do SD, alimentação, retenção e tratamento de indisponibilidade com EPS e coordenação.

## Definições e condições

- Decisão conciliada entre pinagem OBC e mapa compartilhado: LoRa/VSPI, SD/HSPI; inversão anterior é histórica. Conferir esquema, montagem e boot/reset; encerramento documental não conclui validação elétrica.
- Definir formato persistente, FAT32 citado nos PDFs, estatísticas, retenção, extração e escrita/sincronização/fechamento. Acordar SD cheio/indisponível, buffers limitados e como sinalizar perdas.
- Definir interrupções de alimentação e critérios de integridade; fechamento normal ou append não garante recuperação. Considerar registro incompleto e sistema de arquivos, não apenas conteúdo JSON.

### Políticas aprovadas em 05/11/2026

- O SD é armazenamento persistente do histórico da missão, não apenas um cache de retransmissão. Preservar o início da missão e todos os arquivos anteriores. Não apagar, formatar ou truncar automaticamente; cartão cheio e limite de arquivo são falhas explícitas.
- FAT32 previamente preparado; capacidade de referência 32 GB, ou 64 GB se também preparado em FAT32. O FatFs instalado não suporta exFAT. Não presumir compatibilidade elétrica a partir da capacidade do cartão.
- HSPI próprio, pinos existentes de `Pinout.h`, montagem em `/sd` e diretório `/sd/obc`, frequência inicial de 4 MHz ajustável por constante. Confirmar montagem/alimentação com EPS antes do uso físico.
- Criar arquivos exclusivamente, com nome fornecido pela integração; nunca reutilizar um arquivo existente. Nome de 1–64 bytes ASCII `[A-Za-z0-9._-]`, exceto `.`/`..`, sem caminhos. Um escritor ativo, sem fila ou cache próprio.
- Cada `Append` recebe bloco não vazio e buffer do chamador estável até o retorno. Até **três tentativas totais**, sempre reposicionando no último offset confirmado e reescrevendo o bloco inteiro. Somente escrita completa e `fsync` bem-sucedido retornam sucesso. Não usa `O_APPEND`; não duplica blocos por repetir uma confirmação incerta.
- Após três falhas, bloquear escritas/aberturas. Preservar a cauda eventualmente incompleta ou não confirmada e permitir sua leitura depois do fechamento. Retomada exige `End` + `Init` e nome novo. Não há cache sobrevivendo a reset ou retenção automática do bloco depois do retorno.
- Limite por arquivo de `INT32_MAX` bytes, compatível com offsets assinados de 32 bits do runtime. Capacidade do cartão e limite por arquivo são distintos; rotação/nomes por boot pertencem à integração, sem exclusão automática.
- Acesso ESP32 serializado por mutex estático FreeRTOS, sem espera: disputa retorna `BUSY`. Inicialização inicial não concorrente, antes dos consumidores; nenhuma chamada em ISR ou no atendimento FOC. Não acessar SD/HSPI por fora do adaptador.
- Falha de montagem mantém o serviço indisponível. Não reparar FAT32 durante a missão. Em bancada, preservar cópia/imagem antes de reparo externo. Para registros v1 extraídos, reutilizar o leitor existente para recuperar os registros íntegros sem modificar a origem. Snapshots e extras ainda dependem de DAD-003 para persistência/interpretação.

## Critérios de conclusão

Operações e limites documentados e implementados, incluindo sinalização de perdas e recuperação. Compilar e conferir os caminhos de erro disponíveis. Integridade após interrupção de alimentação depende de [TES-006](../04-testes/TES-006-sd-hardware.md)/[TES-009](../04-testes/TES-009-falhas-e-recuperacao.md).

## Evidências

Implementação local verificada em 05/11/2026, sobre a base Git `28504b1` e as alterações locais anteriores de DAD-001. Sem commit, upload ou execução física. A tarefa permanece **Em andamento**, pois confirmação de módulo, alimentação e condições elétricas com EPS continuam pendentes. A política de software foi aprovada pelo responsável; os ensaios físicos conservam suas tarefas e critérios.

### Entrega implementada

- [ObcStorage.h](../../headers/ObcStorage.h) e [ObcStorage.cpp](../../src/ObcStorage.cpp): núcleo POSIX sem montagem, mutex, interpretação de conteúdo ou alocação dinâmica própria. `Storage` não copiável, com `Init`, `OpenNew`, `Append`, `Read`, `Close` e `End`. Diretório de até 127 bytes, permitindo teste com diretório temporário sem outro backend de produção.
- [ObcStorageEsp32.h](../../headers/ObcStorageEsp32.h) e [ObcStorageEsp32.cpp](../../src/ObcStorageEsp32.cpp): SD/HSPI e mutex estático, seguindo o padrão de núcleo portátil/adaptador já usado em `ObcTime`. Reutiliza dependências instaladas; nenhuma alteração em `platformio.ini`, `main.cpp`, pinagem, codec, modelos ou contratos dos setores.
- `RESULT` conserva `status`, `transferred`, `system_error` e `attempts`. Bytes transferidos são os da última tentativa, não a soma das reescritas; fora de `OK`, não comprovam persistência. `SYNC_FAILED` pode ter transferido o bloco inteiro. A integração deve contar registros somente após sucesso e conhece quantos registros existem em cada bloco; o driver não inventa contagem por conhecer apenas bytes.
- Escrita curta/zero, erros de posicionamento/escrita e falha de sincronização repetem no mesmo offset, com limite de três. `ENOSPC` e `EFBIG` são preservados como `NO_SPACE` e `FILE_LIMIT`; escrita curta sem errno definido retorna falha de escrita, sem atribuir causa não comprovada. `Close`/`End` conferem sincronização/fechamento e não repetem `close`, que pode já ter liberado o descritor. Quando limpeza também falha, preservam o erro anterior da operação.
- `Read` abre descritor próprio e lê por offset/capacidade em buffer do chamador; retorna tamanho total e bytes lidos. EOF retorna `OK` com zero bytes; offset acima do fim é inválido. Arquivo de escrita ativo retorna `BUSY`. A API permite extração futura sem carregar o arquivo inteiro; `request_log` e seu transporte ficam em INT-006.

### Procedimento e resultados

Executado em `obc-dh/`:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcStorage.cpp tests/ObcStorageCheck.cpp -Wl,--wrap=write,--wrap=read,--wrap=lseek,--wrap=fsync,--wrap=close -o /tmp/fcp01-storage-check
/tmp/fcp01-storage-check
pio run -e obc
```

Todos os comandos terminaram com código zero. O [teste nativo](../../tests/ObcStorageCheck.cpp) opera sobre arquivos reais em diretório temporário exclusivo; GNU ld injeta falhas somente nesse executável, sem interface alternativa ou simulação no código de produção. Resultado: `Armazenamento: arquivos, offsets, repetição, sincronização e falhas aprovados.` Não houve diagnóstico do UBSan.

- Criação exclusiva não alterou `normal.bin` existente; leitura observou exatamente `ABCDEF`. Nomes inválidos, travessia de diretório, ausência de arquivo, serviço inativo, buffer nulo, limites de caminho/nome/bloco/arquivo e diretório que era arquivo produziram os estados esperados.
- Leitura em offset 2 retornou `CD`; EOF em 6 retornou zero bytes; offset 7 foi rejeitado. Escritor ativo foi recusado para leitura. Leitura incompleta, erro de leitura e fechamento malsucedido foram sinalizados com quantidade/errno apropriados.
- Com prefixo confirmado `OLD`, escrita parcial, erro transitório e falha isolada de `fsync` exigiram duas tentativas cada. Os dois offsets de escrita observados foram 3; os arquivos finais continham exatamente `OLD123456`, sem duplicação ou perda do prefixo.
- Erro permanente de escrita, falha permanente de `fsync`, `ENOSPC`, falha de `lseek` e escrita de zero bytes esgotaram exatamente três tentativas. Falha de posicionamento não executou escrita. Falha de `fsync` preservou `DATA` no arquivo, mas retornou falta de confirmação; os demais casos preservaram arquivos vazios.
- Após falha final, novas escritas e aberturas foram recusadas. Repetir `Init` não limpou a falha. `End` + `Init` recuperou o serviço sem sobrescrever os arquivos: tentar o nome anterior retornou `ALREADY_EXISTS`. Falhas de sincronização/fechamento no encerramento também foram conferidas.
- Testes e entradas/resultados esperados anteriores permaneceram sem alterações. A nova verificação não exercita montagem, FreeRTOS, cartão físico ou corrupção de FAT32; não foi declarada como execução de TES-006/TES-009.

Build ESP32 aprovado: PlatformIO 6.2.0, Espressif32 6.6.0, Arduino-ESP32 2.0.14 e Xtensa 8.4.0+2021r2-patch5. O PlatformIO detectou SD/SPI/Wire instalados; os módulos novos compilaram sem avisos. Permanece o aviso externo conhecido em `esp32-hal-uart.c:153`, retorno sem valor em `uartSetPins`; framework não alterado. RAM estática 21788/327680 bytes; flash 263185/1310720 bytes. `setup()`/`loop()` continuam sem chamar a montagem ou os consumidores; esses valores não representam o consumo de missão integrado.

SHA-256 dos artefatos verificados:

```text
4c4aaba9adbb3d3df008848c93b752889fad757db44431b4607e73f3e8a8344d  obc-dh/headers/ObcStorage.h
9f4e83081a9e975b29a08803d4b333bcd9ec7eea0c86382771dda461c410402d  obc-dh/src/ObcStorage.cpp
646116bd8dc97583e020a519d7be2415b6f39a6244720fb557b95300cc8be902  obc-dh/headers/ObcStorageEsp32.h
5ebfb87f923d19fb439dca9234cca0d8f87cd3693f052f1a18e043bf149a4f96  obc-dh/src/ObcStorageEsp32.cpp
d33cb9f66c6f857c5e14d826628ae0c72e6b9460785362c75076ac689ff7caeb  obc-dh/tests/ObcStorageCheck.cpp
```

### Recuperação e pendências externas

`fsync` bem-sucedido significa confirmação pelo runtime, não garantia contra perda de alimentação ou falha do controlador/cartão. O serviço não detecta corrupção silenciosa por leitura de verificação; CRC e interpretação pertencem ao formato. As três tentativas preservam o prefixo do arquivo lógico, mas não reparam metadados FAT32 nem garantem o estado físico de setores após queda.

Após reset, preservar os arquivos anteriores e criar nome novo a partir do boot na integração. Se a montagem falhar, manter indisponibilidade e comunicar o erro; investigar alimentação/conexões e preparar cópia/imagem em bancada antes de reparo do FAT32. Não formatar nem remover a cauda por tentativa de recuperação. Em arquivo v1 legível, trabalhar em cópia estável e usar o leitor documentado em [tools/README.md](../../tools/README.md).

Faltam confirmar módulo/cartão, alimentação e condições elétricas com EPS, além de medir a frequência adequada e latência real. Boot e fluxo de dados permanecem em INT-003/INT-006; nomes por boot, contabilização de registros/perdas e encaminhamento dos erros serão ligados ali. A tarefa não foi movida para `done/`, nem os ensaios foram concluídos por compilação.

Revisão posterior de 05/11/2026: a entrega parcial de [INT-003](../03-integracao/INT-003-inicializacao-central.md) passou a chamar `Init` no boot de bancada, com diagnóstico USB e continuidade quando o SD não estiver disponível. Não abre arquivos ou grava registros. A descrição anterior e os resultados de COM-005 acima identificam o estado de sua própria entrega; código do serviço e seus hashes não mudaram. Condições elétricas, integração de dados e ensaios físicos continuam pendentes.

## Referências

- Requisitos relacionados: REQ-OBC-005, REQ-NF-OBC-004. Implementação não equivale a requisito validado.
- [obc-dh/.agents/PINAGEM.md](../../.agents/PINAGEM.md)
- [obc-dh/.agents/DEFINICOES_OBC_DP.md](../../.agents/DEFINICOES_OBC_DP.md)
- Origem no planejamento anterior: P-REGISTROS, P-SPI.
- [Painel de tarefas](../../.agents/TODO.md); [organização e rastreabilidade](../README.md).
