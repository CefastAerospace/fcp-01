# Definições do OBC-DH para o Design Package

**Data:** 11/10/2026.
**Finalidade:** consolidar as seis definições solicitadas para a documentação da competição, com justificativas, fontes, estimativas e limites de atendimento.

**Consolidação:** 14/10/2026, incorporando o contrato JSON e os parâmetros atuais de `tt-c/include/TtcProtocol.h`. Critérios de missão/validação e definições financeiras permanecem; as premissas antigas de enquadramento binário TT&C e SF9 foram substituídas pelo contrato implementado. Referências ao DP seguem os materiais citados nesta síntese e pelos setores; a versão Markdown indicada no remoto não está presente neste checkout.

## 1. Custos e sobressalentes

### 1.1 Configuração considerada

O orçamento principal considera um ESP32, um leitor microSD, um cartão microSD de 32 GB e um conjunto de cabos e conexões. Esses componentes atendem ao processamento central, armazenamento e interligação do OBC.

O plano temporal atual utiliza ms monotônicos por boot do ESP32 e referências absolutas recebidas pelo LoRa, com reconstrução retroativa dos horários. Foi acordada a operação sem RTC externo; DS3231 adquirido permanece como compra histórica, fora da configuração aqui orçada. Sua instalação só será reconsiderada se esse método não atender à validação temporal acordada. Precisão e REQ-OBC-006 permanecem exigências; a decisão não equivale a atendimento demonstrado.

### 1.2 Política de reposição

Sobressalentes são unidades de reposição para bancada e competição. Não são componentes duplicados instalados no satélite, não aumentam a disponibilidade durante uma missão sem intervenção e não eliminam pontos únicos de falha.

| Condição | Quantidade reserva |
|---|---|
| Componente crítico, alta taxa de falhas ou uma a duas unidades necessárias | 100% da quantidade necessária. |
| Três a vinte unidades necessárias ou taxa de falhas média, quando a regra anterior não se aplica | 50% da quantidade necessária. |
| Mais de vinte unidades necessárias, quando as regras anteriores não se aplicam | 25% da quantidade necessária. |

A quantidade de reposição é calculada por `ceil(quantidade necessária × percentual)`. Em sobreposições, aplica-se o maior percentual: duas unidades recebem 100%; vinte unidades recebem pelo menos 50%; criticidade ou alta taxa de falhas prevalecem sobre qualquer faixa quantitativa. A contagem se aplica a unidades físicas iguais; conexões são tratadas provisoriamente como um conjunto completo.

**Justificativa:** para itens usados em pequena quantidade, uma única falha pode impedir a integração ou participação na competição. Uma reposição completa evita depender de aquisição imediata no local. Para itens repetidos em maior quantidade, os percentuais menores ainda oferecem várias unidades de substituição, com menor custo de estoque. Não se presume falha simultânea de todos os componentes iguais. Taxas de falha não foram medidas; a classificação atual usa quantidade e criticidade funcional, não estatísticas inventadas.

### 1.3 Componentes necessários

| Componente | Quantidade necessária | Valor unitário considerado | Subtotal |
|---|---:|---:|---:|
| ESP32 | 1 | R$ 34,24 | R$ 34,24 |
| Leitor microSD | 1 | R$ 10,92 | R$ 10,92 |
| Cartão microSD de 32 GB | 1 | R$ 50,00 | R$ 50,00 |
| Conjunto de cabos e conexões | 1 | R$ 10,00 | R$ 10,00 |
| **Total da implementação** | | | **R$ 105,16** |

### 1.4 Sobressalentes

| Componente | Percentual | Quantidade reserva | Subtotal | Motivo |
|---|---:|---:|---:|---|
| ESP32 | 100% | 1 | R$ 34,24 | Uma unidade instalada; sua indisponibilidade interrompe processamento, coordenação e comunicação lógica do sistema central. |
| Leitor microSD | 100% | 1 | R$ 10,92 | Uma unidade instalada; sua falha impede acesso ao armazenamento do OBC. O baixo custo permite reposição completa. |
| Cartão microSD de 32 GB | 100% | 1 | R$ 50,00 | Uma unidade instalada; o cartão é necessário para os registros. A reserva permite substituição diante de falha ou necessidade de diagnóstico de corrupção. |
| Conjunto de cabos e conexões | 100% | 1 | R$ 10,00 | Um conjunto necessário; uma conexão danificada pode impedir integração. A reserva completa evita depender de peças de conexão no local. |
| **Total dos sobressalentes** | | | **R$ 105,16** | |

**Orçamento previsto para implementação e sobressalentes: R$ 210,32.**

