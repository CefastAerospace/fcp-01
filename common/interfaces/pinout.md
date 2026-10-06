# Pinout e Alocação de Barramentos: FCP-01

Registro compartilhado de endereços I2C, barramentos SPI e pinos reservados. Atualizar sempre que um endereço/pino for definido ou mudar. Decisões abertas e divergências devem ser consultadas em [PENDENCIAS_ARQUITETURAIS.md](../../obc-dh/.agents/PENDENCIAS_ARQUITETURAIS.md); uma proposta não equivale a uma alocação aprovada.

## Barramentos SPI (ESP32)

| Barramento | Dispositivo | Subsistema |
|---|---|---|
| VSPI | Cartão SD | OBC&DH |
| HSPI | LoRa SX1276 | TT&C |

Esta tabela preserva a alocação anterior, ainda não conciliada com a pinagem do OBC, que utiliza LoRa no VSPI e SD no HSPI. Confirmar o esquema elétrico antes da integração; consultar [P-SPI](../../obc-dh/.agents/PENDENCIAS_ARQUITETURAIS.md#p-spi--rádio-e-armazenamento).

## Barramento I2C (compartilhado)

| Endereço | Dispositivo | Subsistema | Observação |
|---|---|---|---|
| 0x68 | MPU9250 | ADCS | Confirmado pelo responsável pelo OBC-DH; AD0 em GND. |
| 0x23 | BH1750 #1 | ADCS | Confirmado pelo responsável pelo OBC-DH; ADDR em GND. |
| 0x5C | BH1750 #2 | ADCS | Confirmado pelo responsável pelo OBC-DH; ADDR em VCC. |
| 0x40 (proposto) | INA219 #1 | EPS | Confirmar endereço e configuração dos pinos A0/A1. |
| 0x41 (proposto) | INA219 #2 | EPS | A0=VCC, A1=GND na documentação anterior; confirmar módulo e configuração. |
| 0x48 (proposto) | LM75A | TC | Confirmar endereço e configuração dos pinos A0–A2. |
| Não definido | Payload - Raspberry Pi Zero W | Payload | I2C bidirecional confirmado; proposta `0x80` ainda não é endereço válido de 7 bits. |
| Não alocado | RTC externo | OBC&DH | DS3231 é referência histórica; presença do componente depende da política de tempo. |

Os endereços da tabela usam a convenção de **7 bits**. `0x80` está fora dessa faixa; se representar o byte de escrita, corresponde a `0x40`, já proposto para INA219 #1. Não converter nem escolher outro endereço sem alinhamento. Se o DS3231 for mantido no mesmo barramento, seu `0x68` conflitará com a IMU. Consultar [P-ENDERECOS](../../obc-dh/.agents/PENDENCIAS_ARQUITETURAIS.md#p-enderecos--inventário-i2c-e-proposta-do-payload).

O barramento do Payload, os papéis dos dispositivos e a intenção de envio iniciado por qualquer lado ainda precisam de definição. Não presumir compartilhamento com os sensores; consultar [P-I2C](../../obc-dh/.agents/PENDENCIAS_ARQUITETURAIS.md#p-i2c--comunicação-obcpayload-e-barramento).

## Outros pinos reservados

| Recurso | Subsistema | Tipo | Observação |
|---|---|---|---|
| Burn Wire da antena | Estruturas / EPS / OBC-DH | Digital - saída | GPIO 33 comanda o MOSFET do circuito de queima; consultar P-BURN-WIRE no documento de pendências. |
| SimpleFOC Mini (roda de reação) | ADCS | PWM (3 fases) + encoder | EN no GPIO 12 confirmado. Fases, encoder e integração dependem de P-ADCS no documento de pendências. |

## Como manter

Qualquer subsistema que adicionar um novo módulo I2C/SPI ou reservar um pino deve atualizar esta tabela antes de integrar o hardware físico com o resto do satélite.
