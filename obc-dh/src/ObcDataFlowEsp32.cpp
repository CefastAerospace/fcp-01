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
    obc_payload::Receiver receiver;
    std::unique_ptr<uint8_t[]> snapshot_first;
    std::unique_ptr<uint8_t[]> snapshot_second;
    enum class SLOT_STATE { FREE, ENCODING, READY, WRITING };
    SLOT_STATE snapshot_state = SLOT_STATE::FREE;
    size_t snapshot_size = 0;
    portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
    TaskHandle_t payload_task = nullptr;
    TaskHandle_t writer_task = nullptr;
    bool attempted = false;
    bool active = false;
    bool writer_ready = false;
    uint64_t boot_id = 0;
    uint64_t startup_storage_failures = 0;
    uint64_t last_diagnostic_ms = 0;
    uint64_t maximum_poll_gap_ms = 0;
    obc_storage::RESULT writer_result = {};
    constexpr uint32_t payload_stack_bytes = 8 * 1024;
    constexpr uint32_t writer_stack_bytes = 8 * 1024;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Apenas o escritor acessa estas funções do driver; não há remontagem.
#ifndef OBC_FLOW_BENCH
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
#endif

    // Cópia limitada sob proteção; codificação e notificações ficam fora dela.
    void Submit(
        const uint8_t* bytes,
        size_t size
    ) {
        portENTER_CRITICAL(&state_lock);
        const bool queued = writer_ready && records.Push(bytes, size);
        if (!queued) state.DropRecords();
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
            state.DropRecords();
            portEXIT_CRITICAL(&state_lock);
        }
    }

    // Reserva exclusiva do único bloco grande; não mantém lock ao codificar.
    void PublishSnapshot(
        const obc_data::PAYLOAD_SNAPSHOT& snapshot
    ) {
        portENTER_CRITICAL(&state_lock);
        const bool observed = state.ObserveSnapshot(snapshot);
        const bool reserved = observed && writer_ready && snapshot_state == SLOT_STATE::FREE;
        if (reserved) snapshot_state = SLOT_STATE::ENCODING;
        else state.DropRecords();
        portEXIT_CRITICAL(&state_lock);
        if (!reserved) return;

        size_t size = 0;
        const bool encoded = obc_record::EncodeSnapshotParts(snapshot,
            snapshot_first.get(), snapshot_first_bytes, snapshot_second.get(),
            snapshot_second_bytes, size) == obc_record::STATUS::OK;
        portENTER_CRITICAL(&state_lock);
        if (encoded) {
            snapshot_size = size;
            snapshot_state = SLOT_STATE::READY;
        } else {
            snapshot_state = SLOT_STATE::FREE;
            state.DropRecords();
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
            const obc_payload::STATUS status = obc_payload::esp32::Poll();
            if (status == obc_payload::STATUS::READY) PublishSnapshot(*receiver.GetSnapshot());
            const obc_payload::STATISTICS& observed = receiver.GetStatistics();
            portENTER_CRITICAL(&state_lock);
            state.SetPayloadStatistics(observed.complete_candidates, observed.rejected,
                observed.missing_positions, observed.duplicates);
            const uint64_t gap = now_ms - previous_ms;
            if (gap > maximum_poll_gap_ms) maximum_poll_gap_ms = gap;
            portEXIT_CRITICAL(&state_lock);
            previous_ms = now_ms;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // Publica apenas resultados pequenos; nunca compartilha o Journal em uso.
    void UpdateWriterState(
    ) {
        portENTER_CRITICAL(&state_lock);
        writer_ready = journal.IsReady();
        writer_result = journal.GetLastResult();
        state.SetStorageStatistics(journal.GetWritten(), startup_storage_failures + journal.GetFailures());
        portEXIT_CRITICAL(&state_lock);
    }

    // Um registro pequeno e um snapshot por rodada evitam monopolizar a fila.
    void WriterEntry(
        void*
    ) {
        const obc_time::EVENT_STAMP stamp = {boot_id, obc_time::esp32::GetMonotonicMs()};
#ifdef OBC_FLOW_BENCH
        const JOURNAL_IO io = GetBenchIO();
#else
        const JOURNAL_IO io = {Open, Append, Close, nullptr};
#endif
        journal.Init(io, stamp, static_cast<uint32_t>(esp_reset_reason()));
        UpdateWriterState();
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
            portENTER_CRITICAL(&state_lock);
            const bool small = records.Pop(block);
            portEXIT_CRITICAL(&state_lock);
            if (small && !journal.Write(block.bytes, block.size)) {
                portENTER_CRITICAL(&state_lock);
                state.DropRecords();
                portEXIT_CRITICAL(&state_lock);
            }
            portENTER_CRITICAL(&state_lock);
            const bool snapshot = snapshot_state == SLOT_STATE::READY;
            if (snapshot) snapshot_state = SLOT_STATE::WRITING;
            const size_t size = snapshot_size;
            portEXIT_CRITICAL(&state_lock);
            if (snapshot) {
                const size_t first_size = size < snapshot_first_bytes ? size : snapshot_first_bytes;
                const bool written = journal.WriteParts(snapshot_first.get(), first_size,
                    snapshot_second.get(), size - first_size);
                portENTER_CRITICAL(&state_lock);
                snapshot_state = SLOT_STATE::FREE;
                if (!written) state.DropRecords();
                portEXIT_CRITICAL(&state_lock);
            }
            UpdateWriterState();
            // Uma notificação pode representar várias entradas; não dormir com fila pendente.
            if (small || snapshot) taskYIELD();
            else ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
        }
    }

} // namespace

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