Um cartão reserva não recupera registros perdidos no cartão original. Integridade, recuperação após reset e extração dos dados continuam exigindo políticas próprias de armazenamento e validação.

### 1.5 Fontes e hipóteses financeiras

- **ESP32:** a planilha registra R$ 68,48 com taxas/frete e quantidade 2 na linha 16. Para o rateio, considera-se que a compra corresponde a duas placas físicas: `68,48 / 2 = 34,24`. O nome do item contém “2x”; se a quantidade representar dois kits de duas placas, o rateio deverá ser revisto.
- **Leitor microSD:** a linha 14 registra “2x Mini MicroSD Card Module”, quantidade de compra 1 e total R$ 21,84. Considera-se um kit com dois leitores físicos: `21,84 / 2 = 10,92`.
- **Cartão:** R$ 50,00 é uma provisão baseada no DP, seção 5.4, página 34. Não é uma cotação confirmada de cartão de 32 GB; preço, frete e compatibilidade deverão ser conferidos.
- **Conexões:** R$ 10,00 é a estimativa do DP para interligações. A composição física do conjunto ainda precisa ser fechada.
- **RTC:** a linha 11 registra DS3231 adquirido por R$ 22,12. Esse gasto não desaparece do histórico, mas não integra o subtotal da configuração atual nem seus sobressalentes.
- **Estoque existente:** unidades já compradas podem atender à reserva. O orçamento total não significa necessidade de comprar novamente todos os itens.
- **Limite:** esta tabela cobre os componentes principais do OBC. Não inclui componentes dos setores ADCS, EPS, TT&C ou Estruturas, nem custos adicionais de montagem/placa ainda não definidos. Não representa o orçamento financeiro completo do satélite.

## 2. Data budget para a missão de dez minutos

### 2.1 Capacidade e conteúdo armazenado

Adota-se cartão microSD de **32 GB, formatado em FAT32**. A capacidade foi escolhida para permitir várias execuções de missão, diagnósticos e campanha de integração, com ampla margem sobre o volume estimado. Um cartão SDHC de 32 GB permite usar FAT32 sem depender da configuração exFAT normalmente associada a cartões de maior capacidade. A compatibilidade com leitor, driver e alimentação deverá ser validada.

O OBC armazenará mensagens ADS-B já decodificadas, referências de tempo, identificação dos boots, telemetria de estado, eventos, falhas e estatísticas. Não armazenará amostras brutas de RF/IQ. Essa escolha conserva a informação necessária à análise sem transferir ao OBC o grande volume da aquisição de rádio do Payload.

### 2.2 Premissas de dimensionamento

| Parâmetro | Premissa | Fonte / motivo |
|---|---|---|
| Duração | 600 s | Missão contínua de dez minutos prevista no DP. |
| Aeronaves simultâneas | Até 20 | Cenário da competição descrito no DP. |
| Fluxo de mensagens | 40–120 mensagens/s; nominal 60/s | Estimativa da equipe no DP, seção 5.3, página 33; pendente de medição. |
| Registro binário persistente | 64 bytes por mensagem | Provisão para dados decodificados, tempo, identificação e enquadramento do OBC. |
| Registro JSON compacto | 256 bytes por mensagem | Estimativa média de dimensionamento, com margem sobre o exemplo atual do Payload e espaço para metadados. |
| Reserva por missão | 25 MB | Volume superior estimado em JSON, acrescido de registros auxiliares e margem. |

Os 64 bytes de armazenamento não correspondem aos 25–35 bytes estimados para o enlace UART no DP. O arquivo persistente inclui metadados próprios. Os 256 bytes de JSON não são tamanho fixo ou máximo garantido: tamanho real, campos, limites e registros incompletos serão definidos no contrato e medidos na integração.

### 2.3 Volume de armazenamento

O cálculo utiliza `volume = duração × mensagens por segundo × bytes por registro`. GB, MB e kB nesta estimativa são unidades decimais: `1 MB = 1.000.000 bytes`.

| Cenário | Mensagens/s | Registros em 600 s | Binário a 64 bytes | JSON a 256 bytes |
|---|---:|---:|---:|---:|
| Inferior estimado | 40 | 24.000 | 1,536 MB | 6,144 MB |
| Nominal estimado | 60 | 36.000 | 2,304 MB | 9,216 MB |
| Superior estimado | 120 | 72.000 | 4,608 MB | 18,432 MB |

As representações são alternativas. Se ambas forem mantidas, seus volumes deverão ser somados; no cenário superior, apenas os registros de missão ocupariam 23,040 MB antes dos dados auxiliares, exigindo revisar a reserva de 25 MB.

