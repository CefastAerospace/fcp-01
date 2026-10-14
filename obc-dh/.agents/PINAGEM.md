# Pinagem do OBC — ESP32

Base: Alocação de Pinos.pdf e decisões do responsável. As definições de 06/10/2026 sobre sensores, motor e Burn Wire permanecem; a revisão atual substitui I2C do Payload por UART e confirma ADCS no mesmo ESP32. Pendências e parâmetros estão em [TODO.md](TODO.md#2-pendências-e-dependências); decisões lógicas em [ARCHITECTURE.md](ARCHITECTURE.md).

## Interfaces e alocação

LoRa utiliza VSPI; SD utiliza HSPI; sensores/encoder compartilham I2C. Payload utiliza UART e bancada utiliza UART0/USB. Alocação SPI conciliada como decisão de projeto; conferir montagem, acesso e validação elétrica em [P-SPI](TODO.md#p-spi).

As direções são relativas ao ESP32. GPIOs 16/17 são a alocação de projeto para UART do Payload, já presente em `Pinout.h`; disponibilidade no módulo e montagem ainda precisam de conferência. O mapa compartilhado foi conciliado: LoRa/VSPI e SD/HSPI. P-SPI agora trata montagem, acesso e validação elétrica.

| Pino ESP32 / GPIO | Função / sinal     | Interface / barramento | Subsistema / destino   | Observações                                                                                             |
| ----------------- | ------------------ | ---------------------- | ---------------------- | ------------------------------------------------------------------------------------------------------- |
| 1 - TX0           | Transmissão (TX)   | UART0 / USB            | Bancada / solo         | Porta umbilical para telecomandos e telemetria no nível básico. Conexão USB-C.                          |
| 3 - RX0           | Recepção (RX)      | UART0 / USB            | Bancada / solo         | Recepção da interface umbilical, em conjunto com TX0.                                                   |
| 21                | SDA - dados        | I2C de sensores        | Térmico, EPS e ADCS    | LM75A, INA219, MPU9250, BH1750 e AS5600. Inventário/montagem dependem de P-ENDERECOS/P-ADCS; interface do BMS a conferir. |
| 22                | SCL - clock        | I2C de sensores        | Térmico, EPS e ADCS    | Clock dos sensores; parâmetros e acesso coordenado dependem de P-BARRAMENTO.                      |
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
| 17 | Transmissão (TX2) | UART2 | Payload | Alocação de projeto em Pinout.h; conferir disponibilidade no módulo e ligação ao RX do Pi. |
| 16 | Recepção (RX2) | UART2 | Payload | Alocação de projeto em Pinout.h; conferir disponibilidade no módulo e ligação ao TX do Pi. |

## Resumo por interface

| Interface | Sinais e GPIOs | Uso |
|---|---|---|
| UART0 | TX = 1; RX = 3 | Bancada / solo pela interface USB da placa. |
| UART do Payload | UART2: TX = 17; RX = 16, 115,2 kbps 8N1 | Binário compacto (~25–35 B estimados), SeqID + NACK; conferir módulo, montagem, porta Pi e enquadramento final. |
| I2C | SDA = 21; SCL = 22 | Sensores de ADCS, EPS, Térmico e encoder conforme montagem. |
| VSPI | MOSI = 23; MISO = 19; SCK = 18; CS = 5 | LoRa SX1276, 921 MHz SF7; usa SPI global. |
| Rádio | DIO0 = 4; RST = 14 | Eventos e recuperação do rádio. |
| HSPI | MOSI = 32; MISO = 35; SCK = 15; CS = 13 | microSD FAT32, instância SPI própria. |
| Motor | EN = 12; U/V/W = 25/26/27 | Integração ADCS; fases e estados dependem de confirmação. |
| Burn Wire | Trigger = 33 | MOSFET de liberação física da antena. |

UART exige níveis de 3,3 V, TX/RX cruzados e GND comum. Configuração dos dois lados e comportamento com um lado desligado dependem de [P-PAYLOAD](TODO.md#p-payload). Confirmar circuito USB/UART e conector da placa; DevKit V1 não identifica sozinho o esquema real.

## Inventário I2C

| Dispositivo | Endereço de 7 bits | Estado |
|---|---|---|
| MPU9250 | `0x68` | Confirmado; AD0 em GND. |
| BH1750 #1 / #2 | `0x23` / `0x5C` | Confirmados; ADDR em GND / VCC. |
| AS5600 | `0x36` | Encoder definido e utilizado no ADCS; validar montagem/aquisição. |
| 3x INA219 | A confirmar | Projeto EPS prevê bateria, rail principal e painéis. `0x40`/`0x41` são propostas anteriores; obter mapa final/configuração A0/A1. |
| 5x LM75A | A confirmar | Bateria/pontos críticos; `0x48` é proposta anterior. Obter mapa final/configuração A0–A2. |
| BMS | A definir, se houver interface | Conferir aquisição prevista com EPS; não presumir endereço I2C. |

O projeto EPS/Térmico prevê sensores digitais INA219/LM75A; aquisição, endereços e proteção ainda dependem dos ICDs. Payload UART não possui endereço I2C e a proposta histórica `0x80` não se aplica. Tempo usa ms monotônicos por boot e referências LoRa, com reconstrução retroativa; RTC externo fica fora da configuração atual. DS3231 só será reconsiderado se esse método não atender à validação temporal acordada; seu `0x68` conflita com a IMU no mesmo barramento. Conferir [P-ENDERECOS](TODO.md#p-enderecos), [P-BARRAMENTO](TODO.md#p-barramento) e [P-TEMPO](TODO.md#p-tempo).

## Boot, reset e restrições elétricas

O MISO do SD usa GPIO 35 para evitar que o cartão imponha nível inadequado ao GPIO 12 no boot. GPIOs 5, 12 e 15 continuam sendo strapping pins e devem ser verificados com os circuitos conectados. GPIOs 34–39 são somente de entrada e não têm pulls internos; 34/36/39 estão sem alocação neste mapa, mas disponibilidade real precisa ser confirmada. GPIOs 16/17 dependem do módulo e de flash/PSRAM.

Níveis ativos e estados de CS, RST, EN, PWM e Burn Wire precisam de acordo elétrico, inclusive antes de executar o firmware. Conferir [P-EPS-ELETRICA](TODO.md#p-eps-eletrica), [P-ADCS](TODO.md#p-adcs) e [P-BURN-WIRE](TODO.md#p-burn-wire). EN = 12 permanece confirmado, mas ADCS ainda usa 33; fases também divergem. Nenhum código foi corrigido nesta revisão documental.

Burn Wire substitui a descrição antiga de servo e libera a retenção das antenas. Remoto registra dois circuitos nichrome de 5 V / 4 Ω cada, orçamento de 2,5 A / 12,5 W transitório e acionamento fora de TX. Conferir montagem e usar `Ttc::HoldTransmit()` antes da atuação; autorização, duração, persistência e confirmação física constam do checklist. Desabilitar o motor fora do detumbling é a decisão de operação; medir consumo residual antes de afirmar que é zero.

## Referências

- [REQUISITOS.md](REQUISITOS.md#fontes-e-controle-de-origem): identificação dos PDFs de arquitetura, requisitos e alocação de pinos, com histórico preservado.
- [Mapa compartilhado](../../common/interfaces/pinout.md): alocação de projeto conciliada entre setores, com pendências elétricas/validação preservadas.
- Design Package §3.1/§6.4, conforme referências do remoto; versão Markdown citada não presente neste checkout.
- [Espressif — GPIO & RTC GPIO](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32/api-reference/peripherals/gpio.html): restrições do ESP32 clássico; consulta original em 30/09/2026.
