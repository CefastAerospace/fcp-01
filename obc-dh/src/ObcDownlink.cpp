#include "ObcDownlink.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcDownlink.h".
 */

namespace obc_downlink {
namespace {

    // Traduz falhas do serviço sem confundir indisponibilidade com erro de I/O.
    ttc::LOG_ERROR LogError(
        obc_storage::STATUS status
    ) {
        using obc_storage::STATUS;
        switch (status) {
            case STATUS::BUSY: return ttc::LOG_BUSY;
            case STATUS::NOT_FOUND: return ttc::LOG_NOT_FOUND;
            case STATUS::NOT_INITIALIZED:
            case STATUS::INIT_FAILED:
            case STATUS::MOUNT_FAILED: return ttc::LOG_NO_MEDIA;
            default: return ttc::LOG_IO;
        }
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

bool MapAircraft(
    const obc_delivery::DELIVERY& delivery,
    ttc::AIRCRAFT_DATA& out,
    uint32_t& available
) {
    const auto& aircraft = delivery.aircraft;
    out = {};
    available = 0;
    if (aircraft.icao > 0xFFFFFFUL) return false;
    // A máscara explicita ao compilador o limite já validado, sem truncar entradas inválidas.
    std::snprintf(out.icao, sizeof(out.icao), "%06lx", static_cast<unsigned long>(aircraft.icao & 0xFFFFFFUL));
    const uint8_t position = obc_data::SNAPSHOT_LATITUDE | obc_data::SNAPSHOT_LONGITUDE;
    if ((aircraft.available_fields & position) == position) {
        available |= ttc::AC_POSITION;
        out.latitude = aircraft.latitude;
        out.longitude = aircraft.longitude;
    }
    if (aircraft.available_fields & obc_data::SNAPSHOT_ALTITUDE) {
        // A cópia persistida usa double; o contrato TT&C existente usa float.
        if (!std::isfinite(aircraft.altitude)
                || std::fabs(aircraft.altitude) > std::numeric_limits<float>::max()) return false;
        available |= ttc::AC_ALTITUDE;
        out.altitude = static_cast<float>(aircraft.altitude);
    }
    return true;
}

void BuildLogFailure(
    uint16_t sequence,
    const ttc::LOG_REQUEST& request,
    ttc::LOG_ERROR error,
    LOG_RESPONSE& out
) {
    out = {};
    out.sequence = sequence;
    out.request = request;
    out.length = ttc::BuildLogError(sequence, request.op,
        request.op == ttc::LOG_READ ? request.name : nullptr,
        request.offset, error, out.json, sizeof(out.json));
}

void BuildLogResponse(
    uint16_t sequence,
    const ttc::LOG_REQUEST& request,
    const obc_delivery::READER_IO& io,
    LOG_RESPONSE& out
) {
    BuildLogFailure(sequence, request, ttc::LOG_IO, out);
    if (!io.list || !io.read) {
        BuildLogFailure(sequence, request, ttc::LOG_NO_MEDIA, out);
        return;
    }
    if (request.op == ttc::LOG_READ) {
        uint8_t bytes[ttc::log_block_max];
        uint32_t total = 0;
        const auto result = io.read(io.context, request.name, request.offset,
            bytes, request.length, total);
        if (result.status != obc_storage::STATUS::OK) {
            const auto error = result.status == obc_storage::STATUS::INVALID_ARGUMENT
                && request.offset > total ? ttc::LOG_RANGE : LogError(result.status);
            BuildLogFailure(sequence, request, error, out);
            return;
        }
        const size_t length = ttc::BuildLogData(sequence, request.name, request.offset,
            total, bytes, result.transferred, out.json, sizeof(out.json));
        if (length) out.length = length;
        else BuildLogFailure(sequence, request, ttc::LOG_IO, out);
        return;
    }

    // Um candidato adicional distingue fim de arquivo de página cheia.
    ttc::LOG_ENTRY entries[4] = {};
    char after[obc_storage::filename_chars + 1];
    std::strcpy(after, request.name);
    size_t count = 0;
    bool more = false;
    for (;;) {
        obc_storage::FILE_INFO file = {};
        const auto result = io.list(io.context, after, file);
        if (result.status != obc_storage::STATUS::OK) {
            BuildLogFailure(sequence, request, LogError(result.status), out);
            return;
        }
        if (!file.name[0]) break;
        if (count == 4) {
            more = true;
            break;
        }
        // Arquivos incompatíveis não são truncados nem omitidos silenciosamente.
        if (std::strlen(file.name) >= ttc::log_name_len || std::strcmp(file.name, after) <= 0) return;
        std::strcpy(entries[count].name, file.name);
        entries[count].size = file.size;
        entries[count].open = file.active;
        std::strcpy(after, file.name);
        ++count;
    }
    size_t used = 0;
    const size_t length = ttc::BuildLogList(sequence, entries, count, more,
        out.json, sizeof(out.json), used);
    if (length && (!count || used)) out.length = length;
    else BuildLogFailure(sequence, request, ttc::LOG_IO, out);
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool LogMailbox::Submit(
    uint16_t sequence,
    const ttc::LOG_REQUEST& request,
    uint64_t now_ms
) {
    if (state_ != STATE::FREE) return false;
    response_ = {};
    response_.sequence = sequence;
    response_.request = request;
    submitted_ms_ = now_ms;
    state_ = STATE::PENDING;
    return true;
}

// Público:
bool LogMailbox::Begin(
    uint16_t& sequence,
    ttc::LOG_REQUEST& request
) {
    if (state_ != STATE::PENDING) return false;
    sequence = response_.sequence;
    request = response_.request;
    state_ = STATE::RUNNING;
    return true;
}

// Público:
void LogMailbox::Complete(
    const LOG_RESPONSE& response
) {
    if (state_ == STATE::ABANDONED) state_ = STATE::FREE;
    else if (state_ == STATE::RUNNING) {
        response_ = response;
        state_ = STATE::READY;
    }
}

// Público:
bool LogMailbox::Take(
    uint64_t now_ms,
    LOG_RESPONSE& out
) {
    if (state_ == STATE::FREE || state_ == STATE::ABANDONED) return false;
    if (now_ms >= submitted_ms_ && now_ms - submitted_ms_ >= log_timeout_ms) {
        out = response_;
        out.length = 0;
        state_ = state_ == STATE::RUNNING ? STATE::ABANDONED : STATE::FREE;
        return true;
    }
    if (state_ != STATE::READY) return false;
    out = response_;
    state_ = STATE::FREE;
    return true;
}

} // namespace obc_downlink
