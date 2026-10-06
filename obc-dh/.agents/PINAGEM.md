# Pinagem do OBC - ESP32

*Base: Alocação de Pinos.pdf, com decisões vigentes informadas pelo responsável pelo OBC-DH em 06/10/2026. Definições abertas: [PENDENCIAS_ARQUITETURAIS.md](PENDENCIAS_ARQUITETURAIS.md).*

---
## Protocolos de Comunicação:

- O rádio LoRa utiliza o barramento **VSPI**; 
- O cartão SD utiliza **HSPI**; 
- Os sensores digitais compartilham um barramento **I2C**; 
- A comunicação com o Payload ocorre por **I2C bidirecional**; barramento, GPIOs e papéis ainda dependem de [P-I2C](PENDENCIAS_ARQUITETURAIS.md#p-i2c--comunicação-obcpayload-e-barramento);
- A interface de bancada utiliza **UART0** pela conexão **USB** da placa;

---

## Tabela de alocação

As direções de entrada e saída são relativas ao ESP32.

| Pino ESP32 / GPIO | Função / sinal     | Interface / barramento | Subsistema / destino   | Observações                                                                                             |
| ----------------- | ------------------ | ---------------------- | ---------------------- | ------------------------------------------------------------------------------------------------------- |
| 1 - TX0           | Transmissão (TX)   | UART0 / USB            | Bancada / solo         | Porta umbilical para telecomandos e telemetria no nível básico. Conexão USB-C.                          |
| 3 - RX0           | Recepção (RX)      | UART0 / USB            | Bancada / solo         | Recepção da interface umbilical, em conjunto com TX0.                                                   |
| 21                | SDA - dados        | I2C de sensores        | Térmico, EPS e ADCS    | Sensores previstos: LM75A, INA219, BMS, MPU9250 e BH1750. Inventário completo e ligação do Payload dependem das pendências. |
| 22                | SCL - clock        | I2C de sensores        | Térmico, EPS e ADCS    | Clock do barramento dos sensores; parâmetros e compartilhamento dependem de P-I2C.                      |
| 23                | MOSI               | VSPI                   | TT&C - LoRa SX1276     | Envio de dados ao rádio.                                                                                |
| 19                | MISO               | VSPI                   | TT&C - LoRa SX1276     | Recepção de dados do rádio.                                                                             |
| 18                | SCK                | VSPI                   | TT&C - LoRa SX1276     | Clock SPI do rádio.                                                                                     |
| 5                 | CS - seleção       | Digital - saída        | TT&C - LoRa SX1276     | Seleciona o rádio no VSPI.                                                                              |
| 4                 | DIO0 - interrupção | Digital - entrada      | TT&C - LoRa SX1276     | Sinaliza eventos como RX Done ou TX Done ao ESP32 para tratamento pelo software em FreeRTOS.            |
| 14                | RST - reset        | Digital - saída        | TT&C - LoRa SX1276     | Permite reinicializar fisicamente o rádio em caso de travamento.                                        |
| 12                | EN - habilitação   | Digital - saída        | ADCS - SimpleFOC       | Habilita ou desabilita o driver do motor. Reduz o consumo fora do modo de detumbling.                   |
| 35                | MISO               | HSPI                   | OBC - cartão SD        | Entrada (somente) de dados do SD. Substitui o GPIO 12 usado no mapeamento padrão mencionado pelo PDF.   |
| 15                | SCK                | HSPI                   | OBC - cartão SD        | Clock SPI do cartão SD.                                                                                 |
| 13                | CS - seleção       | Digital - saída        | OBC - cartão SD        | Seleciona o cartão SD para leitura e gravação.                                                          |
| 25                | PWM - fase U       | PWM                    | ADCS - SimpleFOC       | Aciona a fase U do motor brushless da roda de reação por meio do driver.                                |
| 26                | PWM - fase V       | PWM                    | ADCS - SimpleFOC       | Aciona a fase V do motor brushless por meio do driver.                                                  |
| 27                | PWM - fase W       | PWM                    | ADCS - SimpleFOC       | Aciona a fase W do motor brushless por meio do driver.                                                  |
| 32                | MOSI               | HSPI                   | OBC - cartão SD        | Envio de dados ao SD. O PDF justifica a escolha pelo comportamento esperado dessa linha durante o boot. |
| 33                | Trigger - gatilho  | Digital - saída        | Estruturas / EPS       | Aciona o MOSFET do circuito Burn Wire para derreter a linha de nylon e liberar as fitas de antena.      |

---

## Resumo por interface

| Interface         | Sinais e GPIOs                          | Uso                            |
| ----------------- | --------------------------------------- | ------------------------------ |
| UART0             | TX = 1; RX = 3                          | Comunicação de bancada / solo. |
| I2C do Payload    | Barramento e GPIOs a definir             | Comunicação bidirecional; formato JSON ou binário ainda não escolhido. |
| I2C               | SDA = 21; SCL = 22                      | Sensores térmicos, EPS e ADCS. |
| VSPI              | MOSI = 23; MISO = 19; SCK = 18; CS = 5  | Rádio LoRa SX1276.             |
| Controle do rádio | DIO0 = 4; RST = 14                      | Eventos e recuperação do LoRa. |
| HSPI              | MOSI = 32; MISO = 35; SCK = 15; CS = 13 | Cartão SD.                     |
| Controle do motor | U = 25; V = 26; W = 27; EN = 12         | Driver do ADCS / SimpleFOC.    |
| Burn Wire         | Trigger = 33                            | Liberação das antenas.         |

> GPIOs 16 e 17 não têm mais a antiga função UART2 do Payload. Não considerá-los automaticamente livres: o barramento I2C do Payload e a disponibilidade na placa ainda precisam de definição. `Pinout.h` preserva as constantes antigas até uma alteração de código específica; consultar [P-EPS-ELETRICA](PENDENCIAS_ARQUITETURAIS.md#p-eps-eletrica--energia-placa-e-sensores).

> MPU9250 = `0x68` (AD0 em GND); BH1750 #1 = `0x23` e BH1750 #2 = `0x5C`. A proposta `PAYLOAD_ADDR = 0x80` não é endereço válido de 7 bits. Consultar [P-ENDERECOS](PENDENCIAS_ARQUITETURAIS.md#p-enderecos--inventário-i2c-e-proposta-do-payload).

> O OBC é responsável pelo tempo. O uso exclusivo do relógio interno do ESP32 é uma hipótese; a presença de DS3231 externo ainda não está definida. Consultar [P-TEMPO](PENDENCIAS_ARQUITETURAIS.md#p-tempo--referência-temporal-do-obc).

---

## Notas de integração

### Separação dos barramentos SPI

A separação entre VSPI e HSPI evita que rádio e SD disputem o mesmo barramento físico e favorece operações independentes. 

A alocação apresentada é a do OBC; o documento compartilhado ainda registra o mapa inverso. Consultar [P-SPI](PENDENCIAS_ARQUITETURAIS.md#p-spi--rádio-e-armazenamento) antes da integração.

Na implementação, cada periférico ainda deve ter seu acesso coordenado quando utilizado por mais de uma tarefa. A separação física, por si só, não define a sincronização dos drivers, arquivos ou dados compartilhados.

### Inicialização e restrições dos pinos

O MISO do SD é no GPIO 35 para evitar que o cartão imponha um nível inadequado ao GPIO 12 durante a inicialização. 

Isso não torna toda a pinagem imune a problemas de boot: **GPIO 5, GPIO 12 e GPIO 15 continuam sendo strapping pins** no ESP32 clássico. Seus níveis durante reset devem ser conferidos com os circuitos conectados.

GPIOs 34 a 39 são somente de entrada e não possuem pull-up ou pull-down interno configurável por software. 

> A aplicação de GPIOs 16 e 17 também depende do módulo e do uso de flash/PSRAM. Essas restrições são documentadas pela [Espressif — GPIO & RTC GPIO](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32/api-reference/peripherals/gpio.html).

> Qual a variante real do DevKit e do módulo deve ser confirmado antes da montagem. O conector USB-C e o circuito de interface USB/UART devem ser verificados na placa adotada; o nome genérico DevKit V1 não substitui seu esquema elétrico.

### Sensores digitais e expansão

Existe uma proposta para substituir sensores analógicos NTC por LM75A e INA219 em I2C, evitando o uso de ADC nesse mapeamento. 

Também lista **GPIO 34, GPIO 36 e GPIO 39** como livres para futuras entradas e prevê expansão pelo I2C (pontos de pensamento durante a produção, para deixar o código facilmente adaptável e essas adições/modificações).

> Essas possibilidades dependem da placa real, dos endereços I2C, das características elétricas e dos ICDs. Não constituem uma alocação adicional já aprovada.

### Saídas de atuação

Os níveis ativos de EN, RST, CS, PWM e Burn Wire não foram definidos no PDF. Os estados durante boot e reset, a habilitação do motor e as condições de liberação da antena precisam ser acordados com os subsistemas responsáveis.

EN no GPIO 12 está confirmado; o código do ADCS ainda utiliza o valor antigo 33. Burn Wire no GPIO 33 libera fisicamente a retenção da antena. A flag sugerida `IS_WIRE_BURNT` é uma proposta; persistência e confirmação física continuam abertas. Consultar [P-ADCS](PENDENCIAS_ARQUITETURAIS.md#p-adcs--controle-e-integração) e [P-BURN-WIRE](PENDENCIAS_ARQUITETURAIS.md#p-burn-wire--liberação-da-antena).

> Desabilitar o driver do motor representa a intenção de economizar energia (**o consumo residual deve ser medido** - ter certeza se realmente “zera o consumo”).

---

## Pendências relacionadas

Pesquisar em [PENDENCIAS_ARQUITETURAIS.md](PENDENCIAS_ARQUITETURAIS.md) antes de implementar ou montar uma interface ainda aberta: P-I2C e P-ENDERECOS (Payload e sensores), P-SPI (rádio/SD), P-ADCS (motor/encoder), P-TEMPO (relógio), P-EPS-ELETRICA (ADC solar, placa e níveis), P-TERMICO e P-BURN-WIRE (atuações). As ações correspondentes continuam em [TODO.md](TODO.md).

---

## Referências

- **Alocação de Pinos.pdf** — fonte principal da tabela e das justificativas de roteamento; sem versão ou data explícita no arquivo.
- **Definição de Arquitetura OBC.pdf** — versão 1.5, de 10/07/2026; utilizado para identificar o RTC e as interfaces ainda sem pinagem.
- **Especificação de Requisitos OBC.pdf** — utilizado para conferir os limites de responsabilidade do OBC.
- **Espressif. GPIO & RTC GPIO — ESP32** — referência complementar para as restrições dos GPIOs; consulta em 30/09/2026, pelo link acima.
