#include "../headers/I2cBus.h" // i2c::Init, configuração, pinagem e Wire.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "I2cBus.h".
 */

namespace i2c {

    namespace {

        // ─── STRUCTS ──────────────────────────────────────────────────────

        // Cada posição permanece ocupada até o chamador consumir a conclusão.
        enum class REQUEST_STATE {
            FREE,
            QUEUED,
            RUNNING,
            DONE
        };

        struct REQUEST {
            scheduling::REQUEST_ORDER order;
            OPERATION operation;
            void* context;
            REQUEST_STATE state;
            STATUS result;
            StaticSemaphore_t completion_storage;
            SemaphoreHandle_t completion;
        };

        // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

        REQUEST requests[request_capacity] = {};
        REQUEST* pending[request_capacity] = {};
        size_t pending_count = 0;
        StaticSemaphore_t queue_mutex_storage;
        SemaphoreHandle_t queue_mutex = nullptr;
        TaskHandle_t worker_task = nullptr;

        // ─── HELPERS ──────────────────────────────────────────────────────

        // Remove uma entrada mantendo a ordem de chegada das demais.
        void RemovePending(
            size_t index
        ) {
            for (size_t next = index + 1; next < pending_count; ++next) {
                pending[next - 1] = pending[next];
            }
            --pending_count;
        }

        // Seleciona sob o mutex; o callback nunca executa com esse mutex retido.
        bool ProcessNext(
        ) {
            xSemaphoreTake(queue_mutex, portMAX_DELAY);
            if (pending_count == 0) {
                xSemaphoreGive(queue_mutex);
                return false;
            }

            const scheduling::REQUEST_ORDER* order[request_capacity];
            for (size_t index = 0; index < pending_count; ++index) {
                order[index] = &pending[index]->order;
            }

            const uint32_t now_ms = millis();
            const size_t selected = scheduling::SelectRequest(order, pending_count, now_ms);
            REQUEST* request = pending[selected];
            const bool expired = scheduling::IsExpired(request->order, now_ms);
            RemovePending(selected);
            request->state = REQUEST_STATE::RUNNING;
            xSemaphoreGive(queue_mutex);

            // Uma solicitação vencida nunca acessa o dispositivo.
            STATUS result = STATUS::QUEUE_TIMEOUT;
            if (!expired) {
                result = request->operation(Wire, request->context)
                    ? STATUS::OK : STATUS::OPERATION_FAILED;
            }

            // Publica o resultado antes de acordar o chamador.
            xSemaphoreTake(queue_mutex, portMAX_DELAY);
            if (!expired) {
                // O callback terminou: promove somente quem ainda aguarda.
                // - Uma falha do callback também consumiu um atendimento.
                // - Descartar um pedido vencido não promove os demais.
                const uint32_t completed_at_ms = millis();
                for (size_t index = 0; index < pending_count; ++index) {
                    if (!scheduling::IsExpired(pending[index]->order, completed_at_ms)) {
                        scheduling::RegisterCompletion(pending[index]->order);
                    }
                }
            }
            request->result = result;
            request->state = REQUEST_STATE::DONE;
            xSemaphoreGive(queue_mutex);
            xSemaphoreGive(request->completion);
            return true;
        }

        // Dorme sem trabalho; a notificação pertence exclusivamente a esta tarefa.
        void RunWorker(
            void* context
        ) {
            (void)context;
            for (;;) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                while (ProcessNext()) {
                    // Reavalia prioridade e vencimento após cada operação completa.
                }
            }
        }

    } // namespace

    // ─── HELPERS ──────────────────────────────────────────────────────

    bool Init(
    ) {
        if (xPortInIsrContext()) {
            return false;
        }
        if (worker_task != nullptr) {
            return true;
        }

        // Inicializa o controlador com a pinagem e a frequência do barramento.
        if (!Wire.begin(pinout::i2c_sda, pinout::i2c_scl, clock_hz)) {
            return false;
        }

        // Aplica o limite de espera das transferências após a inicialização.
        Wire.setTimeOut(transfer_timeout_ms);

        // Aloca sincronização uma vez; nenhuma solicitação usa alocação dinâmica.
        queue_mutex = xSemaphoreCreateMutexStatic(&queue_mutex_storage);
        for (size_t index = 0; index < request_capacity; ++index) {
            requests[index].completion = xSemaphoreCreateBinaryStatic(
                &requests[index].completion_storage
            );
        }

        if (xTaskCreate(
                RunWorker,
                "I2C",
                worker_stack_bytes,
                nullptr,
                worker_priority,
                &worker_task
            ) != pdPASS) {
            Wire.end();
            return false;
        }
        return true;
    }

    STATUS Execute(
        OPERATION operation,
        void* context,
        uint8_t priority,
        uint32_t max_queue_wait_ms
    ) {
        // Impede bloqueio em ISR e recursão no próprio executor.
        if (xPortInIsrContext()) {
            return STATUS::INVALID_CONTEXT;
        }
        if (worker_task == nullptr) {
            return STATUS::NOT_INITIALIZED;
        }
        if (xTaskGetCurrentTaskHandle() == worker_task) {
            return STATUS::INVALID_CONTEXT;
        }
        if (operation == nullptr || priority > scheduling::max_priority
                || max_queue_wait_ms == 0 || max_queue_wait_ms > 0x7FFFFFFFUL) {
            return STATUS::INVALID_ARGUMENT;
        }

        // Reserva uma posição estática e a insere no final da fila.
        xSemaphoreTake(queue_mutex, portMAX_DELAY);
        REQUEST* request = nullptr;
        for (size_t index = 0; index < request_capacity; ++index) {
            if (requests[index].state == REQUEST_STATE::FREE) {
                request = &requests[index];
                break;
            }
        }
        if (request == nullptr) {
            xSemaphoreGive(queue_mutex);
            return STATUS::QUEUE_FULL;
        }

        request->order = {priority, millis(), max_queue_wait_ms, 0};
        request->operation = operation;
        request->context = context;
        request->state = REQUEST_STATE::QUEUED;
        pending[pending_count++] = request;
        xSemaphoreGive(queue_mutex);
        xTaskNotifyGive(worker_task);

        // Arredonda para cima para nunca transformar uma espera positiva em zero.
        const TickType_t wait_ticks = static_cast<TickType_t>(
            (static_cast<uint64_t>(max_queue_wait_ms) * configTICK_RATE_HZ + 999) / 1000
        );
        if (xSemaphoreTake(request->completion, wait_ticks) != pdTRUE) {
            xSemaphoreTake(queue_mutex, portMAX_DELAY);
            if (request->state == REQUEST_STATE::QUEUED) {
                for (size_t index = 0; index < pending_count; ++index) {
                    if (pending[index] == request) {
                        RemovePending(index);
                        break;
                    }
                }
                request->state = REQUEST_STATE::FREE;
                xSemaphoreGive(queue_mutex);
                return STATUS::QUEUE_TIMEOUT;
            }
            xSemaphoreGive(queue_mutex);

            // A execução começou: preserva o contexto até o callback terminar.
            xSemaphoreTake(request->completion, portMAX_DELAY);
        }

        xSemaphoreTake(queue_mutex, portMAX_DELAY);
        const STATUS result = request->result;
        request->state = REQUEST_STATE::FREE;
        xSemaphoreGive(queue_mutex);
        return result;
    }

} // namespace i2c