Para uma única representação, o orçamento considera **1 MB para eventos, estatísticas, referências temporais e metadados auxiliares**, mais **15,3 kB de TM de estado**: provisão de sessenta quadros JSON de até 255 bytes, conforme seção 2.4. No cenário superior em JSON, a soma estimada é `18,432 + 1 + 0,0153 = 19,4473 MB`. A reserva de 25 MB acrescenta cerca de **28,55%** sobre essa soma. A provisão anterior de 3 kB de TM pressupunha quadros binários de 50 bytes e foi substituída. Quantidade de TMs, metadados e eventos não têm máximo comprovado; a reserva depende das medições e não impõe política operacional.

A reserva não autoriza descarte de mensagens ou encerramento de gravação ao atingir 25 MB. É um valor de dimensionamento; retenção e comportamento de SD cheio/indisponível continuam dependendo do contrato de registros. Nenhuma estimativa de capacidade substitui teste de integridade após queda de energia.

O binário será serializado com campos, ordem dos bytes e versão documentados. “Bytes brutos” não significa persistir memória interna de estruturas C++ sem contrato. JSON compacto favorece inspeção; binário reduz espaço e processamento de serialização. O uso efetivo será escolhido conforme a integração, sem exigir armazenamento duplicado.

### 2.4 Volume e capacidade do downlink

O SD conservará os registros completos recebidos pelo OBC. O downlink usa estados agregados por aeronave em JSON, conforme decisão de Payload/TT&C e [contrato compartilhado](../../common/protocol/ttc-json.md). `BuildAircraft()` inclui ICAO, posição, altitude, velocidade, rumo, horário e contagem; `Send()` enfileira o corpo sem `n`. `SendTelemetry()` gera TM de estado. Não existe fragmentação nem CRC16 de aplicação; a integridade RF usa CRC do LoRa.

Mantêm-se **400 estados agregados de 150 bytes de corpo JSON** como hipótese de dimensionamento, não taxa operacional nem tamanho garantido. São 60 kB de corpos, mais até 4 kB do contador `n` inserido pelo TT&C. Para **sessenta TMs**, reserva-se o limite final de 255 bytes por quadro: **15,3 kB**. O total passa a **79,3 kB de JSON no ar**, incluindo `n` e excluindo ACKs, TCs, eventos e retransmissões. Metadados da ponte `gs`, terminadores de linha e logs do notebook não trafegam nesse payload RF.

O exemplo de aeronave do contrato tem 119 bytes já com `n`; o de TM tem 195 bytes. São exemplos, não máximos. A provisão de 160 bytes por estado agrega um corpo estimado de 150 bytes à reserva máxima de dez bytes para `n`. Valores/campos reais variam; medir serialização e ajustar o orçamento se os corpos superarem essa hipótese, preservando o limite de 245 bytes.

| Parâmetro do cálculo | Valor / origem |
|---|---|
| Frequência / SF | 921 MHz / SF7, já configurados em `TtcProtocol.h`. |
| Largura de banda | 125 kHz. |
| Coding rate | 4/6. |
| Preâmbulo | 32 símbolos. |
| Cabeçalho / CRC do rádio | Cabeçalho explícito; CRC habilitado pelo driver. |
| Low Data Rate Optimization | Desabilitada em SF7/BW125; duração do símbolo = 1,024 ms. |
| Corpo / quadro JSON | `max_body_len` = 245 bytes; `max_frame_len` = 255 bytes. Reserva de dez bytes para `n`, não cabeçalho binário antigo. |
| TM de estado | Tamanho variável; provisão conservadora de 255 bytes por quadro final. |
| Janela hipotética de TX | 200 s em 600 s, mantida só para comparar orçamentos. Não é janela medida nem duty cycle definido. |

Transmissão nominal continua **sob demanda**, com ACK prioritário e atendimento de RX entre quadros. A hipótese de 200 s não impõe alternância 2 s RX / 1 s TX nem modifica a decisão TT&C. Parâmetros do cálculo seguem o código atual; capacidade útil, contato, perdas e condições de voo exigem validação.

Para tornar o cálculo reproduzível, usa-se a fórmula LoRa com `P` = bytes JSON do quadro final, `SF = 7`, `BW = 125000`, `CR = 6`, `IH = 0`, `CRC = 1` e `DE = 0`:

```text
T_sym = 2^SF / BW
N_payload = 8 + max(ceil((8P - 4SF + 28 + 16CRC - 20IH)
                        / (4(SF - 2DE))) × CR, 0)
T_air = (32 + 4,25 + N_payload) × T_sym
```

