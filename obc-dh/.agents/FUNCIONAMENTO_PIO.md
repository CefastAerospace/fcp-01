# Funcionamento do PlatformIO no OBC-DH

## Para que serve

O **PlatformIO** reúne as ferramentas necessárias para desenvolver firmware. Ele baixa compiladores, frameworks e bibliotecas, compila o código, transfere o firmware para a placa, acompanha a comunicação serial e executa testes.

O comando utilizado no terminal é `pio`. Arduino é um dos frameworks suportados; para ESP32, também é possível utilizar ESP-IDF.

## Ferramenta, configuração e arquivos gerados

| Elemento | Função | Versionar no Git? |
|---|---|---|
| PlatformIO | Ferramenta instalada no computador de cada técnico. | Não. |
| `platformio.ini` | Define placa, framework, diretórios e dependências do projeto. | Sim. |
| `.pio/` | Contém arquivos gerados, bibliotecas baixadas e resultados da compilação local. | Não; já consta no `.gitignore`. |
| `.clangd` | Ajusta a análise das flags do ESP32 apenas neste módulo. | Sim. |
| `compile_commands.json` | Informa ao editor os comandos e caminhos de compilação locais. | Não; é gerado pelo PlatformIO e consta no `.gitignore`. |
| `../.zed/settings.json` | Configura o clangd no Zed, na raiz do repositório, com os caminhos deste computador. | Não; consta no `.gitignore` da raiz. |

A pasta `.pio/` é criada automaticamente pelo PlatformIO e pode ser recriada a partir da configuração e do código do projeto. Não deve ser copiada entre computadores como forma de configurar o ambiente. O PlatformIO também mantém ferramentas e caches fora do projeto, normalmente em `~/.platformio/`.

## Instalação no Linux

Uma opção é usar **pipx**, que instala o PlatformIO em um ambiente Python isolado.

Antes de instalar, execute `pio --version`. Se o comando funcionar, pule a instalação do PlatformIO. Se pipx já estiver instalado, pule a instalação desse pacote.

### Arch Linux e derivados

```bash
sudo pacman -S python-pipx
```

### Debian e Ubuntu

```bash
sudo apt update
sudo apt install pipx
```

### Instalação da ferramenta

Depois de instalar o pipx, execute:

```bash
pipx ensurepath
pipx install platformio
```

Reabra o terminal e confira a instalação:

```bash
pio --version
```

Também existe a extensão **PlatformIO IDE** para VS Code, que oferece uma interface para compilação, transferência e monitoramento. A extensão gerencia a instalação do PlatformIO Core; uma instalação separada via pipx não é obrigatória para usar essa interface.

## Configuração local do OBC-DH

### Configuração com Arduino

Confira o arquivo [platformio.ini](platformio.ini) diretamente na pasta `obc-dh/`. Se ele já existir, utilize a configuração versionada do projeto e pule sua criação. Caso esteja ausente, crie-o com a seguinte configuração base:

```ini
[platformio]
src_dir = src
include_dir = headers
test_dir = tests

[env:obc]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200
```

- `platform`: seleciona o suporte ao ESP32 e suas ferramentas de desenvolvimento.
- `board`: seleciona o perfil da placa. Confira se `esp32doit-devkit-v1` corresponde à placa real antes de transferir o firmware.
- `framework`: seleciona Arduino como base do firmware.
- `monitor_speed`: define a velocidade do monitor serial em baud. O firmware deve configurar sua interface serial com a mesma velocidade.
- `src_dir`, `include_dir` e `test_dir`: indicam onde ficam implementações, headers e testes, respectivamente.

Confira também as pastas `headers/`, `src/` e `tests/`; crie somente as que estiverem ausentes. No framework Arduino, o ponto de entrada normalmente fica em `src/main.cpp`, com inclusão de `Arduino.h` e implementação de `setup()` e `loop()`. Se esse ponto de entrada já existir, preserve a implementação do projeto. Caso esteja ausente, implemente-o antes de compilar. O header de pinagem, sozinho, não constitui um firmware executável.

Bibliotecas adicionais devem ser declaradas em `lib_deps` no ambiente `[env:obc]`, conforme os periféricos forem integrados. O PlatformIO baixa as dependências durante a preparação da compilação; não é necessário instalar antecipadamente bibliotecas para todos os periféricos.

Para padronizar os computadores da equipe, mantenha as versões validadas da plataforma e das bibliotecas fixadas no `platformio.ini`. A configuração base acima não fixa versões; se o arquivo do projeto já contiver versões específicas, preserve-as.

## Comandos de uso diário

Execute os comandos a partir da pasta `obc-dh/`, depois de configurar o projeto e implementar o firmware:

```bash
pio run                       # Compila o firmware.
pio device list               # Lista as portas disponíveis.
pio run -t upload             # Compila e transfere para a placa.
pio device monitor            # Abre o monitor serial.
pio test                      # Executa os testes configurados.
```

Se houver mais de um ambiente no `platformio.ini`, use `-e obc` para selecionar o ambiente do OBC, como em `pio run -e obc`.

A execução de `pio test` exige testes implementados e um ambiente adequado. Testes de lógica no computador precisam de um ambiente nativo; testes que dependem de GPIOs, barramentos, boot ou temporização precisam de validação na placa. O ambiente ESP32 não configura automaticamente testes nativos.

### Reconhecimento dos includes no Zed

O Zed utiliza o **clangd** para analisar C++. O PlatformIO conhece os caminhos das bibliotecas, mas o clangd precisa da base `compile_commands.json` para utilizar esses mesmos caminhos e parâmetros. Um erro de include no editor não implica, por si só, falha na compilação do firmware.

