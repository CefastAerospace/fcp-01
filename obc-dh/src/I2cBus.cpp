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
            CHECKED_OPERATION checked_operation;
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
        DIAGNOSTICS diagnostics = {};
        bool recovery_requested = false;

        // ─── HELPERS ──────────────────────────────────────────────────────

#ifndef OBC_I2C_RECOVERY_CHECK
        // Estes acessos pertencem exclusivamente ao executor após o boot.
        bool StopController(
            void*
        ) { return Wire.end(); }

        // Reinicializa a configuração completa depois de cada limpeza elétrica.
        bool StartController(
            void*
        ) {
            if (!Wire.begin(pinout::i2c_sda, pinout::i2c_scl, clock_hz)) return false;
            Wire.setTimeOut(transfer_timeout_ms);
            return true;
        }

        // Escreve a liberação antes de habilitar dreno aberto para evitar pulso espúrio.
        void PrepareLines(
            void*
        ) {
            digitalWrite(pinout::i2c_sda, HIGH);
            digitalWrite(pinout::i2c_scl, HIGH);
            pinMode(pinout::i2c_sda, OUTPUT_OPEN_DRAIN);
            pinMode(pinout::i2c_scl, OUTPUT_OPEN_DRAIN);
        }

        // HIGH em dreno aberto libera a linha para os pull-ups da montagem.
        void SetSda(
            void*,
            bool released
        ) { digitalWrite(pinout::i2c_sda, released ? HIGH : LOW); }

        void SetScl(
            void*,
            bool released
        ) { digitalWrite(pinout::i2c_scl, released ? HIGH : LOW); }

        // A amostra representa o nível elétrico observado pelo controlador.
        bool ReadSda(
            void*
        ) { return digitalRead(pinout::i2c_sda) == HIGH; }

        bool ReadScl(
            void*
        ) { return digitalRead(pinout::i2c_scl) == HIGH; }

        // O ciclo limitado permite subtração modular em micros de 32 bits.
        uint32_t RecoveryNow(
            void*
        ) { return micros(); }

        // Intervalos longos cedem CPU; fases do clock usam espera em microssegundos.
        void RecoveryDelay(
            void*,
            uint32_t duration_us
        ) {
            const uint32_t started_us = micros();
            const TickType_t ticks = pdMS_TO_TICKS(duration_us / 1000);
            if (ticks) vTaskDelay(ticks);
            const uint32_t elapsed_us = static_cast<uint32_t>(micros()) - started_us;
            if (elapsed_us < duration_us) delayMicroseconds(duration_us - elapsed_us);
        }
