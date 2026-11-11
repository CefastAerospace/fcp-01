#include "ObcDelivery.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDelivery.h".
 */

namespace obc_delivery {
namespace {

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Contadores de missão não voltam a zero por overflow.
    void Add(
        uint64_t& value
    ) { if (value != UINT64_MAX) ++value; }

    // Larguras explícitas evitam padding, alinhamento e endianness da RAM.
    void Put(
        uint8_t* bytes,
        uint64_t value,
        size_t width
    ) {
        for (size_t index = 0; index < width; ++index) bytes[index] = static_cast<uint8_t>(value >> (8 * index));
    }

    // Inversa de Put para larguras até oito bytes.
    uint64_t Get(
        const uint8_t* bytes,
        size_t width
    ) {
        uint64_t value = 0;
        for (size_t index = 0; index < width; ++index) value |= static_cast<uint64_t>(bytes[index]) << (8 * index);
        return value;
    }

    // Projeção conservadora dos únicos campos numéricos com unidade definida.
    AIRCRAFT Project(
        const obc_data::AIRCRAFT_STATE& source
    ) {
        AIRCRAFT out = {};
        out.icao = source.icao;
        out.available_fields = source.available_fields & (obc_data::SNAPSHOT_LATITUDE
            | obc_data::SNAPSHOT_LONGITUDE | obc_data::SNAPSHOT_ALTITUDE);
        if (out.available_fields & obc_data::SNAPSHOT_LATITUDE) {
            out.latitude = source.latitude;
            out.longitude = source.longitude;
        }
        if (out.available_fields & obc_data::SNAPSHOT_ALTITUDE) out.altitude = source.altitude;
        return out;
    }

    // Compara valores significativos já normalizados pela projeção.
    bool Same(
        const AIRCRAFT& left,
        const AIRCRAFT& right
    ) {
        return left.icao == right.icao && left.available_fields == right.available_fields
            && left.latitude == right.latitude && left.longitude == right.longitude
            && left.altitude == right.altitude;
    }

    // Validação da projeção sem alocar o modelo completo com extras na pilha.
    bool Valid(
        const AIRCRAFT& value
    ) {
        const uint8_t position = obc_data::SNAPSHOT_LATITUDE | obc_data::SNAPSHOT_LONGITUDE;
        const uint8_t mask = position | obc_data::SNAPSHOT_ALTITUDE;
        if (value.icao > 0xffffff || (value.available_fields & ~mask)
                || ((value.available_fields & position) != 0 && (value.available_fields & position) != position)) return false;
        if (value.available_fields & position) {
            if (!std::isfinite(value.latitude) || !std::isfinite(value.longitude)
                    || value.latitude < -90 || value.latitude > 90
                    || value.longitude < -180 || value.longitude > 180) return false;
        } else if (value.latitude != 0 || value.longitude != 0) return false;
        return value.available_fields & obc_data::SNAPSHOT_ALTITUDE
            ? std::isfinite(value.altitude) : value.altitude == 0;
    }

    // Leitura exata e tamanho estável no arquivo fechado.
    bool ReadExact(
        const READER_IO& io,
        const obc_storage::FILE_INFO& file,
        uint32_t offset,
        uint8_t* bytes,
        size_t size
    ) {
        uint32_t total = 0;
        const auto result = io.read(io.context, file.name, offset, bytes, size, total);
        return result.status == obc_storage::STATUS::OK && result.transferred == size && total == file.size;
    }

    // Valida CRC de registro grande mantendo somente 256 bytes de trabalho.
    bool CheckCrc(
        const READER_IO& io,
        const obc_storage::FILE_INFO& file,
        uint32_t offset,
        uint32_t size,
        bool& valid
    ) {
        uint8_t bytes[256];
        uint32_t crc = obc_data::crc32_initial;
        for (uint32_t position = 0; position < size - 4;) {
            const size_t count = size - 4 - position < sizeof(bytes) ? size - 4 - position : sizeof(bytes);
            if (!ReadExact(io, file, offset + position, bytes, count)) return false;
            crc = obc_record::ContinueCrc32(crc, bytes, count);
            position += static_cast<uint32_t>(count);
        }
        if (!ReadExact(io, file, offset + size - 4, bytes, 4)) return false;
        valid = Get(bytes, 4) == (crc ^ obc_data::crc32_final_xor);
        return true;
    }