`CR = 6` representa o denominador de 4/6. Cabeçalho e CRC físicos já entram na fórmula; não acrescentar os doze bytes do antigo protocolo binário a `P`.

| Cenário por quadro | Bytes JSON finais | Airtime calculado |
|---|---:|---:|
| Referência de JSON final de 150 bytes | 150 | 0,315648 s |
| Corpo aeronave de 150 bytes + reserva de `n` | 160 | 0,334080 s |
| Limite final / provisão TM | 255 | 0,499968 s |

Em 200 s exclusivamente dedicados a quadros de 255 bytes cabem idealmente **400 quadros inteiros**: 102 kB de JSON final, ou até 98 kB de corpos sob a reserva de dez bytes por quadro. Esse corpo inclui chaves/valores JSON, não apenas dados científicos. Esse teto desconsidera RX, ACKs, perdas, retransmissões e processamento; não representa goodput demonstrado.

O exemplo consolidado exige `400 × 0,334080 + 60 × 0,499968 = 163,63008 s`, deixando **36,36992 s** dos 200 s hipotéticos para outras transmissões e margem. São **64 kB de quadros aeronave + 15,3 kB de TM = 79,3 kB**. Os valores substituem o orçamento antigo de 63 kB/141,88 s, que pressupunha outro enquadramento e TM de 50 bytes. Não há divergência atual SF7/SF9 no código recebido; nenhum parâmetro TT&C foi alterado nesta revisão.

Essa provisão não define frequência de atualização nem comprova perdas/latência. Extração posterior dos registros completos pelo umbilical continua dependente de armazenamento e tratamento do comando no OBC; o transporte TT&C já existe, mas não implementa por si só o acesso aos arquivos.

### 2.5 Limite de validade e justificativa da agregação

No cenário nominal do DP, 60 mensagens/s de aproximadamente 30 bytes no enlace UART produzem **1,08 MB em dez minutos**, antes dos metadados de armazenamento. Esse fluxo supera a capacidade do downlink estimada. Um cartão maior resolve espaço de armazenamento, mas não aumenta a capacidade do rádio.

O DP também declara latência máxima de **2 s entre recepção e transmissão**, além de critérios de perdas. Não há demonstração de que a provisão agregada satisfaça esses critérios. Se exigirem transmissão individual de todas as mensagens, o perfil de enlace considerado não comporta o fluxo estimado. O significado desses critérios em relação a aquisição, armazenamento e transmissão agregada precisa ser conciliado com Payload e TT&C antes de declarar atendimento. Não se altera o critério existente para adequá-lo ao orçamento.

## 3. Rastreabilidade das decisões arquiteturais

As correspondências abaixo complementam a matriz histórica dos PDFs com as decisões do planejamento atual. Mapear um requisito não significa que ele já foi implementado ou validado.

| Requisito | Decisão arquitetural | Motivo / dependência |
|---|---|---|
| REQ-OBC-003 | O OBC compõe TM de estado, energia, atitude e missão; a biblioteca TT&C transporta TM e dados. | Reutiliza o protocolo existente e preserva separação entre conteúdo e transporte. Formatos/frequências dependem do contrato. |
| REQ-OBC-006 | Associar identificação do boot e ms monotônicos às mensagens, com referências absolutas recebidas pelo LoRa. | Reconstrói horário sem reescrever registros e distingue resets. Validar precisão; RTC só será reconsiderado se o método não atender aos critérios acordados. |
| REQ-OBC-007 | Supervisionar tarefas e falhas, utilizando watchdog e recuperação de periférico, tarefa ou OBC. | Evita depender de intervenção manual para travamentos recuperáveis. Limites e política de recuperação ainda precisam de definição. |
| REQ-OBC-009 | Registrar falhas e problemas operacionais persistentemente no SD. | Conserva evidências para diagnóstico e análise da missão. SD indisponível exige política própria. |
| REQ-OBC-010 | Associar referência temporal e persistir eventos operacionais. | Permite ordenar eventos dentro de cada boot e correlacioná-los com dados de missão. |
| REQ-OBC-011 | Identificar perda de comunicação por critérios de saúde e timeouts. | Distingue falha de comunicação de ausência de aeronaves ADS-B. Limites e sinais de saúde dependem dos setores. |
| REQ-OBC-012 | Priorizar atividades críticas e reduzir as demais conforme energia e modos acordados com EPS. | Preserva funções essenciais diante de disponibilidade energética reduzida. Cota, limiares e prioridades precisam de acordo. |

O watchdog externo e o firmware de recuperação em outra partição descritos no DP não são assumidos como mecanismos confirmados ou implementados. Sobressalentes também não constituem redundância operacional para atendimento de REQ-OBC-007.

