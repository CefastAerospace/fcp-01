/**
 * @file I2cBus.h
 * @brief Inicialização e atendimento exclusivo do I2C com envelhecimento.
 */

#pragma once

#include "Pinout.h" // pinout::i2c_sda e pinout::i2c_scl.
#include <Wire.h>   // Wire e tipos utilizados na configuração do I2C.
#include "I2cScheduling.h" // Política de prioridade e expiração das solicitações.
#include "I2cRecovery.h" // Resultados tipados e recuperação com orçamento.
#include <freertos/FreeRTOS.h> // Tipos e constantes do FreeRTOS.
#include <freertos/semphr.h> // Mutex e semáforos de conclusão.
#include <freertos/task.h> // Tarefa responsável pelo I2C e notificações de trabalho.

namespace i2c {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    /** @brief Frequência do barramento, em Hz. */
    constexpr uint32_t clock_hz = 400000;

    /** @brief Timeout de cada transferência, em ms; não limita operações completas de drivers. */
    constexpr uint16_t transfer_timeout_ms = 20;

    /** @brief Limite de solicitações simultâneas, incluindo a operação em execução. */
    constexpr size_t request_capacity = 8;

    /** @brief Timeout padrão de espera na fila, em ms; não inclui execução. */
    constexpr uint32_t default_queue_wait_ms = 1000;

    /** @brief Prioridade provisória da tarefa I2C; independente da prioridade dos pedidos. */
    constexpr UBaseType_t worker_priority = 5;

    /** @brief Tamanho da pilha da tarefa I2C, em bytes no ESP32. */
    constexpr uint32_t worker_stack_bytes = 4096;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Resultado do atendimento de uma operação; não identifica erros internos do driver. */
    enum class STATUS {
        OK,
        NOT_INITIALIZED,
        INVALID_ARGUMENT,
        INVALID_CONTEXT,
        QUEUE_FULL,
        QUEUE_TIMEOUT,
        OPERATION_FAILED,
        DEVICE_NACK,
        TRANSFER_TIMEOUT,
        BUS_ERROR,
        READ_INCOMPLETE,
        SERVICE_RECOVERING,
        SERVICE_UNAVAILABLE
    };

    /** @brief Amostra cumulativa do serviço; contadores saturam em UINT64_MAX. */
    struct DIAGNOSTICS {
        SERVICE_STATE state;
        RECOVERY_CAUSE trigger;
        RECOVERY_CAUSE last_failure;
        bool sda_high;
        bool scl_high;
        bool callback_running;
        size_t queued_requests;
        uint32_t callback_started_ms;
        uint32_t last_callback_us;
        uint32_t maximum_callback_us;
        uint32_t last_recovery_us;
        uint32_t worker_stack_free_bytes;
        uint64_t recovery_cycles;
        uint64_t recovery_attempt_count;
        uint64_t recovery_pulses;
        uint64_t recovered;
        uint64_t exhausted;
        uint64_t device_nacks;
        uint64_t transfer_timeouts;
        uint64_t bus_errors;
        uint64_t incomplete_reads;
        uint64_t operation_failures;
    };

    /**
     * @brief Operação executada exclusivamente pela tarefa I2C.
     * @details Recebe o barramento e o contexto do chamador. Retorna true em
     * sucesso. Deve ser finita e curta, sem logs, espera de conversão ou
     * recursão em Execute(). Não deve alterar a configuração de Wire nem
     * usar as notificações da tarefa I2C. Não há preempção de uma operação
     * pelo escalonador de solicitações.
     * Toda transação iniciada deve terminar, incluindo repeated START e STOP,
     * liberando os locks internos de Wire antes do retorno.
     */
    using OPERATION = bool (*)(TwoWire& wire, void* context);

    /** @brief Callback com causa explícita, sujeito ao mesmo contrato de OPERATION. */
    using CHECKED_OPERATION = OPERATION_RESULT (*)(TwoWire& wire, void* context);

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Inicializa Wire, sincronização e a tarefa responsável pelo I2C.
     *
     * @details Chamar antes de inicializar os dispositivos e iniciar tarefas
     * consumidoras do barramento, em contexto de tarefa. Chamadas posteriores
     * bem-sucedidas não interferem no resultado final após a primeira execução;
     * inicializações concorrentes não são permitidas.
     *
     * @return true se o serviço está pronto; false em caso de falha.
     * Com executor existente, consulta prontidão sem reinicializar Wire.
     */
    bool Init(
    );

    /**
     * @brief Enfileira uma operação e aguarda sua conclusão.
     *
     * @details Uso apenas por tarefas. Todos os consumidores devem passar por
     * esta função, sem acesso direto a Wire. O mutex protege os metadados;
     * somente a tarefa I2C executa operações. O envelhecimento não altera
     * prioridades de tarefas FreeRTOS e não comprova prazos do FOC.
     * Cada callback concluído, com sucesso ou falha, envelhece as solicitações
     * ainda aguardando e não vencidas. Apenas a passagem do tempo não promove
     * solicitações; o tempo serve somente ao timeout da fila.
     *
     * O timeout cancela somente operações ainda na fila. Se a execução já
     * começou, aguarda sua conclusão para preservar a validade do contexto.
     * O callback e seus dados não podem ser destruídos durante a chamada;
     * não suspender nem excluir a tarefa chamadora enquanto ela aguarda.
     * Uma operação que não retorna impede o progresso do serviço.
     *
     * @param operation Callback finito que realiza a operação lógica completa.
     * @param context Dados do callback; pode ser nullptr se ele não precisar de dados.
     * @param priority Prioridade base entre zero e scheduling::max_priority.
     * @param max_queue_wait_ms Espera máxima na fila: de 1 a 2147483647 ms.
     * @return Estado final; operação vencida na fila não executa o callback.
     */
    STATUS Execute(
        OPERATION operation,
        void* context,
        uint8_t priority = 0,
        uint32_t max_queue_wait_ms = default_queue_wait_ms
    );

    /**
     * @brief Enfileira callback tipado; espera, contexto e prioridades seguem Execute.
     * @details Novos pedidos recebem SERVICE_RECOVERING durante recuperação.
     * Após esgotamento recebem SERVICE_UNAVAILABLE. Um callback executa uma vez.
     * O pedido ativo recebe sua causa original antes de iniciar recuperação.
     * NACK/leitura incompleta com linhas liberadas são informados ao consumidor.
     * requestFrom curto não identifica sozinho timeout ou ausência de dispositivo.
     */
    STATUS ExecuteChecked(
        CHECKED_OPERATION operation,
        void* context,
        uint8_t priority = 0,
        uint32_t max_queue_wait_ms = default_queue_wait_ms
    );

    /**
     * @brief Solicita recuperação assíncrona de serviço UNAVAILABLE pelo próprio executor.
     * @return true se aceitou o pedido; consultar GetDiagnostics para observar conclusão.
     * Retorna false em ISR, no executor, antes de Init ou em outro estado.
     */
    bool RequestRecovery(
    );

    /** @brief Copia estado protegido; em ISR/antes de Init retorna UNINITIALIZED. */
    DIAGNOSTICS GetDiagnostics(
    );

#ifdef OBC_I2C_RECOVERY_CHECK
    /** @brief Backend exclusivo da bancada e dos testes nativos do executor. */
    RECOVERY_IO GetRecoveryCheckIO(
    );
#endif

} // namespace i2c