Execute dentro de `obc-dh/`:

```bash
pio run -e obc -t compiledb
```

Esse comando gera ou atualiza `compile_commands.json`; ele não substitui `pio run -e obc`, que compila o firmware, nem realiza upload. O clangd encontra a base automaticamente ao analisar os arquivos do módulo.

Regenere a base após adicionar arquivos de implementação, mudar dependências, flags, ambiente, placa ou framework, ou mover o projeto para outro caminho. Não edite o JSON gerado manualmente nem o copie de outro computador.

#### Configuração local do clangd

Confira o arquivo `.clangd` na raiz do módulo. Se estiver ausente, crie-o com:

```yaml
CompileFlags:
  Remove:
    - -mlongcalls
    - -fstrict-volatile-bitfields
    - -fno-tree-switch-conversion
```

As três flags são usadas pelo compilador ESP32, mas não pelo analisador clangd. A remoção vale somente para a análise no editor; o firmware continua sendo compilado com as flags do PlatformIO. Preserve outros ajustes válidos caso o arquivo já exista.

#### Configuração local do Zed

Confira `.zed/settings.json` na raiz do repositório aberto no Zed, ao lado da pasta `obc-dh/`. Se já existir, preserve as demais preferências e ajuste apenas o clangd. Caso esteja ausente, crie o arquivo com a estrutura abaixo:

```text
fcp-01/
├── .zed/
│   └── settings.json
└── obc-dh/
    ├── .clangd
    └── compile_commands.json
```

```json
{
  "lsp": {
    "clangd": {
      "binary": {
        "path": "/usr/bin/clangd",
        "arguments": [
          "--query-driver=/CAMINHO/DO/PLATFORMIO/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-*"
        ]
      }
    }
  }
}
```

Substitua `/CAMINHO/DO/PLATFORMIO` pelo diretório real que contém os pacotes instalados. Ele pode ser `~/.platformio` ou outro caminho configurado na máquina. No JSON, use o caminho absoluto completo, sem `~` ou `$HOME`. Confira também o caminho do clangd com `command -v clangd`.

O argumento `--query-driver` permite que o clangd consulte o compilador ESP32 para descobrir os headers da toolchain. Ele é uma personalização do projeto, não um padrão do Zed. O curinga final contempla os executáveis dessa toolchain.

O ajuste do binário deve ficar na raiz aberta no Zed, onde o servidor de linguagem é iniciado. Não coloque essa configuração em `obc-dh/.zed/settings.json` quando o editor estiver aberto na raiz do repositório. A configuração vale para esse projeto; o argumento autoriza a consulta somente aos executáveis da toolchain indicada. A base de compilação e os ajustes de flags permanecem dentro do OBC-DH.

Não é necessário acrescentar os caminhos do ESP32 às preferências globais do editor. Como o arquivo contém caminhos locais, cada técnico deve configurar sua própria cópia. Mantenha `/.zed/settings.json` no `.gitignore` da raiz do repositório e `/compile_commands.json` no `.gitignore` do OBC-DH.

#### Atualização do editor e diagnóstico

O Zed pode reiniciar o servidor automaticamente ao alterar essa configuração. Se necessário, com `src/main.cpp` aberto, use a paleta de comandos do Zed e execute **`editor: restart language server`**. Confira se `Arduino.h` e `Pinout.h` são reconhecidos.

Para verificar o analisador pelo terminal, use o mesmo argumento configurado no JSON local, substituindo o caminho antes de executar:

```bash
clangd --check=src/main.cpp --query-driver='/CAMINHO/DO/PLATFORMIO/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-*'
```

Se os includes continuarem ausentes, confira a geração da base, os caminhos locais e os diagnósticos do clangd antes de limpar caches ou reinstalar ferramentas. Esses ajustes não exigem alterar o código-fonte.

### Seleção da porta USB

Conecte a placa e execute `pio device list`. A porta pode aparecer como `/dev/ttyUSB0` ou `/dev/ttyACM0`; use o caminho realmente identificado.

Para selecionar explicitamente uma porta:

```bash
pio run -t upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0
```

Use `Ctrl+C` para fechar o monitor serial antes de transferir outro firmware. Permissões de acesso e regras USB dependem da distribuição Linux. Em caso de acesso negado, conferir as permissões do dispositivo e as regras recomendadas pelo PlatformIO para a distribuição utilizada.

## Antes de executar em hardware

Conferir a variante da placa, a alimentação e o mapeamento em [PINAGEM.md](.agents/PINAGEM.md). A configuração do PlatformIO não define os estados seguros das saídas nem resolve as pendências de motor, Burn Wire e strapping pins. Essas definições continuam dependendo dos contratos e do esquema elétrico do projeto.

Pesquisar essas dependências e as decisões de I2C, endereçamento e tempo em [PENDENCIAS_ARQUITETURAIS.md](PENDENCIAS_ARQUITETURAIS.md). O transporte do Payload é I2C bidirecional; `Pinout.h` ainda contém as constantes UART2 antigas, preservadas até uma alteração de código específica. O ambiente de compilação não valida os papéis I2C ou o endereço proposto `0x80`.

## Referências

- [Documentação do PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html)
- [Instalação do PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
- [Configuração do platformio.ini](https://docs.platformio.org/en/latest/projectconf/index.html)
- [Permissões e regras udev no Linux](https://docs.platformio.org/en/latest/core/installation/udev-rules.html)
- [Geração de compile_commands.json pelo PlatformIO](https://docs.platformio.org/en/latest/integration/compile_commands.html)
- [Configurações locais do Zed](https://zed.dev/docs/configuring-zed)
