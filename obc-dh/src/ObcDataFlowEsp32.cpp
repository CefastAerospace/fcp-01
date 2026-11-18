#include "ObcDataFlowEsp32.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDataFlowEsp32.h".
 */

namespace obc_flow {
namespace esp32 {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    State state;
    RecordQueue records;
    Journal journal;
    StorageRecovery storage_recovery;
    STORAGE_RECOVERY_DIAGNOSTICS recovery_diagnostic = {};
    bool synchronizing_reference = false;
    obc_delivery::Queue deliveries;
    obc_downlink::LogMailbox log_mailbox;
    bool delivery_reader_ready = false;
    obc_payload::Receiver receiver;
    SnapshotBuffers snapshot_buffers;
    enum class SLOT_STATE { FREE, ENCODING, READY, WRITING };
    SLOT_STATE snapshot_state = SLOT_STATE::FREE;
    size_t snapshot_size = 0;
    portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
    enum RECORD_DROP_KIND : uint8_t { SNAPSHOT, TELEMETRY, OTHER, DROP_KIND_COUNT };
    enum RECORD_DROP_CAUSE : uint8_t {
        WRITER_UNAVAILABLE, ADMISSION_REJECTED, QUEUE_FULL, SNAPSHOT_BUSY,
        ENCODE_FAILED, TIME_SYNC, WRITE_FAILED, DROP_CAUSE_COUNT
    };
#ifdef OBC_RECORD_DROPS_BENCH
    struct RECORD_DROP_DIAGNOSTICS {
        uint64_t counts[RECORD_DROP_KIND::DROP_KIND_COUNT][RECORD_DROP_CAUSE::DROP_CAUSE_COUNT];
    };
    RECORD_DROP_DIAGNOSTICS record_drops = {};
#endif
    TaskHandle_t payload_task = nullptr;
    TaskHandle_t writer_task = nullptr;
    bool attempted = false;
    bool active = false;
    bool writer_ready = false;
    uint64_t boot_id = 0;
    uint64_t startup_storage_failures = 0;
    uint64_t last_diagnostic_ms = 0;
    uint64_t maximum_poll_gap_ms = 0;
    size_t parser_peak_bytes = 0;
    obc_storage::RESULT writer_result = {};
    constexpr uint32_t payload_stack_bytes = 8 * 1024;
    constexpr uint32_t writer_stack_bytes = 8 * 1024;
    constexpr size_t startup_margin_bytes = 4096;
    obc_health::PROGRESS process_progress = {};
    obc_health::PROGRESS writer_progress = {};
    obc_health::Monitor health_monitor;
    obc_health::REPORT health_reports[obc_health::task_count] = {};
    uint64_t last_health_ms = 0;
    bool health_sampled = false;
    uint64_t allocation_failures = 0;

    // Tabelas reais/simuladas conservam seus contextos; a decoração só mede
    // entrada/retorno. Contexto vive na pilha permanente da tarefa escritora.
    struct WRITER_IO_CONTEXT {
        JOURNAL_IO journal;
        obc_delivery::READER_IO reader;
        RECOVERY_IO recovery;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Conta uma perda sob state_lock ou durante o boot, antes de iniciar tarefas.
    void DropRecord(
        RECORD_DROP_KIND kind,
        RECORD_DROP_CAUSE cause
    ) {
        state.DropRecords();
#ifdef OBC_RECORD_DROPS_BENCH
        auto& count = record_drops.counts[kind][cause];
        if (count != UINT64_MAX) ++count;
#else
        (void)kind;
        (void)cause;
#endif
    }

    // Identifica TM entre os registros curtos do produtor e do escritor.
    RECORD_DROP_KIND EncodedRecordDropKind(
        const uint8_t* bytes,
        size_t size
    ) {
#ifdef OBC_RECORD_DROPS_BENCH
        if (bytes != nullptr && size >= 6
                && (std::memcmp(bytes, obc_record::telemetry_magic, sizeof(obc_record::telemetry_magic)) == 0
                    || (std::memcmp(bytes, obc_data::record_magic, sizeof(obc_data::record_magic)) == 0
                        && bytes[5] == static_cast<uint8_t>(obc_data::RECORD_TYPE::TELEMETRY)))) {
            return RECORD_DROP_KIND::TELEMETRY;
        }
#else
        (void)bytes;
        (void)size;
#endif
        return RECORD_DROP_KIND::OTHER;
    }

#ifdef OBC_RECORD_DROPS_BENCH
    // Publica a cópia do mesmo instante em linhas completas, fora do lock.
    void PrintRecordDrops(
        const RECORD_DROP_DIAGNOSTICS& drops,
        uint64_t sampled_ms,
        uint64_t total
    ) {
        constexpr const char* kinds[] = {"snapshot", "tm", "other"};
        static_assert(sizeof(kinds) / sizeof(kinds[0]) == RECORD_DROP_KIND::DROP_KIND_COUNT,
            "Cada classe de registro precisa de um nome no diagnóstico");
        for (size_t kind = 0; kind < RECORD_DROP_KIND::DROP_KIND_COUNT; ++kind) {
            const auto& counts = drops.counts[kind];
            Serial.printf("[FLOW-DROPS] ms=%llu total=%llu kind=%s writer_unavailable=%llu admission_rejected=%llu queue_full=%llu snapshot_busy=%llu encode_failed=%llu time_sync=%llu write_failed=%llu\n",
                static_cast<unsigned long long>(sampled_ms), static_cast<unsigned long long>(total), kinds[kind],
                static_cast<unsigned long long>(counts[RECORD_DROP_CAUSE::WRITER_UNAVAILABLE]),
                static_cast<unsigned long long>(counts[RECORD_DROP_CAUSE::ADMISSION_REJECTED]),
                static_cast<unsigned long long>(counts[RECORD_DROP_CAUSE::QUEUE_FULL]),
                static_cast<unsigned long long>(counts[RECORD_DROP_CAUSE::SNAPSHOT_BUSY]),
                static_cast<unsigned long long>(counts[RECORD_DROP_CAUSE::ENCODE_FAILED]),
                static_cast<unsigned long long>(counts[RECORD_DROP_CAUSE::TIME_SYNC]),
                static_cast<unsigned long long>(counts[RECORD_DROP_CAUSE::WRITE_FAILED]));
        }
    }
#endif

    // Publicação curta; nunca mantém o lock durante parser ou codificação.
    void MarkProcess(
        obc_health::ACTIVITY activity,
        bool completed = false
    ) {
        portENTER_CRITICAL(&state_lock);
        obc_health::Mark(process_progress, activity, obc_time::esp32::GetMonotonicMs(), completed);
        portEXIT_CRITICAL(&state_lock);
    }

    // Guarda atividade externa para renovar progresso de varreduras por bloco.
    obc_health::ACTIVITY BeginWriterActivity(
        obc_health::ACTIVITY activity
    ) {
        portENTER_CRITICAL(&state_lock);
        const auto previous = writer_progress.activity;
        obc_health::Mark(writer_progress, activity, obc_time::esp32::GetMonotonicMs());
        portEXIT_CRITICAL(&state_lock);
#ifdef OBC_TASK_HEALTH_BENCH
        obc_health::bench::Before(obc_health::TASK::WRITER, activity);
#endif
#ifdef OBC_WATCHDOG_BENCH_CASE
        if (activity == obc_health::ACTIVITY::WRITE) obc_watchdog_bench::Before(obc_watchdog::RESOURCE::WRITER);
#endif
        return previous;
    }

    // Retorno de erro também comprova que a operação terminou.
    void EndWriterActivity(
        obc_health::ACTIVITY previous
    ) {
        portENTER_CRITICAL(&state_lock);
        obc_health::Mark(writer_progress, previous, obc_time::esp32::GetMonotonicMs(), true);
        portEXIT_CRITICAL(&state_lock);
    }