bool Init(
    const obc_startup::STARTUP_REPORT& report
) {
    if (attempted) return active;
    attempted = true;
    boot_id = report.time_status == obc_time::STATUS::OK ? report.boot_id : 0;
    state.Init(boot_id);
    if (boot_id == 0 || !obc_payload::esp32::Init(receiver, boot_id)) {
        Serial.printf("[FLOW] Recepcao inativa: boot ou memoria indisponivel\n");
        return false;
    }
    bool storage = report.storage_result.status == obc_storage::STATUS::OK;
    Serial.printf("[FLOW] RAM apos receptor: heap=%u byte_heap=%u byte_largest=%u\n",
        ESP.getFreeHeap(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (storage) {
        snapshot_first.reset(new (std::nothrow) uint8_t[snapshot_first_bytes]);
        snapshot_second.reset(new (std::nothrow) uint8_t[snapshot_second_bytes]);
        // Orçamento do parser + pilhas + margem para estruturas FreeRTOS.
        const size_t reserve = obc_payload::parser_memory_bytes + payload_stack_bytes + writer_stack_bytes
            + obc_payload::esp32::transport_stack_bytes + 4096;
        Serial.printf("[FLOW] RAM apos bloco SD: bloco=%u heap=%u byte_heap=%u reserva=%u\n",
            snapshot_first && snapshot_second ? 1U : 0U, ESP.getFreeHeap(),
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT), static_cast<unsigned>(reserve));
        if (!snapshot_first || !snapshot_second || ESP.getFreeHeap() < reserve) {
            storage = false;
            snapshot_first.reset();
            snapshot_second.reset();
            Serial.printf("[FLOW] Escritor inativo: reserva insuficiente para parser e tarefas\n");
        }
    }
    startup_storage_failures = storage ? 0 : 1;
    writer_result = report.storage_result;
    if (!storage) state.DropRecords(); // BOOT pretendido, sem escritor disponível.
    state.SetStorageStatistics(0, startup_storage_failures);
    active = obc_payload::esp32::StartTransportTask()
        && xTaskCreatePinnedToCore(PayloadEntry, "payload_rx", payload_stack_bytes,
            nullptr, 2, &payload_task, 1) == pdPASS;
    if (active && storage) {
        if (xTaskCreatePinnedToCore(WriterEntry, "obc_journal", writer_stack_bytes,
                nullptr, 1, &writer_task, 1) != pdPASS) {
            snapshot_first.reset();
            snapshot_second.reset();
            startup_storage_failures = 1;
            portENTER_CRITICAL(&state_lock);
            state.SetStorageStatistics(0, 1);
            portEXIT_CRITICAL(&state_lock);
            Serial.printf("[FLOW] Escritor inativo: falha ao criar tarefa\n");
        }
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

obc_data::TELEMETRY_DATA ComposeTelemetry(
    uint64_t& sampled_ms
) {
    sampled_ms = obc_time::esp32::GetMonotonicMs();
    portENTER_CRITICAL(&state_lock);
    const obc_data::TELEMETRY_DATA telemetry = state.ComposeTelemetry(sampled_ms);
    portEXIT_CRITICAL(&state_lock);
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
        state.DropRecords();
        portEXIT_CRITICAL(&state_lock);
    }
}

void ReportDiagnostics(
) {
    const uint64_t now_ms = obc_time::esp32::GetMonotonicMs();
    if (now_ms - last_diagnostic_ms < 1000 || boot_id == 0) return;
    last_diagnostic_ms = now_ms;
    portENTER_CRITICAL(&state_lock);
    const obc_data::STATISTICS_DATA statistics = state.GetStatistics();
    const bool storage = writer_ready;
    const obc_storage::RESULT result = writer_result;
    const uint64_t poll_gap = maximum_poll_gap_ms;
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
    previous = statistics;
}

} // namespace esp32
} // namespace obc_flow