    struct MATCH {
        ID id;
        bool confirmation;
        bool found;
    };

    // Busca pontual sem construir índice proporcional ao histórico.
    bool MatchRecord(
        void* context,
        const RECORD& record
    ) {
        MATCH& match = *static_cast<MATCH*>(context);
        if (record.confirmation == match.confirmation && Compare(record.delivery.id, match.id) == 0) {
            match.found = true;
            return false;
        }
        return true;
    }

    struct SEARCH {
        const READER_IO* io;
        ID after;
        DELIVERY candidate;
        bool found;
        bool failed;
        uint64_t orphans;
    };

    // Para cada DATA candidato, procura seu CONFIRM em outra passagem limitada.
    bool PendingRecord(
        void* context,
        const RECORD& record
    ) {
        SEARCH& search = *static_cast<SEARCH*>(context);
        if (record.confirmation || Compare(record.delivery.id, search.after) <= 0
                || (search.found && Compare(record.delivery.id, search.candidate.id) >= 0)) return true;
        MATCH match = {record.delivery.id, true, false};
        RECOVERY ignored = {};
        if (!Scan(*search.io, MatchRecord, &match, ignored)) {
            search.failed = true;
            return false;
        }
        if (!match.found) {
            search.candidate = record.delivery;
            search.found = true;
        }
        return true;
    }

