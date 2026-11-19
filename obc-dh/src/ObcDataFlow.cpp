#include "ObcDataFlow.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDataFlow.h".
 */

namespace obc_flow {
namespace {

    // Soma saturada comum aos contadores locais; não altera os do transporte.
    void Add(
        uint64_t& value,
        uint64_t count = 1
    ) {
        const uint64_t maximum = std::numeric_limits<uint64_t>::max();
        value = maximum - value < count ? maximum : value + count;
    }

} // namespace

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
void State::Init(
    uint64_t boot_id
) {
    boot_id_ = boot_id;
    has_reference_ = false;
    has_snapshot_ = false;
    last_snapshot_ms_ = 0;
    aircraft_count_ = 0;
    reference_ = {};
    statistics_ = {};
}

// Público:
bool State::ObserveSnapshot(
    const obc_data::PAYLOAD_SNAPSHOT& snapshot
) {
    if (boot_id_ == 0 || snapshot.stamp.boot_id != boot_id_
            || snapshot.aircraft_count > obc_data::max_snapshot_aircraft) return false;
    has_snapshot_ = true;
    last_snapshot_ms_ = snapshot.stamp.monotonic_ms;
    aircraft_count_ = snapshot.aircraft_count;
    return true;
}

// Público:
bool State::GetLastSnapshotTime(
    uint64_t& sampled_ms
) const {
    if (!has_snapshot_) return false;
    sampled_ms = last_snapshot_ms_;
    return true;
}

// Público:
bool State::SetTime(
    uint32_t unix_s,
    uint64_t now_ms,
    uint32_t rx_ms,
    uint32_t delay_ms,
    obc_time::TIME_REFERENCE& reference
) {
    const uint32_t age = static_cast<uint32_t>(now_ms) - rx_ms;
    if (boot_id_ == 0 || age >= 0x80000000UL || age > now_ms) return false;
    obc_time::TIME_REFERENCE created = {};
    const obc_time::EVENT_STAMP reception = {boot_id_, now_ms - age};
    if (obc_time::CreateReference(reception, unix_s, created) != obc_time::STATUS::OK) return false;
    created.delay_ms = delay_ms;
    reference_ = created;
    has_reference_ = true;
    reference = created;
    return true;
}

// Público:
bool State::GetTimeReference(
    obc_time::TIME_REFERENCE& reference
) const {
    if (!has_reference_) return false;
    reference = reference_;
    return true;
}

// Público:
obc_data::TELEMETRY_DATA State::ComposeTelemetry(
    uint64_t now_ms
) const {
    obc_data::TELEMETRY_DATA telemetry = {};
    telemetry.mode = obc_data::MODE::BENCH;
    if (has_snapshot_) {
        telemetry.available_fields |= obc_data::TM_AIRCRAFT;
        telemetry.aircraft_count = aircraft_count_;
    }
    int64_t unix_ms = 0;
    const obc_time::EVENT_STAMP stamp = {boot_id_, now_ms};
    if (obc_time::Reconstruct(stamp, has_reference_ ? &reference_ : nullptr, unix_ms)
            == obc_time::STATUS::OK && static_cast<uint64_t>(unix_ms / 1000) <= UINT32_MAX) {
        telemetry.available_fields |= obc_data::TM_TIMESTAMP;
        telemetry.timestamp = static_cast<uint32_t>(unix_ms / 1000);
    }
    return telemetry;
}

// Público:
void State::SetPayloadStatistics(
    uint64_t complete_candidates,
    uint64_t rejected,
    uint64_t missing_positions,
    uint64_t duplicates
) {
    statistics_.received = complete_candidates;
    statistics_.invalid = rejected;
    statistics_.gaps = missing_positions;
    statistics_.duplicates = duplicates;
}

// Público:
void State::SetStorageStatistics(
    uint64_t written,
    uint64_t failures
) {
    statistics_.records_written = written;
    statistics_.storage_failures = failures;
}

// Público:
void State::DropRecords(
    uint64_t count
) {
    Add(statistics_.records_dropped, count);
}

// Público:
obc_data::STATISTICS_DATA State::GetStatistics(
) const {
    return statistics_;
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool SnapshotBuffers::Reserve(
    size_t size
) {
    if (size < obc_record::min_snapshot_record_bytes || size > obc_record::max_snapshot_record_bytes) return false;
    Reset();
    const size_t first_size = size < snapshot_first_bytes ? size : snapshot_first_bytes;
    first_.reset(new (std::nothrow) uint8_t[first_size]);
    if (!first_) return false;
    const size_t second_size = size - first_size;
    if (second_size != 0) second_.reset(new (std::nothrow) uint8_t[second_size]);
    if (second_size != 0 && !second_) {
        Reset();
        return false;
    }
    return true;
}

// Público:
void SnapshotBuffers::Reset(
) {
    first_.reset();
    second_.reset();
}

// ─── GETTERS ──────────────────────────────────────────────────────

// Público:
uint8_t* SnapshotBuffers::GetFirstData(
) const { return first_.get(); }

// Público:
uint8_t* SnapshotBuffers::GetSecondData(
) const { return second_.get(); }

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool RecordQueue::Push(
    const uint8_t* bytes,
    size_t size
) {
    if (bytes == nullptr || size == 0 || size > small_record_bytes
            || size_ == small_record_slots) return false;
    RECORD_BLOCK& block = blocks_[(head_ + size_) % small_record_slots];
    block.size = size;
    std::memcpy(block.bytes, bytes, size);
    ++size_;
    return true;
}

// Público:
bool RecordQueue::Pop(
    RECORD_BLOCK& out
) {
    if (size_ == 0) return false;
    out = blocks_[head_];
    head_ = (head_ + 1) % small_record_slots;
    --size_;
    return true;
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool Journal::Init(
    const JOURNAL_IO& io,
    const obc_time::EVENT_STAMP& stamp,
    uint32_t reset_reason
) {
    if (attempted_ || stamp.boot_id == 0 || !io.open || !io.append || !io.close) return false;
    attempted_ = true;
    io_ = io;
    boot_stamp_ = stamp;
    obc_record::RECORD record = {};
    record.header = {1, obc_data::RECORD_TYPE::BOOT, 40, stamp};
    record.body.boot = {stamp.boot_id - 1, reset_reason};
    size_t written = 0;
    if (obc_record::Encode(record, boot_, sizeof(boot_), written) != obc_record::STATUS::OK) return false;
    boot_encoded_ = true;
    return OpenSegment();
}

// Público:
bool Journal::Resume(
    const obc_time::TIME_REFERENCE* reference
) {
    if (!boot_encoded_ || ready_) return false;
    if (reference) {
        obc_record::RECORD record = {};
        record.header = {1, obc_data::RECORD_TYPE::TIME_REFERENCE, 36, reference->reception};
        record.body.time_reference = *reference;
        uint8_t bytes[sizeof(reference_)];
        size_t size = 0;
        if (reference->reception.boot_id != boot_stamp_.boot_id
                || obc_record::Encode(record, bytes, sizeof(bytes), size) != obc_record::STATUS::OK) {
            last_ = {obc_storage::STATUS::INVALID_ARGUMENT, 0, 0, 0};
            Add(failures_);
            return false;
        }
        std::memcpy(reference_, bytes, sizeof(reference_));
        has_reference_ = true;
    }
    if (segment_ == UINT32_MAX) {
        last_ = {obc_storage::STATUS::FILE_LIMIT, 0, 0, 0};
        Add(failures_);
        return false;
    }
    ++segment_;
    return OpenSegment();
}

// Público:
bool Journal::WriteReference(
    const obc_time::TIME_REFERENCE& reference
) {
    if (!ready_ || reference.reception.boot_id != boot_stamp_.boot_id) return false;
    obc_record::RECORD record = {};
    record.header = {1, obc_data::RECORD_TYPE::TIME_REFERENCE, 36, reference.reception};
    record.body.time_reference = reference;
    uint8_t bytes[sizeof(reference_)];
    size_t size = 0;
    if (obc_record::Encode(record, bytes, sizeof(bytes), size) != obc_record::STATUS::OK) return false;
    if (has_reference_ && std::memcmp(reference_, bytes, size) == 0) return true;
    return Write(bytes, size);
}

// Privado:
bool Journal::OpenSegment(
) {
    char name[obc_storage::filename_chars + 1];
    std::snprintf(name, sizeof(name), "boot_%016llx_%08lx.bin",
        static_cast<unsigned long long>(boot_stamp_.boot_id), static_cast<unsigned long>(segment_));
    last_ = io_.open(io_.context, name);
    ready_ = last_.status == obc_storage::STATUS::OK;
    if (!ready_) { Add(failures_); return false; }
    confirmed_ = 0;
    if (!Append(boot_, sizeof(boot_))) return false;
    return !has_reference_ || Append(reference_, sizeof(reference_));
}

// Privado:
bool Journal::Append(
    const uint8_t* bytes,
    size_t size,
    bool count_record
) {
    last_ = io_.append(io_.context, bytes, size);
    if (last_.status == obc_storage::STATUS::OK) {
        confirmed_ += static_cast<uint32_t>(size);
        if (count_record) Add(written_);
        return true;
    }
    Add(failures_);
    if (last_.status != obc_storage::STATUS::FILE_LIMIT) {
        ready_ = false;
        const obc_storage::RESULT closed = io_.close(io_.context);
        if (closed.status != obc_storage::STATUS::OK) Add(failures_);
    }
    return false;
}

// Privado:
bool Journal::Rotate(
) {
    last_ = io_.close(io_.context);
    ready_ = false;
    if (last_.status != obc_storage::STATUS::OK) { Add(failures_); return false; }
    if (segment_ == UINT32_MAX) {
        last_ = {obc_storage::STATUS::FILE_LIMIT, 0, 0, 0};
        Add(failures_);
        return false;
    }
    ++segment_;
    return OpenSegment();
}

// Público:
bool Journal::Write(
    const uint8_t* bytes,
    size_t size
) {
    if (!ready_ || bytes == nullptr || size < obc_data::header_bytes + 4
            || size > obc_record::max_snapshot_record_bytes) return false;
    if (size > obc_storage::max_file_bytes - confirmed_ && !Rotate()) return false;
    if (!Append(bytes, size)) {
        if (last_.status != obc_storage::STATUS::FILE_LIMIT || !Rotate()) return false;
        if (!Append(bytes, size)) {
            if (ready_) {
                ready_ = false;
                const obc_storage::RESULT closed = io_.close(io_.context);
                if (closed.status != obc_storage::STATUS::OK) Add(failures_);
            }
            return false;
        }
    }
    if (size == sizeof(reference_) && std::memcmp(bytes, obc_data::record_magic, 4) == 0
            && bytes[4] == 1 && bytes[5] == static_cast<uint8_t>(obc_data::RECORD_TYPE::TIME_REFERENCE)) {
        std::memcpy(reference_, bytes, size);
        has_reference_ = true;
    }
    return true;
}

// Público:
bool Journal::WriteParts(
    const uint8_t* first,
    size_t first_size,
    const uint8_t* second,
    size_t second_size
) {
    if (second_size == 0) return Write(first, first_size);
    if (!ready_ || first == nullptr || second == nullptr || first_size < obc_data::header_bytes
            || first_size > obc_record::max_snapshot_record_bytes
            || second_size > obc_record::max_snapshot_record_bytes - first_size) return false;
    const size_t size = first_size + second_size;
    if (size > obc_storage::max_file_bytes - confirmed_ && !Rotate()) return false;
    if (!Append(first, first_size, false)) {
        if (last_.status != obc_storage::STATUS::FILE_LIMIT || !Rotate()) return false;
        if (!Append(first, first_size, false)) return false;
    }
    if (Append(second, second_size)) return true;
    // FILE_LIMIT inesperado no meio do registro também é terminal: não migrar a cauda.
    if (ready_) {
        ready_ = false;
        if (io_.close(io_.context).status != obc_storage::STATUS::OK) Add(failures_);
    }
    return false;
}

// ─── GETTERS ──────────────────────────────────────────────────────

// Público:
bool Journal::Seal(
) { return ready_ && Rotate(); }

// Público:
bool Journal::IsReady(
) const { return ready_; }

// Público:
uint64_t Journal::GetWritten(
) const { return written_; }

// Público:
uint64_t Journal::GetFailures(
) const { return failures_; }

// Público:
obc_storage::RESULT Journal::GetLastResult(
) const { return last_; }

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool StorageRecovery::Service(
    const RECOVERY_IO& io,
    Journal& journal,
    uint64_t now_ms,
    const obc_time::TIME_REFERENCE* reference
) {
    if (!io.end || !io.init || journal.IsReady()
            || diagnostic_.state == RECOVERY_STATE::EXHAUSTED) return false;
    if (diagnostic_.state != RECOVERY_STATE::WAITING) {
        if (diagnostic_.state == RECOVERY_STATE::IDLE) {
            Add(diagnostic_.episodes);
            diagnostic_.cause = journal.GetLastResult();
        }
        if (diagnostic_.attempts == max_attempts) {
            Exhaust();
            return false;
        }
        diagnostic_.state = RECOVERY_STATE::WAITING;
        waiting_since_ms_ = now_ms;
        return false;
    }
    if (now_ms - waiting_since_ms_ < retry_delay_ms) return false;
    ++diagnostic_.attempts;
    Add(diagnostic_.total_attempts);
    diagnostic_.last_end = io.end(io.context);
    diagnostic_.last_result = diagnostic_.last_end;
    // Estes erros conservam o resultado do fechamento, mas End já liberou
    // os recursos. BUSY/contexto inválido não autorizam iniciar por cima.
    const auto ended = diagnostic_.last_end.status;
    const bool released = ended == obc_storage::STATUS::OK
        || ended == obc_storage::STATUS::WRITER_FAILED
        || ended == obc_storage::STATUS::SYNC_FAILED
        || ended == obc_storage::STATUS::CLOSE_FAILED;
    bool mounted = false;
    if (released) {
        diagnostic_.last_result = io.init(io.context);
        mounted = diagnostic_.last_result.status == obc_storage::STATUS::OK;
    }
    if (!mounted) Add(diagnostic_.lifecycle_failures);
    if (mounted) {
        const bool resumed = journal.Resume(reference);
        diagnostic_.last_result = journal.GetLastResult();
        if (resumed) {
            diagnostic_.state = RECOVERY_STATE::VERIFYING;
            return true;
        }
    }
    // A próxima chamada captura o tempo após o retorno do I/O. Assim uma
    // montagem lenta não consome os 5 s de espera da tentativa seguinte.
    diagnostic_.state = RECOVERY_STATE::RETRY;
    if (diagnostic_.attempts == max_attempts
            || diagnostic_.last_result.status == obc_storage::STATUS::FILE_LIMIT) Exhaust();
    return false;
}

// Privado:
void StorageRecovery::Exhaust(
) {
    diagnostic_.state = RECOVERY_STATE::EXHAUSTED;
    Add(diagnostic_.exhausted);
}

// Público:
void StorageRecovery::ObserveWrite(
    bool success
) {
    if (!success || diagnostic_.state != RECOVERY_STATE::VERIFYING) return;
    diagnostic_.state = RECOVERY_STATE::IDLE;
    diagnostic_.attempts = 0;
    Add(diagnostic_.recovered);
}

// ─── GETTERS ──────────────────────────────────────────────────────

// Público:
STORAGE_RECOVERY_DIAGNOSTICS StorageRecovery::GetDiagnostics(
) const { return diagnostic_; }

} // namespace obc_flow