    // Mesma instrumentação para o SD físico e todos os backends de bancada.
    obc_storage::RESULT HealthOpen(
        void* context,
        const char* name
    ) {
        const auto& io = static_cast<WRITER_IO_CONTEXT*>(context)->journal;
        const auto previous = BeginWriterActivity(obc_health::ACTIVITY::OPEN);
        const auto result = io.open(io.context, name);
        EndWriterActivity(previous);
        return result;
    }

    obc_storage::RESULT HealthAppend(
        void* context,
        const uint8_t* bytes,
        size_t size
    ) {
        const auto& io = static_cast<WRITER_IO_CONTEXT*>(context)->journal;
        const auto previous = BeginWriterActivity(obc_health::ACTIVITY::WRITE);
        const auto result = io.append(io.context, bytes, size);
        EndWriterActivity(previous);
        return result;
    }

    obc_storage::RESULT HealthClose(
        void* context
    ) {
        const auto& io = static_cast<WRITER_IO_CONTEXT*>(context)->journal;
        const auto previous = BeginWriterActivity(obc_health::ACTIVITY::CLOSE);
        const auto result = io.close(io.context);
        EndWriterActivity(previous);
        return result;
    }

    obc_storage::RESULT HealthRead(
        void* context,
        const char* name,
        uint32_t offset,
        uint8_t* bytes,
        size_t capacity,
        uint32_t& total
    ) {
        const auto& io = static_cast<WRITER_IO_CONTEXT*>(context)->reader;
        const auto previous = BeginWriterActivity(obc_health::ACTIVITY::READ);
        const auto result = io.read(io.context, name, offset, bytes, capacity, total);
        EndWriterActivity(previous);
        return result;
    }

    obc_storage::RESULT HealthList(
        void* context,
        const char* after,
        obc_storage::FILE_INFO& out
    ) {
        const auto& io = static_cast<WRITER_IO_CONTEXT*>(context)->reader;
        const auto previous = BeginWriterActivity(obc_health::ACTIVITY::LIST);
        const auto result = io.list(io.context, after, out);
        EndWriterActivity(previous);
        return result;
    }

    obc_storage::RESULT HealthEnd(
        void* context
    ) {
        const auto& io = static_cast<WRITER_IO_CONTEXT*>(context)->recovery;
        const auto previous = BeginWriterActivity(obc_health::ACTIVITY::UNMOUNT);
        const auto result = io.end(io.context);
        EndWriterActivity(previous);
        return result;
    }

    obc_storage::RESULT HealthInit(
        void* context
    ) {
        const auto& io = static_cast<WRITER_IO_CONTEXT*>(context)->recovery;
        const auto previous = BeginWriterActivity(obc_health::ACTIVITY::MOUNT);
        const auto result = io.init(io.context);
        EndWriterActivity(previous);
        return result;
    }

    // Apenas o escritor acessa estas funções do driver, inclusive remontagem.
#ifndef OBC_FLOW_BENCH
    obc_storage::RESULT EndStorage(
        void*
    ) { return obc_storage::esp32::End(); }

    obc_storage::RESULT InitStorage(
        void*
    ) { return obc_storage::esp32::Init(); }

    obc_storage::RESULT Open(
        void*,
        const char* name
    ) { return obc_storage::esp32::OpenNew(name); }

    obc_storage::RESULT Append(
        void*,
        const uint8_t* bytes,
        size_t size
    ) { return obc_storage::esp32::Append(bytes, size); }

    obc_storage::RESULT Close(
        void*
    ) { return obc_storage::esp32::Close(); }

    // Leitura no escritor pode ceder CPU ao transporte UART a cada bloco.
    obc_storage::RESULT ReadDelivery(
        void*,
        const char* name,
        uint32_t offset,
        uint8_t* bytes,
        size_t capacity,
        uint32_t& total
    ) {
        const auto result = obc_storage::esp32::Read(name, offset, bytes, capacity, total);
        vTaskDelay(pdMS_TO_TICKS(1));
        return result;
    }

    // O cursor lexical não mantém descritor de diretório entre chamadas.
    obc_storage::RESULT ListDelivery(
        void*,
        const char* after,
        obc_storage::FILE_INFO& out
    ) { return obc_storage::esp32::List(after, out); }
#endif

    // Cópia limitada sob proteção; codificação e notificações ficam fora dela.
    void Submit(
        const uint8_t* bytes,
        size_t size
    ) {
        portENTER_CRITICAL(&state_lock);
        const bool queued = writer_ready && records.Push(bytes, size);
        if (!queued) {
            const auto kind = EncodedRecordDropKind(bytes, size);
            if (!writer_ready) DropRecord(kind, RECORD_DROP_CAUSE::WRITER_UNAVAILABLE);
            else if (bytes == nullptr || size == 0 || size > small_record_bytes) {
                DropRecord(kind, RECORD_DROP_CAUSE::ADMISSION_REJECTED);
            } else DropRecord(kind, RECORD_DROP_CAUSE::QUEUE_FULL);
        }
        portEXIT_CRITICAL(&state_lock);
        if (queued) xTaskNotifyGive(writer_task);
    }

    // Falha de geração também é perda explícita, sem tentar registrar a si mesma.
    void SubmitRecord(
        const obc_record::RECORD& record
    ) {
        uint8_t bytes[small_record_bytes];
        size_t size = 0;
        if (obc_record::Encode(record, bytes, sizeof(bytes), size) == obc_record::STATUS::OK) {
            Submit(bytes, size);
        } else {
            portENTER_CRITICAL(&state_lock);
            DropRecord(record.header.type == obc_data::RECORD_TYPE::TELEMETRY
                ? RECORD_DROP_KIND::TELEMETRY : RECORD_DROP_KIND::OTHER, RECORD_DROP_CAUSE::ENCODE_FAILED);
            portEXIT_CRITICAL(&state_lock);
        }
    }

    // Eventos reutilizam o diário e suas perdas; sem boot não há registro fabricado.
    void RecordResourceEvent(
        obc_resource_health::CODE code,
        bool fault,
        const char* description,
        uint64_t now_ms
    ) {
        obc_record::RECORD record = {};
        if (obc_resource_health::MakeEvent(code, fault, description, {boot_id, now_ms}, record)) SubmitRecord(record);
    }

    // Não aninha locks nem mede pilhas sob lock; tarefa ausente tem active=false.
    obc_resource_health::MEMORY_SAMPLE SampleMemory(
        const LINK_DIAGNOSTICS& ttc
    ) {
        obc_resource_health::MEMORY_SAMPLE sample = {};
        sample.byte_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        sample.byte_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        const auto uart = obc_payload::esp32::GetTransportStatistics();
        const auto bus = i2c::GetDiagnostics();
        sample.stacks[0] = {uart.progress.enabled, uart.stack_free_bytes};
        sample.stacks[1] = {payload_task != nullptr, payload_task ? uxTaskGetStackHighWaterMark(payload_task) : 0U};
        sample.stacks[2] = {writer_task != nullptr, writer_task ? uxTaskGetStackHighWaterMark(writer_task) : 0U};
        sample.stacks[3] = {bus.state != i2c::SERVICE_STATE::UNINITIALIZED, bus.worker_stack_free_bytes};
        sample.stacks[4] = {ttc.task_started, ttc.stack_free_bytes};
        sample.stacks[5] = {true, uxTaskGetStackHighWaterMark(nullptr)};
        portENTER_CRITICAL(&state_lock);
        sample.allocation_failures = allocation_failures;
        portEXIT_CRITICAL(&state_lock);
#ifdef OBC_RESOURCES_BENCH
        obc_resources_bench::OverrideMemory(sample);
#endif
        return sample;
    }