    // Um CONFIRM órfão nunca elimina outro DATA durante recuperação.
    bool AuditRecord(
        void* context,
        const RECORD& record
    ) {
        SEARCH& search = *static_cast<SEARCH*>(context);
        if (!record.confirmation) return true;
        MATCH match = {record.delivery.id, false, false};
        RECOVERY ignored = {};
        if (!Scan(*search.io, MatchRecord, &match, ignored)) {
            search.failed = true;
            return false;
        }
        if (!match.found) Add(search.orphans);
        return true;
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

int Compare(
    ID left,
    ID right
) {
    if (left.boot != right.boot) return left.boot < right.boot ? -1 : 1;
    if (left.sequence != right.sequence) return left.sequence < right.sequence ? -1 : 1;
    return 0;
}

bool Encode(
    const RECORD& record,
    uint8_t* bytes,
    size_t capacity,
    size_t& written
) {
    written = 0;
    const DELIVERY& delivery = record.delivery;
    const size_t size = record.confirmation ? confirm_bytes : data_bytes;
    if (!bytes || capacity < size || !delivery.id.boot || !delivery.id.sequence
            || (!record.confirmation && !Valid(delivery.aircraft))) return false;
    std::memset(bytes, 0, size);
    std::memcpy(bytes, "FCD1", 4);
    bytes[4] = 1;
    bytes[5] = record.confirmation ? 2 : 1;
    Put(bytes + 6, size, 2);
    Put(bytes + 8, delivery.id.boot, 8);
    Put(bytes + 16, delivery.id.sequence, 8);
    if (!record.confirmation) {
        Put(bytes + 24, delivery.monotonic_ms, 8);
        Put(bytes + 32, delivery.aircraft.icao, 4);
        bytes[36] = delivery.aircraft.available_fields;
        const double values[] = {delivery.aircraft.latitude, delivery.aircraft.longitude, delivery.aircraft.altitude};
        for (size_t index = 0; index < 3; ++index) {
            uint64_t bits = 0;
            std::memcpy(&bits, values + index, 8);
            Put(bytes + 40 + 8 * index, bits, 8);
        }
    }
    Put(bytes + size - 4, obc_record::ComputeCrc32(bytes, size - 4), 4);
    written = size;
    return true;
}

bool Decode(
    const uint8_t* bytes,
    size_t size,
    RECORD& out
) {
    if (!bytes || size < confirm_bytes || std::memcmp(bytes, "FCD1", 4) || bytes[4] != 1
            || (bytes[5] != 1 && bytes[5] != 2)) return false;
    const bool confirmation = bytes[5] == 2;
    if (size != (confirmation ? confirm_bytes : data_bytes) || Get(bytes + 6, 2) != size
            || Get(bytes + size - 4, 4) != obc_record::ComputeCrc32(bytes, size - 4)) return false;
    RECORD record = {};
    record.confirmation = confirmation;
    DELIVERY& delivery = record.delivery;
    delivery.id = {Get(bytes + 8, 8), Get(bytes + 16, 8)};
    delivery.monotonic_ms = Get(bytes + 24, 8);
    if (!delivery.id.boot || !delivery.id.sequence) return false;
    if (confirmation) {
        if (delivery.monotonic_ms != 0) return false;
    } else {
        delivery.aircraft.icao = static_cast<uint32_t>(Get(bytes + 32, 4));
        delivery.aircraft.available_fields = bytes[36];
        if (bytes[37] || bytes[38] || bytes[39]) return false;
        double* values[] = {&delivery.aircraft.latitude, &delivery.aircraft.longitude, &delivery.aircraft.altitude};
        for (size_t index = 0; index < 3; ++index) {
            const uint64_t bits = Get(bytes + 40 + 8 * index, 8);
            std::memcpy(values[index], &bits, 8);
        }
        if (!Valid(delivery.aircraft)) return false;
    }
    out = record;
    return true;
}

bool Scan(
    const READER_IO& io,
    VISITOR visitor,
    void* context,
    RECOVERY& diagnostic
) {
    diagnostic = {};
    if (!io.list || !io.read) return false;
    char after[obc_storage::filename_chars + 1] = {};
    for (;;) {
        obc_storage::FILE_INFO file = {};
        if (io.list(io.context, after, file).status != obc_storage::STATUS::OK) return false;
        if (!file.name[0]) return true;
        if (std::strcmp(file.name, after) <= 0) return false;
        std::strcpy(after, file.name);
        const size_t length = std::strlen(file.name);
        if (file.active || length != 34 || std::strncmp(file.name, "boot_", 5)
                || std::strcmp(file.name + length - 4, ".bin")) continue;
        uint32_t offset = 0;
        while (offset < file.size) {
            uint8_t bytes[data_bytes] = {};
            const size_t remaining = file.size - offset;
            const size_t count = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
            if (!ReadExact(io, file, offset, bytes, count)) return false;
            const bool delivery = count >= 4 && std::memcmp(bytes, "FCD1", 4) == 0;
            const bool history = count >= 4 && std::memcmp(bytes, "FCP", 3) == 0
                && bytes[3] >= '1' && bytes[3] <= '3';
            if ((delivery || history) && count < 8) Add(diagnostic.incomplete_tails);
            if ((delivery || history) && count >= 8) {
                const uint32_t size = static_cast<uint32_t>(Get(bytes + 6, 2));
                const bool length_valid = delivery ? (size == data_bytes || size == confirm_bytes)
                    : (size >= obc_data::header_bytes + 4 && size <= obc_record::max_snapshot_record_bytes);
                if (length_valid && size > remaining) Add(diagnostic.incomplete_tails);
                if (length_valid && size <= remaining) {
                    bool valid = false;
                    RECORD record = {};
                    if (delivery) valid = Decode(bytes, size, record);
                    else if (!CheckCrc(io, file, offset, size, valid)) return false;
                    if (valid) {
                        offset += size;
                        if (delivery) {
                            Add(record.confirmation ? diagnostic.confirmations : diagnostic.data);
                            if (visitor && !visitor(context, record)) return true;
                        }
                        continue;
                    }
                }
            }
            Add(diagnostic.corrupt_bytes);
            ++offset;
        }
    }
}

bool Audit(
    const READER_IO& io,
    RECOVERY& diagnostic
) {
    SEARCH search = {};
    search.io = &io;
    const bool success = Scan(io, AuditRecord, &search, diagnostic);
    diagnostic.orphan_confirmations = search.orphans;
    return success && !search.failed;
}

bool FindPending(
    const READER_IO& io,
    ID after,
    DELIVERY& out,
    bool& found
) {
    found = false;
    SEARCH search = {};
    search.io = &io;
    search.after = after;
    RECOVERY ignored = {};
    if (!Scan(io, PendingRecord, &search, ignored) || search.failed) return false;
    found = search.found;
    if (found) out = search.candidate;
    return true;
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
void Queue::Init(
    uint64_t boot,
    bool recovery_pending
) {
    for (auto& entry : entries_) entry = {};
    previous_count_ = 0;
    boot_ = boot;
    sequence_ = 0;
    frontier_ = {};
    inflight_ = {};
    transmitting_ = false;
    waiting_ = false;
    disk_checked_ = !recovery_pending;
    statistics_ = {};
}

// Público:
void Queue::ObserveValidatedSnapshot(
    const obc_data::PAYLOAD_SNAPSHOT& snapshot
) {
    if (!boot_ || snapshot.stamp.boot_id != boot_ || snapshot.aircraft_count > obc_data::max_snapshot_aircraft) return;
    AIRCRAFT projected[obc_data::max_snapshot_aircraft] = {};
    for (size_t index = 0; index < snapshot.aircraft_count; ++index) {
        projected[index] = Project(snapshot.aircraft[index]);
        bool same = false;
        for (size_t previous = 0; previous < previous_count_; ++previous) {
            if (Same(projected[index], previous_[previous])) same = true;
        }
        if (same) { Add(statistics_.repeated); continue; }
        ENTRY* entry = sequence_ == UINT64_MAX ? nullptr : Reserve();
        if (!entry) { Add(statistics_.lost); continue; }
        entry->delivery = {{boot_, ++sequence_}, snapshot.stamp.monotonic_ms, projected[index]};
        Add(statistics_.accepted);
    }
    std::memcpy(previous_, projected, sizeof(projected));
    previous_count_ = snapshot.aircraft_count;
}

// Privado:
Queue::ENTRY* Queue::Reserve(
    bool for_recovery
) {
    // A admissão conserva uma posição livre até a consulta durável terminar.
    const bool free_slot_allowed = for_recovery || disk_checked_ || statistics_.occupied < cache_slots - 1;
    if (free_slot_allowed) {
        for (auto& entry : entries_) {
            if (!entry.used) {
                entry = {};
                entry.used = true;
                ++statistics_.occupied;
                if (statistics_.occupied > statistics_.high_water) statistics_.high_water = statistics_.occupied;
                return &entry;
            }
        }
    }
    // Somente cópias duráveis e sem consumidor podem sair do cache.
    for (auto& entry : entries_) {
        if (entry.stored && !entry.confirmed && !entry.busy && Compare(entry.delivery.id, inflight_) != 0) {
            disk_checked_ = false;
            Add(statistics_.evicted);
            if (!for_recovery && statistics_.occupied == cache_slots) {
                // Ao abrir uma consulta por evicção, a posição liberada fica reservada.
                Release(entry);
                continue;
            }
            entry = {};
            entry.used = true;
            return &entry;
        }
    }
    return nullptr;
}

// Privado:
Queue::ENTRY* Queue::Find(
    ID id
) {
    for (auto& entry : entries_) if (entry.used && Compare(entry.delivery.id, id) == 0) return &entry;
    return nullptr;
}

// Privado:
void Queue::Release(
    ENTRY& entry
) {
    entry = {};
    --statistics_.occupied;
}

// Público:
bool Queue::BeginStorage(
    RECORD& out
) {
    ENTRY* selected = nullptr;
    for (auto& entry : entries_) {
        if (entry.used && !entry.busy && (!entry.data_finished || (entry.stored && entry.confirmed))
                && (!selected || Compare(entry.delivery.id, selected->delivery.id) < 0)) selected = &entry;
    }
    if (!selected) return false;
    selected->busy = true;
    out = {selected->data_finished, selected->delivery};
    return true;
}

// Público:
void Queue::FinishStorage(
    const RECORD& record,
    bool success
) {
    ENTRY* entry = Find(record.delivery.id);
    if (!entry || !entry->busy) return;
    entry->busy = false;
    if (!success) Add(statistics_.storage_failures);
    if (!record.confirmation) {
        entry->data_finished = true;
        entry->stored = success;
        if (entry->confirmed && !success) Release(*entry);
    } else {
        // Uma falha de CONFIRM permite repetição do mesmo ID no próximo boot.
        Release(*entry);
    }
}

// Público:
bool Queue::BeginTransmit(
    uint64_t now_ms,
    uint64_t timeout_ms,
    DELIVERY& out
) {
    if (!disk_checked_ || transmitting_) return false;
    ENTRY* entry = nullptr;
    if (waiting_) {
        if (now_ms < attempt_ms_ || now_ms - attempt_ms_ < timeout_ms) return false;
        entry = Find(inflight_);
    } else {
        for (auto& candidate : entries_) {
            if (candidate.used && !candidate.confirmed
                    && (!entry || Compare(candidate.delivery.id, entry->delivery.id) < 0)) entry = &candidate;
        }
    }
    if (!entry) return false;
    out = entry->delivery;
    inflight_ = out.id;
    transmitting_ = true;
    waiting_ = true;
    attempt_ms_ = now_ms;
    Add(statistics_.attempts);
    return true;
}

// Público:
bool Queue::FinishTransmit(
    ID id,
    bool success
) {
    if (!transmitting_ || Compare(id, inflight_) != 0) return false;
    transmitting_ = false;
    if (!success) Add(statistics_.transmit_failures);
    return true;
}

// Público:
bool Queue::Confirm(
    ID id
) {
    ENTRY* entry = Find(id);
    if ((entry && entry->confirmed) || (frontier_.boot && Compare(id, frontier_) == 0)) return true;
    if (!entry || !waiting_ || Compare(id, inflight_) != 0) {
        Add(statistics_.unknown_confirmations);
        return false;
    }
    entry->confirmed = true;
    frontier_ = id;
    waiting_ = false;
    // O callback local pode chegar depois do ACK. A cópia do transporte continua válida.
    transmitting_ = false;
    inflight_ = {};
    disk_checked_ = false;
    Add(statistics_.confirmed);
    if (entry->data_finished && !entry->stored && !entry->busy) Release(*entry);
    return true;
}

// ─── GETTERS ──────────────────────────────────────────────────────

// Público:
ID Queue::GetFrontier(
) const { return frontier_; }

// Público:
bool Queue::NeedsRecovery(
) const { return !disk_checked_; }

// Público:
bool Queue::Restore(
    ID frontier,
    const DELIVERY* found
) {
    if (Compare(frontier, frontier_) != 0) return false;
    if (found && Compare(found->id, frontier_) <= 0) return false;
    if (found && !Find(found->id)) {
        ENTRY* entry = Reserve(true);
        if (!entry) return false;
        entry->delivery = *found;
        entry->stored = true;
        entry->data_finished = true;
    }
    disk_checked_ = true;
    return true;
}

// Público:
void Queue::ReaderFailed(
) {
    disk_checked_ = true;
    Add(statistics_.storage_failures);
}

// Público:
STATISTICS Queue::GetStatistics(
) const { return statistics_; }

} // namespace obc_delivery