## 4. Encoder AS5600 por I2C

**Definição:** adota-se encoder AS5600 conectado por I2C, endereço de sete bits `0x36`, no barramento compartilhado do ESP32 central: SDA no GPIO 21 e SCL no GPIO 22.

**Justificativa:** o DP, seção de ADCS na página 12, confirma AS5600/I2C. A compra identifica esse componente e o código ADCS utiliza `MagneticSensorI2C(AS5600_I2C)` com `encoder.init(&Wire)`. A escolha mantém coerência com o hardware adquirido e o driver existente, sem introduzir outra interface ou reservar sinais SPI.

Referências a SPI para esse encoder devem ser corrigidas nos documentos vigentes de ADCS, fluxo de dados do OBC e alocação de pinos. No repositório atual, I2C já está indicado; versões externas não disponíveis não foram corrigidas. Montagem, funcionamento e acesso coordenado com outros sensores ainda precisam de validação, principalmente por causa do atendimento frequente ao controle FOC.

## 5. IMU MPU9250

**Definição:** adota-se MPU9250 conectado por I2C, com endereço de sete bits `0x68` e AD0 em GND.

**Justificativa:** a compra identifica MPU9250, o código ADCS utiliza a biblioteca `MPU9250_WE` e o DP e a pinagem vigente registram o mesmo modelo. Corrigir menções anteriores a MPU6050 preserva coerência entre seleção de componente, configuração e documentação; não exige escolher novamente a IMU.

Documentos externos defasados deverão acompanhar essa definição. Se um DS3231 for posteriormente instalado no mesmo barramento, também utiliza `0x68` e seu conflito deverá ser resolvido por decisão elétrica, sem mudar silenciosamente o endereço confirmado da IMU.

## 6. Missão antes dos ensaios ambientais

### REQ-OBC-013 — Execução prévia da missão

| Campo | Definição |
|---|---|
| Requisito | O sistema deverá executar autonomamente a missão ADS-B completa e bem-sucedida antes do início da campanha de ensaios ambientais. |
| Requisito superior | HLR-ENV-01. |
| Prioridade | Alta. |
| Verificação | Teste integrado, com registros e evidências dos critérios de sucesso da missão. |
| Origem | DP, páginas 20 e 54: requisito proposto, pendente de incorporação às especificações de origem. |

**Justificativa:** executar e comprovar a missão antes dos ensaios ambientais atende à sequência de verificação prevista no DP e estabelece uma referência funcional anterior à exposição ambiental. Essa referência ajuda a distinguir falhas já presentes de alterações observadas após os ensaios.

A sequência de verificação deverá comprovar a missão contínua de dez minutos antes da campanha ambiental. Os ensaios ambientais, quando realizados, também estarão condicionados à aprovação no Fit-Check, conforme o DP, página 5.

Essa definição é uma condição para iniciar os ensaios, não um novo modo operacional. Sua inclusão não comprova missão executada nem altera os procedimentos e resultados esperados dos requisitos anteriores. A incorporação formal ao catálogo e à matriz de origem continua sendo uma atualização documental a realizar.

## 7. Confirmações pendentes antes da submissão definitiva

| Assunto | Confirmação necessária |
|---|---|
| Rateio financeiro | Quantidade física dos kits/placas e correspondência do ESP32 adquirido com a placa de integração. |
| Cartão e conexões | Cotação do cartão de 32 GB, composição das conexões e compatibilidade elétrica/software. |
| Tempo | Validar ms monotônicos/referências LoRa, quantização e latência de `args.unix` em segundos, erro/deriva e resets. RTC externo só será reconsiderado se o método não atender aos critérios temporais acordados. Não inferir tempo desligado após reset. |
| Formatos | Implementar binário UART decidido, preservando JSON local de bancada do Payload; definir formato persistente e medir tamanhos. Downlink já usa JSON TT&C, corpo ≤245 bytes/quadro ≤255 bytes, sem fragmentação. |
| Rádio | Validar parâmetros atuais SF7/BW125/CR4/6/preâmbulo32, capacidade útil, contato e filas. Orçamento recalculado não comprova entrega nem autoriza mudar decisões/limites TT&C. |
| Critérios de missão | Confirmar compatibilidade entre transmissão agregada, latência e perdas sem modificar critérios existentes silenciosamente. |
| Recuperação | Confirmar ou corrigir promessas de watchdog externo/partição de recuperação presentes no DP. |
| Evidências | Demonstrar integração, integridade dos registros, sensores e missão; não declarar testes aprovados a partir destas definições. |
