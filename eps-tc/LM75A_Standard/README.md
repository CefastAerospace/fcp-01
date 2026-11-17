# LM75A — aquisição de bancada

Driver local para **NXP LM75A**, com base no
[datasheet LM75A_4, revisão 04 de 10/07/2007](https://www.nxp.com/docs/en/data-sheet/LM75A.pdf).
Essa referência foi escolhida provisoriamente para desenvolvimento. O
fabricante dos módulos reais ainda precisa ser confirmado com Térmico.

## Responsabilidades

O OBC inicializa e arbitra o barramento. O driver recebe `TwoWire` pronto,
sem chamar `Wire.begin()`, criar tarefa, mutex, temporizador ou espera.
No ESP32, configuração e leitura devem passar pelo executor I2C existente.

Somente `int009-lm75a-check` consome esta biblioteca na integração atual.
Os endereços `0x48` a `0x4C` identificam cinco canais provisórios; não
atribuem posição física nem aprovam o mapa final de A0–A2.

## Operações

O namespace é `CEFAST::thermal`:

- `Configure(wire, address, diagnostic)` lê Conf (`0x01`) como um byte,
  preserva bits 4–1, zera bits reservados 7–5 e limpa SHUTDOWN. Não escreve
  Tos/Thyst nem espera conversão.
- `ReadRaw(wire, address, raw, diagnostic)` lê os dois bytes de Temp (`0x00`),
  MSB primeiro. `raw` permanece intacto em falha.
- `DecodeTemperature(raw, temperature_c)` converte os onze bits significativos
  com sinal, ignora os cinco LSB e aceita −55 a +125 °C. Retorno `false`
  preserva `temperature_c`, sem limitar artificialmente o valor.

O endereço de sete bits deve estar em `0x48`–`0x4F`; endereço inválido é
rejeitado antes de acessar Wire. O diagnóstico conserva fase, registrador,
status Wire e quantidade recebida. Leituras curtas permanecem indeterminadas:
não provam timeout nem ausência do dispositivo.

Todos os caminhos encerram as transações iniciadas, inclusive falhas de
buffer, NACK e leitura incompleta. A conversão dos dados brutos é separada
do I/O para executar fora do callback do barramento.

## Tempo e proteção

A resolução digital é 0,125 °C; não equivale à precisão física. O datasheet
informa conversão típica de 100 ms, sem máximo nessa especificação. O OBC
adota espera inicial provisória de 1.000 ms e controla cadência/validade.

A saída OS não é usada pela bancada. Leitura de registrador pode reconhecer
a interrupção quando OS está em modo interrupt, conforme o datasheet.
Esta biblioteca não implementa proteção térmica nem autoriza atuação.
Montagem, mapa térmico, proteção e parâmetros de voo permanecem nas
integrações e ensaios correspondentes.

## Verificação

`obc-dh/tests/ObcThermalCheck.cpp` executa o driver real contra registradores
restritos de ensaio, com vetores independentes, erros e política temporal.
O roteiro `ObcThermalBenchCheck.py` observa o ESP32 sem sensores por 30 s.
Procedimentos e resultados estão na INT-009 e no guia de ferramentas OBC.
Nenhuma dessas verificações comprova medição por um LM75A físico.
