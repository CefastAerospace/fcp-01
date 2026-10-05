# Pinagem do OBC - ESP32

*Fonte: Alocação de Pinos.pdf.*

---
## Protocolos de Comunicação:

- O rádio LoRa utiliza o barramento **VSPI**; 
- O cartão SD utiliza **HSPI**; 
- Os sensores digitais compartilham um barramento **I2C**; 
- A comunicação com o Payload ocorre por **UART2**;
- A interface de bancada utiliza **UART0** pela conexão **USB** da placa;

---

## Tabela de alocação

As direções de entrada e saída são relativas ao ESP32.

| Pino ESP32 / GPIO | Função / sinal     | Interface / barramento | Subsistema / destino   | Observações                                                                                             |
| ----------------- | ------------------ | ---------------------- | ---------------------- | ------------------------------------------------------------------------------------------------------- |
| 1 - TX0           | Transmissão (TX)   | UART0 / USB            | Bancada / solo         | Porta umbilical para telecomandos e telemetria no nível básico. Conexão USB-C.                          |
| 3 - RX0           | Recepção (RX)      | UART0 / USB            | Bancada / solo         | Recepção da interface umbilical, em conjunto com TX0.                                                   |
| 16                | Recepção (RX2)     | UART2                  | Payload - Raspberry Pi | Recebe pacotes JSON com dados ADS-B já decodificados.                                                   |
| 17                | Transmissão (TX2)  | UART2                  | Payload - Raspberry Pi | Envia comandos de status ou requisições de temperatura interna.                                         |
| 21                | SDA - dados        | I2C único              | Térmico, EPS e ADCS    | Barramento mestre para LM75A, INA219, BMS, MPU-6050 e GY-302.                                           |
| 22                | SCL - clock        | I2C único              | Térmico, EPS e ADCS    | Clock do barramento mestre compartilhado pelos sensores I2C.                                            |
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
| UART2             | TX = 17; RX = 16                        | Comunicação com o Payload.     |
| I2C               | SDA = 21; SCL = 22                      | Sensores térmicos, EPS e ADCS. |
| VSPI              | MOSI = 23; MISO = 19; SCK = 18; CS = 5  | Rádio LoRa SX1276.             |
| Controle do rádio | DIO0 = 4; RST = 14                      | Eventos e recuperação do LoRa. |
| HSPI              | MOSI = 32; MISO = 35; SCK = 15; CS = 13 | Cartão SD.                     |
| Controle do motor | U = 25; V = 26; W = 27; EN = 12         | Driver do ADCS / SimpleFOC.    |
| Burn Wire         | Trigger = 33                            | Liberação das antenas.         |

> O DS3231 está confirmado na arquitetura como dispositivo I2C, mas não aparece explicitamente na tabela de pinagem original. A integração ao barramento SDA = 21 / SCL = 22 deve ser confirmada no ICD e no esquema elétrico.

---

## Notas de integração

### Separação dos barramentos SPI

A separação entre VSPI e HSPI evita que rádio e SD disputem o mesmo barramento físico e favorece operações independentes. 

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

> Desabilitar o driver do motor representa a intenção de economizar energia (**o consumo residual deve ser medido** - ter certeza se realmente “zera o consumo”).

---

## Pendências relacionadas

| Pendência | Definição necessária |
|---|---|
| ADC das placas solares | A arquitetura prevê essa leitura, mas a pinagem evita ADC e não reserva um pino para ela. |
| Encoder SPI do ADCS | A arquitetura prevê a interface, mas não há alocação para o encoder nem definição de compartilhamento de barramento. |
| RTC DS3231 | Confirmar ligação, endereço e configuração no I2C compartilhado. |
| Thermal Watchdog | A arquitetura cita seu acionamento, mas não define interface, circuito ou sinal dedicado. |
| Controle físico do ADCS | Confirmar a divisão entre OBC e ADCS, pois os requisitos limitam o OBC à coordenação lógica, enquanto a pinagem prevê PWM e EN do motor. |
| Elétrica e temporização | Confirmar alimentação, GND, níveis lógicos, resistores, velocidades dos barramentos e estados no reset. |

O detalhamento dessas pendências está em [ARCHITECTURE.md](ARCHITECTURE.md), etapa 1. As ações correspondentes estão em [TODO.md](TODO.md).

---

## Referências

- **Alocação de Pinos.pdf** — fonte principal da tabela e das justificativas de roteamento; sem versão ou data explícita no arquivo.
- **Definição de Arquitetura OBC.pdf** — versão 1.5, de 10/07/2026; utilizado para identificar o RTC e as interfaces ainda sem pinagem.
- **Especificação de Requisitos OBC.pdf** — utilizado para conferir os limites de responsabilidade do OBC.
- **Espressif. GPIO & RTC GPIO — ESP32** — referência complementar para as restrições dos GPIOs; consulta em 30/09/2026, pelo link acima.
