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
    aircraft_count_ = snapshot.aircraft_count;
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
    return OpenSegment();
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

} // namespace obc_flow