#endif

        // O mesmo algoritmo atende ao backend físico e ao backend de ensaio.
        RECOVERY_IO GetRecoveryIO(
        ) {
#ifdef OBC_I2C_RECOVERY_CHECK
            return GetRecoveryCheckIO();
#else
            return {StopController, StartController, PrepareLines, SetSda, SetScl,
                ReadSda, ReadScl, RecoveryNow, RecoveryDelay, nullptr};
#endif
        }

        // Diagnósticos cumulativos conservam saturação em vez de voltar a zero.
        void Count(
            uint64_t& value,
            uint64_t amount = 1
        ) { value = UINT64_MAX - value < amount ? UINT64_MAX : value + amount; }

        // Mapeamento explícito evita acoplar valores numéricos de enums distintos.
        STATUS OperationStatus(
            OPERATION_RESULT result
        ) {
            switch (result) {
                case OPERATION_RESULT::OK: return STATUS::OK;
                case OPERATION_RESULT::DEVICE_NACK: return STATUS::DEVICE_NACK;
                case OPERATION_RESULT::TRANSFER_TIMEOUT: return STATUS::TRANSFER_TIMEOUT;
                case OPERATION_RESULT::BUS_ERROR: return STATUS::BUS_ERROR;
                case OPERATION_RESULT::READ_INCOMPLETE: return STATUS::READ_INCOMPLETE;
                default: return STATUS::OPERATION_FAILED;
            }
        }

        // Atualiza somente a classificação disponível, sem inferir causa de leitura curta.
        void CountOperation(
            STATUS result
        ) {
            switch (result) {
                case STATUS::DEVICE_NACK: Count(diagnostics.device_nacks); break;
                case STATUS::TRANSFER_TIMEOUT: Count(diagnostics.transfer_timeouts); break;
                case STATUS::BUS_ERROR: Count(diagnostics.bus_errors); break;
                case STATUS::READ_INCOMPLETE: Count(diagnostics.incomplete_reads); break;
                case STATUS::OPERATION_FAILED: Count(diagnostics.operation_failures); break;
                default: break;
            }
        }

        // Remove uma entrada mantendo a ordem de chegada das demais.
        void RemovePending(
            size_t index
        ) {
            for (size_t next = index + 1; next < pending_count; ++next) {
                pending[next - 1] = pending[next];
            }
            --pending_count;
        }

        // Recupera fora do mutex. Publica estado e conclui pendências antes de acordá-las.
        void RecoverService(
        ) {
            const RECOVERY_RESULT result = RecoverBus(GetRecoveryIO());
            SemaphoreHandle_t completed[request_capacity];
            size_t completed_count = 0;
            xSemaphoreTake(queue_mutex, portMAX_DELAY);
            Count(diagnostics.recovery_cycles);
            Count(diagnostics.recovery_attempt_count, result.attempts);
            Count(diagnostics.recovery_pulses, result.pulses);
            diagnostics.last_failure = result.failure;
            diagnostics.last_recovery_us = result.elapsed_us;
            diagnostics.sda_high = result.sda_high;
            diagnostics.scl_high = result.scl_high;
            diagnostics.state = result.recovered ? SERVICE_STATE::READY : SERVICE_STATE::UNAVAILABLE;
            if (result.recovered) Count(diagnostics.recovered);
            else {
                Count(diagnostics.exhausted);
                const uint32_t now_ms = millis();
                for (size_t index = 0; index < pending_count; ++index) {
                    REQUEST& request = *pending[index];
                    request.result = scheduling::IsExpired(request.order, now_ms)
                        ? STATUS::QUEUE_TIMEOUT : STATUS::SERVICE_UNAVAILABLE;
                    request.state = REQUEST_STATE::DONE;
                    completed[completed_count++] = request.completion;
                }
                pending_count = 0;
            }
            xSemaphoreGive(queue_mutex);
            for (size_t index = 0; index < completed_count; ++index) xSemaphoreGive(completed[index]);
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
            if (!expired) {
                diagnostics.callback_running = true;
                diagnostics.callback_started_ms = now_ms;
            }
            xSemaphoreGive(queue_mutex);

            // Uma solicitação vencida nunca acessa o dispositivo.
            STATUS result = STATUS::QUEUE_TIMEOUT;
            RECOVERY_CAUSE recovery_cause = RECOVERY_CAUSE::NONE;
            uint32_t duration_us = 0;
            bool sda_high = false;
            bool scl_high = false;
            if (!expired) {
                const uint32_t started_us = micros();
                const OPERATION_RESULT operation_result = request->checked_operation
                    ? request->checked_operation(Wire, request->context)
                    : (request->operation(Wire, request->context)
                        ? OPERATION_RESULT::OK : OPERATION_RESULT::OPERATION_FAILED);
                duration_us = static_cast<uint32_t>(micros()) - started_us;
                result = OperationStatus(operation_result);
                const RECOVERY_IO io = GetRecoveryIO();
                sda_high = io.read_sda(io.context);
                scl_high = io.read_scl(io.context);
                recovery_cause = GetRecoveryCause(operation_result, sda_high, scl_high);
            }

            // Publica o resultado antes de acordar o chamador.
            xSemaphoreTake(queue_mutex, portMAX_DELAY);
            if (!expired) {
                diagnostics.callback_running = false;
                diagnostics.last_callback_us = duration_us;
                if (duration_us > diagnostics.maximum_callback_us) diagnostics.maximum_callback_us = duration_us;
                diagnostics.sda_high = sda_high;
                diagnostics.scl_high = scl_high;
                CountOperation(result);
                if (recovery_cause != RECOVERY_CAUSE::NONE) {
                    diagnostics.trigger = recovery_cause;
                    diagnostics.state = SERVICE_STATE::RECOVERING;
                }
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
            // Depois da conclusão, nenhum acesso ao contexto ou aos campos do pedido.
            if (recovery_cause != RECOVERY_CAUSE::NONE) RecoverService();
            return true;
        }

        // Dorme sem trabalho; a notificação pertence exclusivamente a esta tarefa.
        void RunWorker(
            void* context
        ) {
            (void)context;
            for (;;) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                for (;;) {
                    xSemaphoreTake(queue_mutex, portMAX_DELAY);
                    const bool recover = recovery_requested;
                    recovery_requested = false;
                    xSemaphoreGive(queue_mutex);
                    if (recover) RecoverService();
                    if (!ProcessNext()) break;
                }
            }
        }

        // Uma reserva e uma espera comuns mantêm a semântica das duas APIs.
        STATUS Submit(
            OPERATION operation,
            CHECKED_OPERATION checked_operation,
            void* context,
            uint8_t priority,
            uint32_t max_queue_wait_ms
        );

    } // namespace

    // ─── HELPERS ──────────────────────────────────────────────────────

    bool Init(
    ) {
        if (xPortInIsrContext()) {
            return false;
        }
        if (worker_task != nullptr) {
            return GetDiagnostics().state == SERVICE_STATE::READY;
        }

        // Inicializa o controlador com a pinagem e a frequência do barramento.
        if (!Wire.begin(pinout::i2c_sda, pinout::i2c_scl, clock_hz)) {
            return false;
        }

        // Aplica o limite de espera das transferências após a inicialização.
        Wire.setTimeOut(transfer_timeout_ms);

        // Aloca sincronização uma vez; nenhuma solicitação usa alocação dinâmica.
        queue_mutex = xSemaphoreCreateMutexStatic(&queue_mutex_storage);
        if (!queue_mutex) { Wire.end(); return false; }
        for (size_t index = 0; index < request_capacity; ++index) {
            requests[index].completion = xSemaphoreCreateBinaryStatic(
                &requests[index].completion_storage
            );
            if (!requests[index].completion) { Wire.end(); return false; }
        }
        diagnostics = {};
        diagnostics.state = SERVICE_STATE::READY;

        if (xTaskCreate(
                RunWorker,
                "I2C",
                worker_stack_bytes,
                nullptr,
                worker_priority,
                &worker_task
            ) != pdPASS) {
            Wire.end();
            diagnostics.state = SERVICE_STATE::UNINITIALIZED;
            worker_task = nullptr;
            return false;
        }
        return true;
    }

    STATUS Execute(
        OPERATION operation,
        void* context,
        uint8_t priority,
        uint32_t max_queue_wait_ms
    ) { return Submit(operation, nullptr, context, priority, max_queue_wait_ms); }

    STATUS ExecuteChecked(
        CHECKED_OPERATION operation,
        void* context,
        uint8_t priority,
        uint32_t max_queue_wait_ms
    ) { return Submit(nullptr, operation, context, priority, max_queue_wait_ms); }

    namespace {

    // A posição só volta a FREE depois de o chamador consumir a conclusão.
    STATUS Submit(
        OPERATION operation,
        CHECKED_OPERATION checked_operation,
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
        if ((!operation && !checked_operation) || priority > scheduling::max_priority
                || max_queue_wait_ms == 0 || max_queue_wait_ms > 0x7FFFFFFFUL) {
            return STATUS::INVALID_ARGUMENT;
        }

        // Reserva uma posição estática e a insere no final da fila.
        xSemaphoreTake(queue_mutex, portMAX_DELAY);
        if (diagnostics.state != SERVICE_STATE::READY) {
            const STATUS result = diagnostics.state == SERVICE_STATE::RECOVERING
                ? STATUS::SERVICE_RECOVERING : STATUS::SERVICE_UNAVAILABLE;
            xSemaphoreGive(queue_mutex);
            return result;
        }
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
        request->checked_operation = checked_operation;
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

    } // namespace

    bool RequestRecovery(
    ) {
        if (xPortInIsrContext() || !worker_task || xTaskGetCurrentTaskHandle() == worker_task) return false;
        xSemaphoreTake(queue_mutex, portMAX_DELAY);
        const bool accepted = diagnostics.state == SERVICE_STATE::UNAVAILABLE;
        if (accepted) {
            diagnostics.state = SERVICE_STATE::RECOVERING;
            diagnostics.trigger = RECOVERY_CAUSE::REQUESTED;
            recovery_requested = true;
        }
        xSemaphoreGive(queue_mutex);
        if (accepted) xTaskNotifyGive(worker_task);
        return accepted;
    }

    DIAGNOSTICS GetDiagnostics(
    ) {
        if (xPortInIsrContext() || !queue_mutex) return {};
        xSemaphoreTake(queue_mutex, portMAX_DELAY);
        DIAGNOSTICS copy = diagnostics;
        copy.queued_requests = pending_count;
        xSemaphoreGive(queue_mutex);
        if (worker_task) copy.worker_stack_free_bytes = uxTaskGetStackHighWaterMark(worker_task);
        return copy;
    }

} // namespace i2c
