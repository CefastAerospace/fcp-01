# *On-Boarding Computer & Data Handling* (OBC-DH)

Software de bordo em C++ para o ESP32, com FreeRTOS, destinado à coordenação dos subsistemas e à organização dos dados da missão.

---

## O que é o OBC?

É o núcleo de processamento e gerenciamento de dados do CubeSat. Ele executa o software de voo, acompanha os subsistemas e organiza comandos, telemetria e registros da missão.

- **Recebimento de dados:** O Payload recebe e decodifica os sinais ADS-B das aeronaves. O OBC recebe os dados já decodificados, associa informações de tempo, armazena os registros e prepara a telemetria que será enviada ao solo pelo TT&C. 

- **Envio de dados:** No sentido contrário, recebe os telecomandos do TT&C, valida as solicitações e executa as ações permitidas.

### Conceitos principais

| Termo       | Significado                                                                     |
| ----------- | ------------------------------------------------------------------------------- |
| Payload     | Carga útil responsável pela recepção e decodificação ADS-B.                     |
| Telemetria  | Informações sobre o estado do CubeSat e os dados da missão enviadas ao solo.    |
| Telecomando | Solicitação enviada pelo solo para executar uma ação a bordo.                   |
| EPS         | Subsistema responsável pela energia e pela distribuição elétrica.               |
| ADCS        | Subsistema responsável pela determinação e pelo controle de atitude.            |
| ConOps      | Conceito de operações que define os modos e o comportamento esperado da missão. |
| ICD         | Documento que define a interface acordada entre subsistemas.                    |

---

## Funcionalidades previstas

- **Inicialização automática:** começar a operação após a energização.
- **Coordenação da missão:** administrar os modos previstos no ConOps e as prioridades de execução.
- **Dados ADS-B:** receber informações já decodificadas do Payload e associar timestamps.
- **Telemetria e telecomandos:** compilar o estado do sistema e executar comandos válidos.
- **Armazenamento:** registrar dados da missão, telemetria, eventos e estatísticas em cartão SD.
- **Supervisão:** identificar falhas e perda de comunicação, registrar reinicializações e apoiar a recuperação automática.
- **Gestão lógica de energia:** ajustar as atividades ao orçamento fornecido pelo EPS.

Essas funcionalidades representam o escopo esperado. A documentação recebida não permite afirmar quais já estão implementadas; o acompanhamento deve ser feito em [TODO.md](TODO.md).

---

## Documentação do projeto

| Arquivo / material | Finalidade |
|---|---|
| [README.md](README.md) | Visão geral e regras para trabalhar no projeto. |
| [PINAGEM.md](PINAGEM.md) | Mapeamento dos GPIOs e observações de integração. |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Arquitetura e requisitos organizados por dependências. |
| [TODO.md](TODO.md) | Checklist de implementação na mesma sequência da arquitetura. |
| PDFs de origem | Referência das decisões e dos requisitos que deram origem aos arquivos Markdown. |
| ConOps e ICDs | Definição dos modos de operação e dos contratos entre subsistemas. |
| Padrão existente de escrita e documentação em C++ | Convenções de código e documentação já adotadas pela equipe. |

A sequência de desenvolvimento começa pelos contratos e pela plataforma, passa por comunicação, tempo e armazenamento e chega à integração dos subsistemas, aos comandos, aos modos de voo e à recuperação de falhas.

---

## Regras de negócio e desenvolvimento

### Antes de qualquer alteração

1. **Consultar os materiais de referência:** ler as partes relevantes da arquitetura, da pinagem, dos requisitos, do ConOps, dos ICDs, do regulamento e do padrão existente de C++ antes de alterar código ou documentação.
2. **Verificar o que já existe:** antes de criar uma função, classe, driver, utilitário, script ou outra ferramenta de código, procurar implementações equivalentes no projeto e nas dependências adotadas. Reutilizar ou adaptar a solução existente quando ela atender à necessidade.
3. **Resolver divergências:** se as referências discordarem ou omitirem uma definição necessária, registrar a pendência e alinhá-la com a área responsável antes de implementar o comportamento dependente. Não escolher silenciosamente uma versão.
4. **Manter a documentação coerente:** atualizar arquitetura, pinagem e checklist quando a alteração afetar interfaces, responsabilidades, requisitos ou o comportamento esperado.

### Durante a operação

