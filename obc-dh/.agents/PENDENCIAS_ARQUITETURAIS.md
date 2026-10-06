# Pendências arquiteturais do OBC-DH

Este documento centraliza as definições ainda abertas entre OBC-DH e os demais subsistemas. Arquitetura, pinagem e checklist devem referenciar esta lista quando dependerem dessas definições. Uma decisão de projeto não comprova implementação ou validação.

## Decisões confirmadas em 06/10/2026

As decisões abaixo foram informadas pelo responsável pelo OBC-DH. Elas atualizam as descrições vigentes; não alteram o conteúdo histórico dos PDFs nem os requisitos e critérios de validação.

| Assunto | Decisão vigente | Limite da confirmação |
|---|---|---|
| Motor | EN do driver no GPIO 12. | O valor 33 no código do ADCS está desatualizado. Fases, níveis e inicialização ainda precisam de alinhamento. |
| IMU | MPU9250, endereço de 7 bits `0x68`, com AD0 em GND. | Menções ao MPU6050 estão desatualizadas. Conferir os demais dispositivos do barramento. |
| Luminosidade | BH1750 #1 em `0x23` (ADDR em GND) e BH1750 #2 em `0x5C` (ADDR em VCC). | Confirmar montagem, orientação e parâmetros de aquisição com ADCS. |
| Payload | Comunicação I2C bidirecional. UART2 não é mais a interface de missão do Payload. | A intenção de que qualquer lado possa iniciar o envio não define os papéis ou a arbitragem do I2C. |
| Tempo | O OBC é responsável pela referência temporal e associação dos timestamps. | Uso exclusivo do relógio interno do ESP32 é uma hipótese, não uma escolha de hardware confirmada. |
| Antena | Burn Wire, com gatilho no GPIO 33 conforme a pinagem do OBC. | O MOSFET comanda a corrente do elemento de queima para liberar a retenção física da antena. Servo está desatualizado. |

## P-I2C — Comunicação OBC–Payload e barramento

**Áreas para alinhamento:** OBC-DH, Payload, ADCS e EPS. **Etapas afetadas:** 2, 3, 6 e 9.

- Definir controlador e alvo, quem inicia cada transação e se haverá suporte a múltiplos controladores. Bidirecionalidade de dados também pode existir com um único controlador; não implica, por si só, múltiplos controladores.
- A intenção operacional é permitir envio iniciado por qualquer lado. “Quem enviar primeiro manda” não resolve tentativas simultâneas, arbitragem, perda da arbitragem, repetição ou timeout.
- Comprovar que hardware, drivers e bibliotecas do ESP32 e do Raspberry Pi Zero W suportam os papéis escolhidos. A interface I2C habitual do Raspberry Pi OS é de controlador; operação como alvo exige suporte específico.
- Definir se o Payload compartilhará SDA = 21 / SCL = 22 com os sensores ou utilizará outro barramento. Não há nova alocação de GPIOs aprovada.
- Definir velocidade, níveis lógicos, pull-ups, capacitância, comprimento das conexões e comportamento quando um dos dispositivos estiver desligado.
- Definir recuperação se SDA/SCL permanecerem presos, limites das transações e convivência com sensores e controle FOC. Tráfego do Payload ou uma falha no barramento compartilhado pode afetar o ADCS e o EPS.
- Definir taxa nominal, rajadas, buffers, filas, confirmação, repetição e tratamento de perdas. O script `payload/telemetry.py` ainda não implementa transporte I2C para o OBC.

## P-ENDERECOS — Inventário I2C e proposta do Payload

**Áreas para alinhamento:** OBC-DH, Payload, ADCS, EPS e Térmico. **Etapas afetadas:** 2, 3 e 6.

- Os endereços confirmados são MPU9250 `0x68`, BH1750 #1 `0x23` e BH1750 #2 `0x5C`, na convenção de 7 bits. Conferir o inventário e a configuração física antes da integração.
- Foi proposto `PAYLOAD_ADDR = 0x80`. Esse valor está fora da faixa de endereços I2C de 7 bits e não pode ser tratado como endereço aprovado nessa convenção.
- Se `0x80` representar o byte de endereço de escrita, que inclui o bit de leitura/escrita, o endereço de 7 bits correspondente é `0x40`; o byte de leitura seria `0x81`. `0x40` já aparece como proposta do INA219 #1 em `common/interfaces/pinout.md`. Uma eventual colisão depende de estarem no mesmo barramento.
- Confirmar a convenção esperada pelas APIs, os papéis dos dispositivos e um endereço válido, disponível e não reservado. Não converter nem escolher um substituto silenciosamente.
- Os endereços INA219 `0x40` / `0x41` e LM75A `0x48` vêm da documentação anterior e ainda precisam de confirmação com os responsáveis. Definir também modelo e endereço do BMS.
- O DS3231 dos PDFs utiliza `0x68`. Sua presença não está confirmada na configuração atual; se for mantido no barramento da IMU, haverá conflito. Não alterar MPU9250 para `0x69` nem descartar o RTC sem decisão de hardware.