    // Avaliação independente do boot/fluxo; sem boot válido só publica por USB.
    void ServiceI2cHealth(
        uint64_t now_ms
    ) {
        static obc_i2c_health::Monitor monitor;
        static bool sampled = false;
        static uint64_t last_sample_ms = 0;
        static uint64_t last_print_ms = 0;
        if (sampled && now_ms - last_sample_ms < obc_i2c_health::evaluation_period_ms) return;
        sampled = true;
        last_sample_ms = now_ms;
        const auto diagnostics = i2c::GetDiagnostics();
        const obc_i2c_health::SAMPLE sample = {diagnostics.state, diagnostics.callback_running,
            diagnostics.callback_started_ms, diagnostics.recovery_started_ms, diagnostics.sampled_ms};
        obc_i2c_health::REPORT report = {};
        const bool changed = monitor.Evaluate(sample, report);
        if (changed || now_ms - last_print_ms >= 1000) {
            Serial.printf("[I2C-HEALTH] state=%s service=%s activity=%s age_ms=%lu limit_ms=%lu sampled_ms=%lu monotonic_ms=%llu changed=%u queued=%u cause=%u failure=%u cycles=%llu recovered=%llu exhausted=%llu\n",
                obc_i2c_health::StateName(report.state), obc_i2c_health::ServiceName(report.service),
                obc_i2c_health::ActivityName(report.activity), static_cast<unsigned long>(report.age_ms),
                static_cast<unsigned long>(report.limit_ms), static_cast<unsigned long>(report.sampled_ms),
                static_cast<unsigned long long>(now_ms), changed ? 1U : 0U,
                static_cast<unsigned>(diagnostics.queued_requests), static_cast<unsigned>(diagnostics.trigger),
                static_cast<unsigned>(diagnostics.last_failure), static_cast<unsigned long long>(diagnostics.recovery_cycles),
                static_cast<unsigned long long>(diagnostics.recovered), static_cast<unsigned long long>(diagnostics.exhausted));
            // Transições não adiam a amostra periódica de um segundo.
            if (now_ms - last_print_ms >= 1000) last_print_ms = now_ms;
        }
        if (changed && boot_id) {
            obc_record::RECORD event = {};
            if (obc_i2c_health::MakeEvent(report, {boot_id, obc_time::esp32::GetMonotonicMs()}, event)) SubmitRecord(event);
        }
    }

    // Uma escrita por linha conserva separação dos quadros JSON do umbilical.
    void PrintHealth(
        const obc_health::REPORT& report,
        bool changed
    ) {
        Serial.printf("[HEALTH] task=%s state=%s activity=%s age_ms=%llu limit_ms=%llu completions=%llu sampled_ms=%llu changed=%u\n",
            obc_health::TaskName(report.task), obc_health::StateName(report.state),
            obc_health::ActivityName(report.activity), static_cast<unsigned long long>(report.age_ms),
            static_cast<unsigned long long>(report.limit_ms), static_cast<unsigned long long>(report.completions),
            static_cast<unsigned long long>(report.sampled_ms), changed ? 1U : 0U);
    }

    // Cópias sequenciais, sem aninhar state_lock e transport_lock. O relógio
    // é lido após as amostras para não atribuir idade negativa à publicação.
    void ServiceTaskHealth(
        uint64_t now_ms
    ) {
        if (health_sampled && now_ms - last_health_ms < obc_health::evaluation_period_ms) return;
        health_sampled = true;
        last_health_ms = now_ms;
        obc_health::PROGRESS progress[obc_health::task_count] = {};
        portENTER_CRITICAL(&state_lock);
        progress[1] = process_progress;
        progress[2] = writer_progress;
        portEXIT_CRITICAL(&state_lock);
        progress[0] = obc_payload::esp32::GetTransportStatistics().progress;
        const uint64_t sampled_ms = obc_time::esp32::GetMonotonicMs();
        for (size_t index = 0; index < obc_health::task_count; ++index) {
            auto& report = health_reports[index];
            if (!health_monitor.Evaluate(static_cast<obc_health::TASK>(index), progress[index], sampled_ms, report)) continue;
            PrintHealth(report, true);
            obc_record::RECORD event = {};
            if (obc_health::MakeEvent(report, {boot_id, sampled_ms}, event)) SubmitRecord(event);
        }
    }

    // Reserva exclusiva do único bloco grande; não mantém lock ao codificar.
    void PublishSnapshot(
        const obc_data::PAYLOAD_SNAPSHOT& snapshot
    ) {
        portENTER_CRITICAL(&state_lock);
        const bool observed = state.ObserveSnapshot(snapshot);
#if !defined(OBC_FLOW_BENCH) || defined(OBC_DELIVERY_BENCH)
        if (observed) deliveries.ObserveValidatedSnapshot(snapshot);
#endif
        const bool reserved = observed && writer_ready && snapshot_state == SLOT_STATE::FREE;
        if (reserved) snapshot_state = SLOT_STATE::ENCODING;
        else if (!observed) DropRecord(RECORD_DROP_KIND::SNAPSHOT, RECORD_DROP_CAUSE::ADMISSION_REJECTED);
        else if (!writer_ready) DropRecord(RECORD_DROP_KIND::SNAPSHOT, RECORD_DROP_CAUSE::WRITER_UNAVAILABLE);
        else DropRecord(RECORD_DROP_KIND::SNAPSHOT, RECORD_DROP_CAUSE::SNAPSHOT_BUSY);
        portEXIT_CRITICAL(&state_lock);
        if (!reserved) return;

        MarkProcess(obc_health::ACTIVITY::ENCODING);
        size_t size = 0;
#ifdef OBC_HOUSEKEEPING_BENCH
        // O slot ENCODING pertence ao processador; nenhuma escrita usa estes blocos.
        size_t expected_size = 0;
        const bool prepared = obc_record::GetSnapshotSize(snapshot, expected_size) == obc_record::STATUS::OK
            && snapshot_buffers.Reserve(expected_size);
        const size_t first_capacity = expected_size < snapshot_first_bytes ? expected_size : snapshot_first_bytes;
        const size_t second_capacity = expected_size - first_capacity;
#else
        const bool prepared = true;
        const size_t first_capacity = snapshot_first_bytes;
        const size_t second_capacity = snapshot_second_bytes;
#endif
        const bool encoded = prepared && obc_record::EncodeSnapshotParts(snapshot,
            snapshot_buffers.GetFirstData(), first_capacity, snapshot_buffers.GetSecondData(),
            second_capacity, size) == obc_record::STATUS::OK;
#ifdef OBC_HOUSEKEEPING_BENCH
        if (!encoded) snapshot_buffers.Reset(); // Ainda ENCODING, antes de publicar FREE.
#endif
        portENTER_CRITICAL(&state_lock);
        if (encoded) {
            snapshot_size = size;
            snapshot_state = SLOT_STATE::READY;
        } else {
            snapshot_state = SLOT_STATE::FREE;
            DropRecord(RECORD_DROP_KIND::SNAPSHOT, RECORD_DROP_CAUSE::ENCODE_FAILED);
        }
        portEXIT_CRITICAL(&state_lock);
        if (encoded) xTaskNotifyGive(writer_task);
    }