- **Respeitar as responsabilidades:** o OBC coordena o fluxo lógico; a recepção ADS-B pertence ao Payload, a comunicação RF pertence ao TT&C, o controle físico de atitude pertence ao ADCS e a gestão elétrica pertence ao EPS. A divisão do controle do motor precisa ser conciliada com a pinagem fornecida.
- **Validar antes de executar:** telecomandos devem respeitar o ICD, o estado operacional e as condições acordadas para cada ação.
- **Preservar os dados:** registros de missão e telemetria devem ser estruturados, documentados e recuperáveis para análise posterior.
- **Manter a referência de tempo:** cada mensagem ADS-B processada deve ter timestamp com resolução suficiente para a missão; eventos operacionais também devem registrar horário.
- **Priorizar as atividades críticas:** em disponibilidade energética reduzida, aplicar as prioridades definidas com EPS e ConOps.
- **Supervisionar a operação:** detectar falhas e perda de comunicação e realizar a recuperação prevista dentro dos limites definidos pela equipe.
- **Seguir as restrições do projeto:** respeitar a arquitetura lógica do CEFAST Aerospace, os ICDs, a alimentação compatível com o EPS e o regulamento da competição.

---

## Módulos previstos

| Área | Responsabilidade |
|---|---|
| Plataforma e comunicação | Inicialização do ESP32, configuração dos GPIOs, barramentos e tarefas FreeRTOS. |
| Tempo e dados | Referência do RTC, timestamps e organização dos registros. |
| Armazenamento | Escrita e leitura do SD, eventos e estatísticas. |
| Interfaces dos subsistemas | Troca de dados com Payload, TT&C, EPS, ADCS e sistema térmico. |
| Missão e supervisão | Telecomandos, telemetria, modos de operação, prioridades e recuperação. |

Essa divisão descreve responsabilidades lógicas. Os nomes e diretórios de implementação devem seguir a arquitetura padronizada existente.

---

## Organização do código

```text
obc-dh/
├── .agents/          # Documentação e instruções do projeto.
├── headers/         # Declarações, tipos, constantes e interfaces públicas.
│   └── Pinout.h      # Constantes de GPIO conforme PINAGEM.md.
├── src/             # Implementações dos módulos do firmware.
└── tests/           # Testes de comportamento dos módulos e de integração.
```

O header [Pinout.h](../headers/Pinout.h) expõe os GPIOs como constantes `constexpr int` no namespace `pinout`, agrupadas por interface. Ele não inicializa hardware nem define níveis ativos, velocidades ou políticas de atuação. As pendências de [PINAGEM.md](PINAGEM.md) continuam válidas.

O estado mutável deve pertencer ao módulo responsável. Quando uma variável compartilhada for necessária, declarar com `extern` no header e fornecer uma única definição no `.cpp`; o acesso entre tarefas deve ser coordenado.

Criar arquivos de implementação e testes conforme os comportamentos forem desenvolvidos, sem módulos vazios antecipados. Arquivos `.gitkeep` podem preservar pastas vazias no Git. O [platformio.ini](../platformio.ini) configura o ambiente `obc` com Arduino, implementações em `src/`, headers em `headers/` e testes em `tests/`. Consulte [FUNCIONAMENTO_PIO.md](../FUNCIONAMENTO_PIO.md) para instalação e uso. O ponto de entrada do firmware deve ser implementado conforme o comportamento acordado.

Testes de lógica podem executar no computador quando independentes do hardware. Comunicação, estados em boot/reset e temporização precisam de validação no ESP32. Organizar testes por contratos e cenários, sem exigir um teste separado para cada função privada ou getter.

---

## Validação e configuração

Os testes devem verificar inicialização automática, troca de dados, timestamps, telemetria, telecomandos, modos operacionais e recuperação de falhas. Os critérios estão em [ARCHITECTURE.md](ARCHITECTURE.md), etapa 10, e no checklist correspondente.

Antes de executar em hardware, conferir [PINAGEM.md](PINAGEM.md), os ICDs, a correspondência do perfil de placa configurado e os estados das saídas durante boot e reset. Registrar dependências e parâmetros no `platformio.ini` conforme forem integrados e validados.

---

## Materiais de referência

- Arquivos `.md` na pasta - Visão geral de todos os aspectos do projeto.
- ConOps, ICDs, arquitetura lógica do CEFAST Aerospace, regulamento CubeDesign e padrão existente de C++ - consultar suas versões vigentes no acervo do projeto.