## P-DADOS — Formato das mensagens e contratos

**Áreas para alinhamento:** OBC-DH, Payload e TT&C. **Etapas afetadas:** 3 a 7 e 9.

- Escolher JSON ou formato binário para o I2C. O JSON gravado localmente pelo Payload não determina o formato no enlace.
- Definir campos, tipos, unidades, limites, versão, comprimento, enquadramento, fragmentação, sequência, integridade e confirmação.
- Para formato binário, definir serialização e ordem dos bytes; “bytes brutos” não define representação portável nem autoriza transmitir diretamente a memória de uma struct.
- Definir status, temperatura, solicitações e respostas; significado de `val_flags`, validade de dados acumulados em instantes diferentes e comportamento de `seq_id` após reset.
- Definir formato e frequência de TM, catálogo de TC, parâmetros, confirmação/rejeição e tratamento de duplicatas. Conciliar menções a PUS com os pacotes binários presentes na estação de solo.
- A TM presente em `gs/src/main.cpp` não inclui os campos ADS-B da missão. Definir como esses dados chegarão ao solo e a capacidade útil do enlace.

## P-TEMPO — Referência temporal do OBC

**Área para definição:** OBC-DH, com alinhamento de Payload e coordenação. **Etapas afetadas:** 2, 4, 6, 7 e 9.

- A responsabilidade é do OBC. A hipótese atual é usar o relógio interno do ESP32, sem RTC externo; ainda falta confirmar essa escolha e sua implementação.
- DS3231 e seus valores de consumo permanecem apenas como referência histórica dos PDFs enquanto a decisão de hardware estiver aberta.
- Distinguir tempo decorrido desde o boot de data/hora absoluta. O relógio interno não fornece automaticamente UTC válido nem conserva a data/hora após perda total de alimentação.
- Definir ajuste inicial sem internet, resolução, erro/deriva admissível, validade, sincronização e recuperação após reset ou perda de energia.
- Definir o instante representado pelo timestamp do OBC e como tratar o horário que o Payload já grava. Não remover esse campo nem redefinir seu significado sem contrato.

## P-BURN-WIRE — Liberação da antena

**Áreas para alinhamento:** OBC-DH, Estruturas, EPS e TT&C. **Etapas afetadas:** 2, 7, 8 e 9.

- O mecanismo confirmado libera fisicamente a retenção da antena mediante um circuito de queima comandado por MOSFET no GPIO 33.
- Definir nível ativo, estado durante boot/reset, corrente, duração máxima, autorização, condições energéticas e possibilidade de nova tentativa.
- Foi sugerida uma flag booleana `IS_WIRE_BURNT` para impedir novo acionamento. É uma proposta, sem implementação aprovada; não estabelece uma convenção de nomes para o código.
- Uma flag volátil perde seu valor após reset. Definir persistência, estado inicial e tratamento de uma interrupção durante o acionamento.
- Distinguir comando emitido de liberação física confirmada. Definir se haverá sensor ou outro critério de sucesso; não presumir que marcar a flag comprova a queima.
- Se a antena for necessária para receber TC, definir como autorizar a primeira liberação. Esses parâmetros condicionam a implementação e não devem ser deixados a valores presumidos.

## P-ADCS — Controle e integração

**Áreas para alinhamento:** ADCS e OBC-DH. **Etapas afetadas:** 2, 3, 6 e 8.

- EN no GPIO 12 está confirmado, mas `adcs/include/config.h` ainda contém `PIN_EN = 33`. Corrigir esse código em uma alteração de implementação separada.
- Conciliar a correspondência entre fases: o ADCS utiliza GPIOs 27/26/25; a pinagem do OBC utiliza U/V/W = 25/26/27.
- Confirmar a interface final do encoder. O código utiliza AS5600 por I2C; há texto antigo citando SPI e a tabela local da arquitetura registra I2C. Definir modelo, endereço e ligação.
- Conciliar controle físico sob responsabilidade do ADCS com execução no ESP32 central. O ADCS possui `setup()` / `loop()` e ambiente PlatformIO próprios; confirmar o papel dessa bancada e a integração ao firmware do OBC.
- Definir frequência e latência admissível de FOC, aquisição e missão, comandos, status, falhas e atuação permitida em cada modo.
- O código atual realiza alinhamento do motor no boot. Definir se esse movimento é permitido e em quais condições.
- Confirmar a ação denominada “tumbling” nos requisitos versus “detumbling” nas demais referências.

## P-SPI — Rádio e armazenamento

**Áreas para alinhamento:** OBC-DH e TT&C. **Etapas afetadas:** 2, 3 e 6.