    // Somente este contexto acessa o parser/modelo; UART possui tarefa própria.
    void PayloadEntry(
        void*
    ) {
        uint64_t previous_ms = obc_time::esp32::GetMonotonicMs();
        for (;;) {
            const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
            MarkProcess(obc_health::ACTIVITY::PROCESSING);
#ifdef OBC_TASK_HEALTH_BENCH
            obc_health::bench::Before(obc_health::TASK::PROCESS, obc_health::ACTIVITY::PROCESSING);
#endif
#ifdef OBC_WATCHDOG_BENCH_CASE
            obc_watchdog_bench::Before(obc_watchdog::RESOURCE::PROCESS);
#endif
            const obc_payload::STATUS status = obc_payload::esp32::Poll();
            if (status == obc_payload::STATUS::READY) PublishSnapshot(*receiver.GetSnapshot());
#if defined(OBC_CAPACITY_BENCH) || defined(OBC_MEMORY_BENCH)
            if (status != obc_payload::STATUS::WAITING) {
                portENTER_CRITICAL(&state_lock);
                const auto delivery_statistics = deliveries.GetStatistics();
                portEXIT_CRITICAL(&state_lock);
                ObserveLimitsPayload(status, receiver.GetSnapshot(), delivery_statistics);
            }
#endif
            const obc_payload::STATISTICS& observed = receiver.GetStatistics();
            portENTER_CRITICAL(&state_lock);
            if (status == obc_payload::STATUS::NO_MEMORY && allocation_failures != UINT64_MAX) ++allocation_failures;
            state.SetPayloadStatistics(observed.complete_candidates, observed.rejected,
                observed.missing_positions, observed.duplicates);
            parser_peak_bytes = observed.parser_peak_bytes;
            const uint64_t gap = now_ms - previous_ms;
            if (gap > maximum_poll_gap_ms) maximum_poll_gap_ms = gap;
            portEXIT_CRITICAL(&state_lock);
            previous_ms = now_ms;
            MarkProcess(obc_health::ACTIVITY::IDLE, true);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // Publica apenas resultados pequenos; nunca compartilha o Journal em uso.
    void UpdateWriterState(
    ) {
        const auto recovery = storage_recovery.GetDiagnostics();
        portENTER_CRITICAL(&state_lock);
        writer_ready = journal.IsReady() && !synchronizing_reference;
        writer_result = journal.GetLastResult();
        if (!journal.IsReady() && recovery.total_attempts) writer_result = recovery.last_result;
        recovery_diagnostic = recovery;
        uint64_t failures = journal.GetFailures();
        const uint64_t maximum = std::numeric_limits<uint64_t>::max();
        failures = maximum - failures < startup_storage_failures ? maximum : failures + startup_storage_failures;
        failures = maximum - failures < recovery.lifecycle_failures ? maximum : failures + recovery.lifecycle_failures;
        state.SetStorageStatistics(journal.GetWritten(), failures);
        portEXIT_CRITICAL(&state_lock);
    }

    // Compara os campos, sem depender de padding da representação em RAM.
    bool SameReference(
        const obc_time::TIME_REFERENCE& left,
        const obc_time::TIME_REFERENCE& right
    ) {
        return left.reception.boot_id == right.reception.boot_id
            && left.reception.monotonic_ms == right.reception.monotonic_ms
            && left.unix_s == right.unix_s && left.delay_ms == right.delay_ms;
    }

    // Revalida a referência após o I/O antes de liberar a admissão de dados.
    // SetTime concorrente conserva a nova referência em State mesmo sem fila SD.
    void SynchronizeRecoveredReference(
    ) {
        if (!synchronizing_reference || !journal.IsReady()) return;
        obc_time::TIME_REFERENCE reference = {};
        portENTER_CRITICAL(&state_lock);
        const bool known = state.GetTimeReference(reference);
        portEXIT_CRITICAL(&state_lock);
        if (known && !journal.WriteReference(reference)) return;
        obc_time::TIME_REFERENCE current = {};
        portENTER_CRITICAL(&state_lock);
        const bool current_known = state.GetTimeReference(current);
        if (known == current_known && (!known || SameReference(reference, current))) {
            synchronizing_reference = false;
        }
        portEXIT_CRITICAL(&state_lock);
    }

    // Não confirma recuperação por gravar o seu próprio evento de diagnóstico.
    void RecordStorageRecovery(
        const char* description
    ) {
        if (!journal.IsReady()) return;
        obc_record::RECORD record = {};
        record.header = {1, obc_data::RECORD_TYPE::EVENT, 96,
            {boot_id, obc_time::esp32::GetMonotonicMs()}};
        record.body.event = {};
        record.body.event.source = obc_data::SOURCE::SD;
        record.body.event.code = 2;
        record.body.event.severity = obc_data::SEVERITY::INFO;
        std::strcpy(record.body.event.description, description);
        uint8_t bytes[96];
        size_t size = 0;
        if (obc_record::Encode(record, bytes, sizeof(bytes), size) == obc_record::STATUS::OK) {
            if (!journal.Write(bytes, size)) {
                portENTER_CRITICAL(&state_lock);
                DropRecord(RECORD_DROP_KIND::OTHER, RECORD_DROP_CAUSE::WRITE_FAILED);
                portEXIT_CRITICAL(&state_lock);
            }
        }
    }

    // Um ciclo por chamada; o prazo é consultado sem suspender a tarefa por 5 s.
    void ServiceStorageRecovery(
        const RECOVERY_IO& io,
        const obc_delivery::READER_IO& reader
    ) {
        const auto before = storage_recovery.GetDiagnostics();
        obc_time::TIME_REFERENCE reference = {};
        portENTER_CRITICAL(&state_lock);
        const bool known = state.GetTimeReference(reference);
        portEXIT_CRITICAL(&state_lock);
        const bool resumed = storage_recovery.Service(io, journal,
            obc_time::esp32::GetMonotonicMs(), known ? &reference : nullptr);
        const auto after = storage_recovery.GetDiagnostics();
        if (after.total_attempts != before.total_attempts) delivery_reader_ready = false;
        if (resumed) {
            synchronizing_reference = true;
            obc_delivery::RECOVERY audit = {};
            const auto previous = BeginWriterActivity(obc_health::ACTIVITY::SCAN);
            delivery_reader_ready = obc_delivery::Audit(reader, audit);
            EndWriterActivity(previous);
            portENTER_CRITICAL(&state_lock);
            deliveries.RecheckStorage();
            portEXIT_CRITICAL(&state_lock);
        }
        SynchronizeRecoveredReference();
        UpdateWriterState();
        if (resumed && !synchronizing_reference) RecordStorageRecovery("storage remounted; awaiting normal write");
    }

    // Um registro pequeno e um snapshot por rodada evitam monopolizar a fila.
    void WriterEntry(
        void*
    ) {
        const obc_time::EVENT_STAMP stamp = {boot_id, obc_time::esp32::GetMonotonicMs()};
#ifdef OBC_FLOW_BENCH
        const JOURNAL_IO raw_io = GetBenchIO();
        const RECOVERY_IO raw_recovery = GetBenchRecoveryIO();
#else
        const JOURNAL_IO raw_io = {Open, Append, Close, nullptr};
        const RECOVERY_IO raw_recovery = {EndStorage, InitStorage, nullptr};
#endif
        // Os mesmos callbacks atendem recuperação e extração, sempre neste contexto.
#if defined(OBC_FLOW_BENCH) && !defined(OBC_DELIVERY_BENCH)
        const obc_delivery::READER_IO raw_reader = {};
#elif defined(OBC_DELIVERY_BENCH)
        const obc_delivery::READER_IO raw_reader = GetBenchReaderIO();
#else
        const obc_delivery::READER_IO raw_reader = {ListDelivery, ReadDelivery, nullptr};
#endif
        WRITER_IO_CONTEXT io_context = {raw_io, raw_reader, raw_recovery};
        const JOURNAL_IO io = {HealthOpen, HealthAppend, HealthClose, &io_context};
        const obc_delivery::READER_IO reader = raw_reader.list && raw_reader.read
            ? obc_delivery::READER_IO{HealthList, HealthRead, &io_context} : obc_delivery::READER_IO{};
        const RECOVERY_IO recovery_io = raw_recovery.end && raw_recovery.init
            ? RECOVERY_IO{HealthEnd, HealthInit, &io_context} : RECOVERY_IO{};
        journal.Init(io, stamp, static_cast<uint32_t>(esp_reset_reason()));
        UpdateWriterState();
#if !defined(OBC_FLOW_BENCH) || defined(OBC_DELIVERY_BENCH)
        obc_delivery::RECOVERY recovery = {};
        const auto previous = BeginWriterActivity(obc_health::ACTIVITY::SCAN);
        delivery_reader_ready = obc_delivery::Audit(reader, recovery);
        EndWriterActivity(previous);
        Serial.printf("[DELIVERY] recovery=%u data=%llu confirm=%llu corrupt=%llu tails=%llu orphan=%llu cache_bytes=%u\n",
            delivery_reader_ready ? 1U : 0U, static_cast<unsigned long long>(recovery.data),
            static_cast<unsigned long long>(recovery.confirmations), static_cast<unsigned long long>(recovery.corrupt_bytes),
            static_cast<unsigned long long>(recovery.incomplete_tails), static_cast<unsigned long long>(recovery.orphan_confirmations),
            static_cast<unsigned>(sizeof(deliveries)));
#endif
        if (journal.IsReady()) {
            obc_record::RECORD started = {};
            started.header = {1, obc_data::RECORD_TYPE::EVENT, 96, stamp};
            started.body.event = {};
            started.body.event.source = obc_data::SOURCE::OBC;
            started.body.event.code = 1;
            started.body.event.severity = obc_data::SEVERITY::INFO;
            std::strcpy(started.body.event.description, "data_flow_started");
            SubmitRecord(started);
        }
        RECORD_BLOCK block = {};
        for (;;) {
#ifdef OBC_TASK_HEALTH_BENCH
            if (HealthBenchScanRequested()) {
                const auto scan_previous = BeginWriterActivity(obc_health::ACTIVITY::SCAN);
                RunHealthBenchScan(reader);
                EndWriterActivity(scan_previous);
            }
#endif
            ServiceStorageRecovery(recovery_io, reader);
            const auto recovered_before = storage_recovery.GetDiagnostics().recovered;
            uint16_t log_sequence = 0;
            ttc::LOG_REQUEST log_request = {};
            portENTER_CRITICAL(&state_lock);
            const bool log_work = log_mailbox.Begin(log_sequence, log_request);
            portEXIT_CRITICAL(&state_lock);
            if (log_work) {
                obc_downlink::LOG_RESPONSE response = {};
                obc_downlink::BuildLogResponse(log_sequence, log_request, reader, response);
                portENTER_CRITICAL(&state_lock);
                log_mailbox.Complete(response);
                portEXIT_CRITICAL(&state_lock);
            }
#if !defined(OBC_FLOW_BENCH) || defined(OBC_DELIVERY_BENCH)
            // Somente este contexto grava DATA/CONFIRM e lê segmentos fechados.
            obc_delivery::RECORD delivery_record = {};
            portENTER_CRITICAL(&state_lock);
            const bool delivery_work = deliveries.BeginStorage(delivery_record);
            portEXIT_CRITICAL(&state_lock);
            if (delivery_work) {
                uint8_t bytes[obc_delivery::data_bytes];
                size_t count = 0;
                const bool written = !synchronizing_reference
                    && obc_delivery::Encode(delivery_record, bytes, sizeof(bytes), count)
                    && journal.Write(bytes, count) && journal.Seal();
                portENTER_CRITICAL(&state_lock);
                deliveries.FinishStorage(delivery_record, written);
                portEXIT_CRITICAL(&state_lock);
                storage_recovery.ObserveWrite(written);
            }
            // A consulta usa a fronteira capturada e só publica se ela continuar atual.
            portENTER_CRITICAL(&state_lock);
            const auto frontier = deliveries.GetFrontier();
            const bool needs_recovery = deliveries.NeedsRecovery();
            portEXIT_CRITICAL(&state_lock);
            obc_delivery::DELIVERY recovered = {};
            bool found = false;
            if (needs_recovery) {
                const auto scan_previous = BeginWriterActivity(obc_health::ACTIVITY::SCAN);
                const bool read = delivery_reader_ready
                    && obc_delivery::FindPending(reader, frontier, recovered, found);
                EndWriterActivity(scan_previous);
                portENTER_CRITICAL(&state_lock);
                if (read) deliveries.Restore(frontier, found ? &recovered : nullptr);
                else deliveries.ReaderFailed();
                portEXIT_CRITICAL(&state_lock);
                if (!read) delivery_reader_ready = false;
            }
#endif
            portENTER_CRITICAL(&state_lock);
            const bool small = records.Pop(block);
            portEXIT_CRITICAL(&state_lock);
            if (small) {
                const bool written = !synchronizing_reference && journal.Write(block.bytes, block.size);
                if (!written) {
                    portENTER_CRITICAL(&state_lock);
                    DropRecord(EncodedRecordDropKind(block.bytes, block.size), synchronizing_reference
                        ? RECORD_DROP_CAUSE::TIME_SYNC : RECORD_DROP_CAUSE::WRITE_FAILED);
                    portEXIT_CRITICAL(&state_lock);
                }
                const bool reference = block.bytes[5] == static_cast<uint8_t>(obc_data::RECORD_TYPE::TIME_REFERENCE)
                    && std::memcmp(block.bytes, obc_data::record_magic, 4) == 0;
                if (!reference) storage_recovery.ObserveWrite(written);
            }
            portENTER_CRITICAL(&state_lock);
            const bool snapshot = snapshot_state == SLOT_STATE::READY;
            if (snapshot) snapshot_state = SLOT_STATE::WRITING;
            const size_t size = snapshot_size;
            portEXIT_CRITICAL(&state_lock);
            if (snapshot) {
                const size_t first_size = size < snapshot_first_bytes ? size : snapshot_first_bytes;
                const bool written = !synchronizing_reference && journal.WriteParts(snapshot_buffers.GetFirstData(), first_size,
                    snapshot_buffers.GetSecondData(), size - first_size);
#ifdef OBC_HOUSEKEEPING_BENCH
                // I/O terminou; liberar ainda em WRITING impede realocação pelo processador.
                snapshot_buffers.Reset();
#endif
                portENTER_CRITICAL(&state_lock);
                snapshot_state = SLOT_STATE::FREE;
                if (!written) DropRecord(RECORD_DROP_KIND::SNAPSHOT, synchronizing_reference
                    ? RECORD_DROP_CAUSE::TIME_SYNC : RECORD_DROP_CAUSE::WRITE_FAILED);
                portEXIT_CRITICAL(&state_lock);
                storage_recovery.ObserveWrite(written);
            }
            if (storage_recovery.GetDiagnostics().recovered != recovered_before) {
                RecordStorageRecovery("storage recovered; normal write confirmed");
            }
            UpdateWriterState();
#ifdef OBC_TASK_HEALTH_BENCH
            ObserveHealthBenchRecovery(storage_recovery.GetDiagnostics().recovered);
#endif
            const auto recovery_state = storage_recovery.GetDiagnostics().state;
            const auto resting = recovery_state == RECOVERY_STATE::WAITING || recovery_state == RECOVERY_STATE::RETRY
                ? obc_health::ACTIVITY::RECOVERY_WAIT : obc_health::ACTIVITY::IDLE;
            EndWriterActivity(resting);
            // Uma notificação pode representar várias entradas; não dormir com fila pendente.
            if (small || snapshot) taskYIELD();
            else ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
        }
    }

} // namespace

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

#ifdef OBC_HOUSEKEEPING_BENCH
bool ReserveHousekeepingBuffers(
    uint64_t boot
) {
    if (attempted || boot == 0) return false;
    const bool reserved = receiver.Init(boot);
    char line[180];
    const int length = std::snprintf(line, sizeof(line),
        "[HK-RESERVE] receiver=%u staging=on_demand byte_heap=%u byte_largest=%u\n", reserved ? 1U : 0U,
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    if (length > 0 && static_cast<size_t>(length) < sizeof(line)) {
        Serial.write(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(length));
    }
    return reserved;
}
#endif

bool Init(
    const obc_startup::STARTUP_REPORT& report
) {
    if (attempted) return active;
    attempted = true;
    boot_id = report.time_status == obc_time::STATUS::OK ? report.boot_id : 0;
    state.Init(boot_id);
    deliveries.Init(boot_id, report.storage_result.status == obc_storage::STATUS::OK);
    if (boot_id == 0 || !obc_payload::esp32::Init(receiver, boot_id)) {
        Serial.printf("[FLOW] Recepcao inativa: boot ou memoria indisponivel\n");
        return false;
    }
    bool storage = report.storage_result.status == obc_storage::STATUS::OK;
    Serial.printf("[FLOW] RAM apos receptor: heap=%u byte_heap=%u byte_largest=%u\n",
        ESP.getFreeHeap(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (storage) {
#ifndef OBC_HOUSEKEEPING_BENCH
        const bool snapshot_memory_ready = snapshot_buffers.Reserve(obc_record::max_snapshot_record_bytes);
#else
        const bool snapshot_memory_ready = true; // Reserva por registro, fora do boot.
        Serial.write("[HK-STAGING] allocation=on_demand max_bytes=64497\n");
#endif
        // Admissão das tarefas ainda não criadas, em memória acessível por byte.
        // O parser mantém seu teto por quadro e NO_MEMORY; não reserva 96 KiB.
        const size_t stack_budget = payload_stack_bytes + writer_stack_bytes
            + obc_payload::esp32::transport_stack_bytes
#ifdef OBC_HOUSEKEEPING_BENCH
            + obc_housekeeping::bench::pending_stack_bytes
#endif
            ;
        const size_t reserve = stack_budget + startup_margin_bytes;
        const size_t flow_stack = payload_stack_bytes > writer_stack_bytes
            ? payload_stack_bytes : writer_stack_bytes;
        const size_t largest_stack = flow_stack > obc_payload::esp32::transport_stack_bytes
            ? flow_stack : obc_payload::esp32::transport_stack_bytes;
        const size_t byte_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        const size_t byte_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        Serial.printf("[FLOW] RAM apos bloco SD: bloco=%u heap=%u byte_heap=%u byte_largest=%u stack_budget=%u margin=%u reserva=%u parser_limit=%u\n",
            snapshot_buffers.GetFirstData() && snapshot_buffers.GetSecondData() ? 1U : 0U, ESP.getFreeHeap(),
            static_cast<unsigned>(byte_heap), static_cast<unsigned>(byte_largest),
            static_cast<unsigned>(stack_budget), static_cast<unsigned>(startup_margin_bytes),
            static_cast<unsigned>(reserve), static_cast<unsigned>(obc_payload::parser_memory_bytes));
        // Fragmentação pode impedir uma pilha mesmo quando a soma cabe.
        // A criação real das tarefas abaixo continua sendo verificada.
        if (!snapshot_memory_ready || byte_heap < reserve || byte_largest < largest_stack) {
            storage = false;
            snapshot_buffers.Reset();
            Serial.printf("[FLOW] Escritor inativo: memoria por byte insuficiente para tarefas e margem\n");
        }
    }
    startup_storage_failures = storage ? 0 : 1;
    writer_result = report.storage_result;
    if (!storage) DropRecord(RECORD_DROP_KIND::OTHER, RECORD_DROP_CAUSE::WRITER_UNAVAILABLE);
    state.SetStorageStatistics(0, startup_storage_failures);
#ifdef OBC_HOUSEKEEPING_BENCH
    obc_housekeeping::bench::TraceTaskCreation("uart", false,
        obc_payload::esp32::transport_stack_bytes, 0);
#endif
    const bool transport_started = obc_payload::esp32::StartTransportTask();
#ifdef OBC_HOUSEKEEPING_BENCH
    obc_housekeeping::bench::TraceTaskCreation("uart", true,
        obc_payload::esp32::transport_stack_bytes, transport_started ? pdPASS : pdFAIL);
#endif
    if (transport_started) {
        portENTER_CRITICAL(&state_lock);
        obc_health::Start(process_progress, obc_time::esp32::GetMonotonicMs());
        portEXIT_CRITICAL(&state_lock);
#ifdef OBC_HOUSEKEEPING_BENCH
        obc_housekeeping::bench::TraceTaskCreation("process", false, payload_stack_bytes, 0);
#endif
        const BaseType_t process_create_result = xTaskCreatePinnedToCore(PayloadEntry, "payload_rx", payload_stack_bytes,
            nullptr, 2, &payload_task, 1);
        active = process_create_result == pdPASS;
#ifdef OBC_HOUSEKEEPING_BENCH
        obc_housekeeping::bench::TraceTaskCreation("process", true, payload_stack_bytes, process_create_result);
#endif
        if (!active) {
            portENTER_CRITICAL(&state_lock);
            process_progress = {};
            portEXIT_CRITICAL(&state_lock);
        }
    }
    if (active && storage) {
        portENTER_CRITICAL(&state_lock);
        obc_health::Start(writer_progress, obc_time::esp32::GetMonotonicMs());
        portEXIT_CRITICAL(&state_lock);
#ifdef OBC_HOUSEKEEPING_BENCH
        obc_housekeeping::bench::TraceTaskCreation("writer", false, writer_stack_bytes, 0);
#endif
        const BaseType_t writer_create_result = xTaskCreatePinnedToCore(WriterEntry, "obc_journal", writer_stack_bytes,
            nullptr, 1, &writer_task, 1);
#ifdef OBC_HOUSEKEEPING_BENCH
        obc_housekeeping::bench::TraceTaskCreation("writer", true, writer_stack_bytes, writer_create_result);
#endif
        if (writer_create_result != pdPASS) {
            snapshot_buffers.Reset();
            startup_storage_failures = 1;
            portENTER_CRITICAL(&state_lock);
            writer_progress = {};
            state.SetStorageStatistics(0, 1);
            portEXIT_CRITICAL(&state_lock);
            Serial.printf("[FLOW] Escritor inativo: falha ao criar tarefa\n");
        }
    }
    if (!writer_task) {
        portENTER_CRITICAL(&state_lock);
        deliveries.ReaderFailed();
        portEXIT_CRITICAL(&state_lock);
    }
    Serial.printf("[FLOW] Recepcao=%s; SD=%s; heap=%u; maior_bloco=%u\n",
        active ? "OK" : "inativa", storage && writer_task ? "em inicializacao" : "indisponivel",
        ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    return active;
}

bool SetTime(
    uint32_t unix_s,
    uint32_t rx_ms,
    uint32_t delay_ms
) {
    obc_time::TIME_REFERENCE reference = {};
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    portENTER_CRITICAL(&state_lock);
    const bool applied = active && state.SetTime(unix_s, now_ms, rx_ms, delay_ms, reference);
    portEXIT_CRITICAL(&state_lock);
    if (!applied) return false;
    obc_record::RECORD record = {};
    record.header = {1, obc_data::RECORD_TYPE::TIME_REFERENCE, 36, reference.reception};
    record.body.time_reference = reference;
    SubmitRecord(record);
#ifdef OBC_FLOW_BENCH
    obc_payload::esp32::StartBenchInput();
#endif
    return true;
}

void GetTaskProgress(
    obc_health::PROGRESS (&out)[obc_health::task_count]
) {
    portENTER_CRITICAL(&state_lock);
    out[1] = process_progress;
    out[2] = writer_progress;
    portEXIT_CRITICAL(&state_lock);
    out[0] = obc_payload::esp32::GetTransportStatistics().progress;
}

void RecordWatchdogEvent(
    obc_watchdog::CODE code,
    bool fault,
    const char* description
) {
    if (!boot_id) return;
    obc_record::RECORD record = {};
    record.header = {1, fault ? obc_data::RECORD_TYPE::FAULT : obc_data::RECORD_TYPE::EVENT,
        96, {boot_id, obc_time::esp32::GetMonotonicMs()}};
    record.body.event.source = obc_data::SOURCE::OBC;
    record.body.event.code = static_cast<uint16_t>(code);
    record.body.event.severity = fault ? obc_data::SEVERITY::ERROR : obc_data::SEVERITY::INFO;
    std::snprintf(record.body.event.description, sizeof(record.body.event.description), "%s", description);
    SubmitRecord(record);
}

obc_data::TELEMETRY_DATA ComposeTelemetry(
    uint64_t& sampled_ms
) {
    sampled_ms = obc_time::esp32::GetMonotonicMs();
    portENTER_CRITICAL(&state_lock);
    obc_data::TELEMETRY_DATA telemetry = state.ComposeTelemetry(sampled_ms);
    portEXIT_CRITICAL(&state_lock);
#ifdef OBC_HOUSEKEEPING_BENCH
    // Getters e conversão fora do lock; o mesmo objeto segue para TT&C e TM v3.
    const auto readings = obc_housekeeping::bench::Read(sampled_ms);
    obc_housekeeping::ApplyTelemetry(readings, telemetry);
#endif
    return telemetry;
}

void RecordTelemetry(
    const obc_data::TELEMETRY_DATA& telemetry,
    uint64_t sampled_ms
) {
    const obc_time::EVENT_STAMP stamp = {boot_id, sampled_ms};
    uint8_t bytes[obc_record::telemetry_record_bytes];
    size_t size = 0;
    if (obc_record::EncodeTelemetryV3(stamp, telemetry, bytes, sizeof(bytes), size) == obc_record::STATUS::OK) {
        Submit(bytes, size);
    } else {
        portENTER_CRITICAL(&state_lock);
        DropRecord(RECORD_DROP_KIND::TELEMETRY, RECORD_DROP_CAUSE::ENCODE_FAILED);
        portEXIT_CRITICAL(&state_lock);
    }
}

void RecordAcquisition(
    obc_record::ACQUISITION_KIND kind,
    uint64_t completed_ms,
    const obc_record::ACQUISITION_DATA& data
) {
    uint8_t bytes[obc_flow::small_record_bytes];
    size_t size = 0;
    if (obc_record::EncodeAcquisition(kind, {boot_id, completed_ms}, data,
            bytes, sizeof(bytes), size) == obc_record::STATUS::OK) {
        Submit(bytes, size);
    } else {
        portENTER_CRITICAL(&state_lock);
        DropRecord(RECORD_DROP_KIND::OTHER, RECORD_DROP_CAUSE::ENCODE_FAILED);
        portEXIT_CRITICAL(&state_lock);
    }
}

void ReportResourceHealth(
    const LINK_DIAGNOSTICS& ttc
) {
    using namespace obc_resource_health;
    static TtcMonitor ttc_monitor;
    static MemoryMonitor memory_monitor;
    static bool memory_sampled = false;
    static uint64_t last_memory_ms = 0;
    static uint64_t last_ttc_print_ms = 0;
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    const TTC_SAMPLE ttc_sample = {ttc.init_attempted, ttc.initialized, ttc.task_started,
        ttc.radio_ready, ttc.progress_ms, ttc.sampled_ms, ttc.completions};
    const TTC_REPORT report = ttc_monitor.Evaluate(ttc_sample);
    if (report.progress_changed || report.radio_changed || now_ms - last_ttc_print_ms >= 1000) {
        Serial.printf("[TTC-HEALTH] state=%s radio=%s updating=%u wired_ready=%u age_ms=%lu limit_ms=%lu completions=%llu rf_seen=%u rf_age_ms=%lu wired_seen=%u wired_age_ms=%lu resets=%lu changed=%u monotonic_ms=%llu\n",
            StateName(report.progress), StateName(report.radio), ttc.updating ? 1U : 0U, ttc.wired_ready ? 1U : 0U,
            static_cast<unsigned long>(report.age_ms), static_cast<unsigned long>(ttc_limit_ms),
            static_cast<unsigned long long>(ttc.completions), ttc.rf_contact.seen ? 1U : 0U,
            static_cast<unsigned long>(ttc.rf_contact.seen ? ttc.sampled_ms - ttc.rf_contact.received_ms : 0),
            ttc.wired_contact.seen ? 1U : 0U,
            static_cast<unsigned long>(ttc.wired_contact.seen ? ttc.sampled_ms - ttc.wired_contact.received_ms : 0),
            static_cast<unsigned long>(ttc.radio_resets), report.progress_changed || report.radio_changed ? 1U : 0U,
            static_cast<unsigned long long>(now_ms));
        if (now_ms - last_ttc_print_ms >= 1000) last_ttc_print_ms = now_ms;
    }
    char description[65];
    if (report.progress_changed && report.progress != STATE::INACTIVE) {
        std::snprintf(description, sizeof(description), "ttc %s age=%lu", StateName(report.progress),
            static_cast<unsigned long>(report.age_ms));
        RecordResourceEvent(CODE::TTC_PROGRESS, report.progress == STATE::SUSPECT, description, now_ms);
    }
    if (report.radio_changed && report.radio != STATE::INACTIVE) {
        std::snprintf(description, sizeof(description), "radio %s resets=%lu", StateName(report.radio),
            static_cast<unsigned long>(ttc.radio_resets));
        RecordResourceEvent(CODE::RADIO, report.radio == STATE::UNAVAILABLE, description, now_ms);
    }
    if (memory_sampled && now_ms - last_memory_ms < memory_period_ms) return;
    memory_sampled = true;
    last_memory_ms = now_ms;
    const MEMORY_SAMPLE sample = SampleMemory(ttc);
    const MEMORY_REPORT memory = memory_monitor.Evaluate(sample);
    Serial.printf("[MEMORY-HEALTH] byte_heap=%u byte_largest=%u heap_low=%u block_low=%u stack_low_mask=%u allocation_failures=%llu allocation_delta=%llu changed=%u monotonic_ms=%llu\n",
        static_cast<unsigned>(sample.byte_heap), static_cast<unsigned>(sample.byte_largest),
        memory.heap_low ? 1U : 0U, memory.block_low ? 1U : 0U, memory.stack_low_mask,
        static_cast<unsigned long long>(sample.allocation_failures), static_cast<unsigned long long>(memory.allocation_delta),
        memory.pressure_changed || memory.stack_new_mask || memory.allocation_delta ? 1U : 0U,
        static_cast<unsigned long long>(now_ms));
    if (memory.pressure_changed) {
        std::snprintf(description, sizeof(description), "heap low=%u block_low=%u free=%u block=%u",
            memory.heap_low ? 1U : 0U, memory.block_low ? 1U : 0U,
            static_cast<unsigned>(sample.byte_heap), static_cast<unsigned>(sample.byte_largest));
        RecordResourceEvent(CODE::HEAP, memory.heap_low || memory.block_low, description, now_ms);
    }
    for (size_t index = 0; index < stack_count; ++index) {
        const auto& stack = sample.stacks[index];
        Serial.printf("[MEMORY-HEALTH] task=%s active=%u stack_free=%lu low=%u monotonic_ms=%llu\n",
            StackName(static_cast<STACK>(index)), stack.active ? 1U : 0U, static_cast<unsigned long>(stack.free_bytes),
            (memory.stack_low_mask & (1U << index)) ? 1U : 0U, static_cast<unsigned long long>(now_ms));
        if (!(memory.stack_new_mask & (1U << index))) continue;
        std::snprintf(description, sizeof(description), "stack %s free=%lu limit=1024",
            StackName(static_cast<STACK>(index)), static_cast<unsigned long>(stack.free_bytes));
        RecordResourceEvent(CODE::STACK_MARGIN, true, description, now_ms);
    }
    if (memory.allocation_delta) {
        std::snprintf(description, sizeof(description), "NO_MEMORY delta=%llu total=%llu",
            static_cast<unsigned long long>(memory.allocation_delta), static_cast<unsigned long long>(sample.allocation_failures));
        RecordResourceEvent(CODE::ALLOCATION, true, description, now_ms);
    }
}

void ReportDiagnostics(
) {
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    ServiceI2cHealth(now_ms);
    if (boot_id == 0) return;
    ServiceTaskHealth(now_ms);
    if (now_ms - last_diagnostic_ms < 1000) return;
    last_diagnostic_ms = now_ms;
    portENTER_CRITICAL(&state_lock);
    const obc_data::STATISTICS_DATA statistics = state.GetStatistics();
#ifdef OBC_RECORD_DROPS_BENCH
    const auto drops = record_drops;
#endif
    const bool storage = writer_ready;
    const obc_storage::RESULT result = writer_result;
    const uint64_t poll_gap = maximum_poll_gap_ms;
    const size_t parser_peak = parser_peak_bytes;
    const auto recovery = recovery_diagnostic;
    portEXIT_CRITICAL(&state_lock);
    const auto transport = obc_payload::esp32::GetTransportStatistics();
    obc_record::RECORD record = {};
    record.header = {1, obc_data::RECORD_TYPE::STATISTICS, 84, {boot_id, now_ms}};
    record.body.statistics = statistics;
    SubmitRecord(record);

    static obc_data::STATISTICS_DATA previous = {};
    if (statistics.invalid != previous.invalid || statistics.storage_failures != previous.storage_failures
            || statistics.records_dropped != previous.records_dropped) {
        record.header.type = obc_data::RECORD_TYPE::FAULT;
        record.header.total_bytes = 96;
        record.body.event = {};
        record.body.event.source = statistics.invalid != previous.invalid
            ? obc_data::SOURCE::PAYLOAD : obc_data::SOURCE::SD;
        record.body.event.code = 1;
        record.body.event.severity = obc_data::SEVERITY::ERROR;
        std::strcpy(record.body.event.description, "flow failure; cumulative counters in statistics");
        SubmitRecord(record);
    }
    Serial.printf("[FLOW] received=%llu invalid=%llu gaps=%llu duplicates=%llu written=%llu failures=%llu dropped=%llu SD=%u status=%u poll_gap_ms=%llu process_gap_ms=%llu stack_rx=%u stack_sd=%u stack_uart=%u uart_full=%llu\n",
        static_cast<unsigned long long>(statistics.received), static_cast<unsigned long long>(statistics.invalid),
        static_cast<unsigned long long>(statistics.gaps), static_cast<unsigned long long>(statistics.duplicates),
        static_cast<unsigned long long>(statistics.records_written), static_cast<unsigned long long>(statistics.storage_failures),
        static_cast<unsigned long long>(statistics.records_dropped), storage ? 1U : 0U,
        static_cast<unsigned>(result.status), static_cast<unsigned long long>(transport.maximum_gap_ms),
        static_cast<unsigned long long>(poll_gap),
        payload_task ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(payload_task)) : 0U,
        writer_task ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(writer_task)) : 0U,
        static_cast<unsigned>(transport.stack_free_bytes), static_cast<unsigned long long>(transport.full_observations));
#ifdef OBC_RECORD_DROPS_BENCH
    PrintRecordDrops(drops, now_ms, statistics.records_dropped);
#endif
    Serial.printf("[FLOW-MEMORY] heap=%u byte_heap=%u byte_largest=%u byte_min=%u parser_peak_bytes=%u parser_limit_bytes=%u\n",
        ESP.getFreeHeap(),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(parser_peak), static_cast<unsigned>(obc_payload::parser_memory_bytes));
    Serial.printf("[STORAGE] state=%u attempt=%u episodes=%llu attempts=%llu recovered=%llu exhausted=%llu lifecycle_failures=%llu cause=%u end=%u status=%u monotonic_ms=%llu\n",
        static_cast<unsigned>(recovery.state), recovery.attempts,
        static_cast<unsigned long long>(recovery.episodes), static_cast<unsigned long long>(recovery.total_attempts),
        static_cast<unsigned long long>(recovery.recovered), static_cast<unsigned long long>(recovery.exhausted),
        static_cast<unsigned long long>(recovery.lifecycle_failures), static_cast<unsigned>(recovery.cause.status),
        static_cast<unsigned>(recovery.last_end.status), static_cast<unsigned>(recovery.last_result.status),
        static_cast<unsigned long long>(now_ms));
    for (const auto& report : health_reports) PrintHealth(report, false);
    previous = statistics;
#if !defined(OBC_FLOW_BENCH) || defined(OBC_DELIVERY_BENCH)
    portENTER_CRITICAL(&state_lock);
    const auto delivery_statistics = deliveries.GetStatistics();
    portEXIT_CRITICAL(&state_lock);
#if defined(OBC_CAPACITY_BENCH) || defined(OBC_MEMORY_BENCH)
    ObserveLimitsDeliveries(delivery_statistics);
#endif
    Serial.printf("[DELIVERY] occupied=%u peak=%u accepted=%llu repeated=%llu lost=%llu storage_failures=%llu attempts=%llu tx_failures=%llu confirmed=%llu unknown=%llu evicted=%llu recovery_slot=%u\n",
        static_cast<unsigned>(delivery_statistics.occupied), static_cast<unsigned>(delivery_statistics.high_water),
        static_cast<unsigned long long>(delivery_statistics.accepted), static_cast<unsigned long long>(delivery_statistics.repeated),
        static_cast<unsigned long long>(delivery_statistics.lost), static_cast<unsigned long long>(delivery_statistics.storage_failures),
        static_cast<unsigned long long>(delivery_statistics.attempts), static_cast<unsigned long long>(delivery_statistics.transmit_failures),
        static_cast<unsigned long long>(delivery_statistics.confirmed), static_cast<unsigned long long>(delivery_statistics.unknown_confirmations),
        static_cast<unsigned long long>(delivery_statistics.evicted),
        delivery_statistics.recovery_occupied ? 1U : 0U);
#endif
}

bool BeginDeliveryTransmit(
    uint64_t timeout_ms,
    obc_delivery::DELIVERY& out
) {
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    portENTER_CRITICAL(&state_lock);
    const bool result = active && deliveries.BeginTransmit(now_ms, timeout_ms, out);
    portEXIT_CRITICAL(&state_lock);
    return result;
}

bool FinishDeliveryTransmit(
    obc_delivery::ID id,
    bool success
) {
    portENTER_CRITICAL(&state_lock);
    const bool result = deliveries.FinishTransmit(id, success);
    portEXIT_CRITICAL(&state_lock);
    return result;
}

bool ConfirmDelivery(
    obc_delivery::ID id
) {
    portENTER_CRITICAL(&state_lock);
    const bool result = deliveries.Confirm(id);
    if (!writer_task) deliveries.ReaderFailed();
    portEXIT_CRITICAL(&state_lock);
    if (result && writer_task) xTaskNotifyGive(writer_task);
    return result;
}

obc_storage::STATUS RequestLog(
    uint16_t sequence,
    const ttc::LOG_REQUEST& request
) {
    if (!writer_task) return obc_storage::STATUS::NOT_INITIALIZED;
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    portENTER_CRITICAL(&state_lock);
    const bool reserved = log_mailbox.Submit(sequence, request, now_ms);
    portEXIT_CRITICAL(&state_lock);
    if (reserved) xTaskNotifyGive(writer_task);
    return reserved ? obc_storage::STATUS::OK : obc_storage::STATUS::BUSY;
}

bool TakeLogResponse(
    obc_downlink::LOG_RESPONSE& out
) {
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    portENTER_CRITICAL(&state_lock);
    const bool ready = log_mailbox.Take(now_ms, out);
    portEXIT_CRITICAL(&state_lock);
    return ready;
}

} // namespace esp32
} // namespace obc_flow