- `PINAGEM.md` e `Pinout.h` utilizam LoRa no VSPI e SD no HSPI. O README geral e a tabela anterior de `common/interfaces/pinout.md` indicam o inverso.
- Confirmar o mapa no esquema elétrico e o documento compartilhado de referência. O mapa do OBC está documentado, mas a divergência não está resolvida.
- Definir velocidades, drivers, sincronização de acesso e parâmetros de rádio/protocolo coerentes entre satélite e estação de solo.

## P-EPS-ELETRICA — Energia, placa e sensores

**Áreas para alinhamento:** EPS, OBC-DH e ADCS. **Etapas afetadas:** 2, 3, 6, 8 e 9.

- Confirmar variante do DevKit/módulo, conectores, circuito USB/UART e disponibilidade dos GPIOs. Retirar a antiga função UART2 do Payload não aprova automaticamente os GPIOs 16/17 para outro uso.
- Conferir alimentação, GND, níveis, resistores e estados em reset de CS, RST, EN, PWM e Burn Wire. GPIOs 5, 12 e 15 são strapping pins; GPIO 35 não possui pulls internos.
- Resolver medição das placas solares: ADC nos PDFs versus proposta de sensores digitais. Definir responsável, grandezas e interface, sem reservar um ADC por suposição.
- Definir a cota energética real, limiares, prioridades, baixo consumo, recuperação e possibilidade de corte de alimentação dos periféricos. Medir consumo da montagem e consumo residual do motor desabilitado.

## P-TERMICO — Thermal Watchdog

**Áreas para alinhamento:** EPS, Térmico e OBC-DH. **Etapas afetadas:** 2, 6, 8 e 9.

- A hipótese informada é uma verificação de temperatura no EPS com envio de dados para armazenamento pelo OBC. Não há confirmação do comportamento.
- Definir sensores e localização, aquisição, periodicidade, dados enviados, limites, condições de retorno e quem executa eventual proteção.
- Armazenar temperatura não comprova uma função de proteção térmica. Confirmar se haverá apenas monitoramento ou também atuação; não presumir sinal dedicado.
- Distinguir essa função do watchdog de tarefas do ESP32.

## P-MISSAO — Referências, modos e recuperação

**Áreas para alinhamento:** coordenação e responsáveis pelos subsistemas. **Etapas afetadas:** todas.

- Confirmar requisitos vigentes: capa v1.5 de 25/05/2026 versus histórico v1.6 de 29/05/2026. Obter versões vigentes de ConOps, ICDs, arquitetura lógica CEFAST e regulamento.
- Definir modos, transições, estado inicial, retomada após reset, comandos permitidos e prioridades. Nomes e prioridades no código ou README não confirmam sua aprovação no ConOps.
- Definir tempos máximos de resposta a TC, detecção de perda de comunicação, detecção de falhas e recuperação.
- Definir critérios de saúde, tentativas e quando recuperar periférico, tarefa ou OBC. Não confundir ausência de dados ADS-B com falha de comunicação do Payload.
- O watchdog não recupera falha física do ESP32. Não há redundância física confirmada do OBC ou armazenamento.

## P-REGISTROS — Persistência e integridade

**Áreas para alinhamento:** OBC-DH, EPS e coordenação. **Etapas afetadas:** 4, 5 e 9.

- Definir formato persistente, estatísticas, retenção, extração, escrita/sincronização e política de SD cheio ou indisponível.
- Definir persistência da contagem de reinicializações e dos estados necessários à retomada, incluindo a proposta do Burn Wire.
- Definir condições de interrupção de alimentação e critérios para avaliar integridade. Fechar arquivos na operação normal não garante recuperação de uma queda durante escrita.

## P-VALIDACAO — Critérios preservados e adequações pendentes

**Áreas para alinhamento:** OBC-DH e coordenação. **Etapa afetada:** 10, com verificações nas etapas anteriores.

- Os IDs dos requisitos, entradas, resultados esperados e critérios de validação existentes não foram alterados nesta atualização documental.
- O cenário de excesso de dados UART em `TODO.md` foi preservado. Ele contém uma referência ao transporte antigo e precisa de uma proposta explícita de adequação ao I2C antes de alterar o cenário.
- Preservar o teste de TC associado a REQ-OBC-001 no PDF. Esclarecer sua relação com REQ-OBC-002 e a evidência complementar de autonomia necessária, sem trocar a associação original.
- Obter limites quantitativos de tempo, resolução temporal, consumo e integridade para avaliar os requisitos. Registrar evidências; não marcar testes como concluídos a partir de decisões de projeto.

## Como encerrar uma pendência

Registrar decisão, responsável, referência e parâmetros com unidades. Atualizar os documentos dependentes e o checklist, distinguindo decisão, implementação e validação. Manter hipóteses identificadas enquanto não houver confirmação.
