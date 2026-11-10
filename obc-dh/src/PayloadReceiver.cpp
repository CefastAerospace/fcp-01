#include "PayloadReceiver.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "PayloadReceiver.h".
 */

namespace obc_payload {
namespace {

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Mantém estatísticas cumulativas sem rollover silencioso.
    void Increment(
        uint64_t& value
    ) {
        if (value != std::numeric_limits<uint64_t>::max()) ++value;
    }

    // Lê inteiros little-endian sem alinhamento ou casts de estruturas.
    uint64_t ReadUnsigned(
        const uint8_t* data,
        size_t width
    ) {
        uint64_t value = 0;
        for (size_t index = 0; index < width; ++index) {
            value |= static_cast<uint64_t>(data[index]) << (8 * index);
        }
        return value;
    }

    // Grava larguras explícitas; só é chamado com buffers já dimensionados.
    void WriteUnsigned(
        uint8_t* data,
        uint64_t value,
        size_t width
    ) {
        for (size_t index = 0; index < width; ++index) {
            data[index] = static_cast<uint8_t>(value >> (8 * index));
        }
    }

    // Restringe os espaços aos quatro permitidos pela sintaxe JSON.
    bool IsSpace(
        char value
    ) {
        return value == ' ' || value == '\t' || value == '\r' || value == '\n';
    }

    // Aceita os dígitos hexadecimais necessários ao ICAO e escapes Unicode.
    int HexDigit(
        char value
    ) {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    }

    // Confere número decimal completo, sem aceitar prefixos, hex, NaN ou inf.
    const char* NumberEnd(
        const char* data,
        const char* end
    ) {
        const char* cursor = data;
        if (cursor != end && *cursor == '-') ++cursor;
        if (cursor == end) return nullptr;
        if (*cursor == '0') ++cursor;
        else {
            if (*cursor < '1' || *cursor > '9') return nullptr;
            do { ++cursor; } while (cursor != end && *cursor >= '0' && *cursor <= '9');
        }
        if (cursor != end && *cursor == '.') {
            ++cursor;
            const char* first = cursor;
            while (cursor != end && *cursor >= '0' && *cursor <= '9') ++cursor;
            if (cursor == first) return nullptr;
        }
        if (cursor != end && (*cursor == 'e' || *cursor == 'E')) {
            ++cursor;
            if (cursor != end && (*cursor == '+' || *cursor == '-')) ++cursor;
            const char* first = cursor;
            while (cursor != end && *cursor >= '0' && *cursor <= '9') ++cursor;
            if (cursor == first) return nullptr;
        }
        return cursor;
    }

    // ArduinoJson aceita extensões como aspas simples; esta passagem confere
    // somente a gramática estrita, sem construir outra árvore ou copiar textos.
    bool JsonValue(
        const char*& cursor,
        const char* end,
        unsigned depth,
        bool& unsupported_nul
    );

    // Confere aspas, caracteres de controle e escapes antes da decodificação.
    bool JsonStringSyntax(
        const char*& cursor,
        const char* end,
        bool& unsupported_nul
    ) {
        if (cursor == end || *cursor++ != '"') return false;
        while (cursor != end) {
            const unsigned char value = static_cast<unsigned char>(*cursor++);
            if (value == '"') return true;
            if (value < 0x20) return false;
            if (value != '\\') continue;
            if (cursor == end) return false;
            const char escape = *cursor++;
            if (escape == 'u') {
                unsigned code_point = 0;
                for (unsigned digit = 0; digit < 4; ++digit) {
                    if (cursor == end) return false;
                    const int nibble = HexDigit(*cursor++);
                    if (nibble < 0) return false;
                    code_point = (code_point << 4) | static_cast<unsigned>(nibble);
                }
                // NUL é sintaticamente JSON válido, mas não cabe nos textos C
                // do modelo. Detectar antes que a biblioteca use strlen na chave.
                if (code_point == 0) unsupported_nul = true;
            } else if (escape != '"' && escape != '\\' && escape != '/'
                    && escape != 'b' && escape != 'f' && escape != 'n'
                    && escape != 'r' && escape != 't') return false;
        }
        return false;
    }

    // Valida um valor com profundidade limitada; objetos do contrato usam
    // somente três níveis. Arrays são sintaxe válida, mas não valores do modelo.
    bool JsonValue(
        const char*& cursor,
        const char* end,
        unsigned depth,
        bool& unsupported_nul
    ) {
        while (cursor != end && IsSpace(*cursor)) ++cursor;
        if (cursor == end) return false;
        if (*cursor == '"') return JsonStringSyntax(cursor, end, unsupported_nul);
        if (*cursor == '{' || *cursor == '[') {
            if (depth >= 4) return false;
            const bool object = *cursor++ == '{';
            const char closing = object ? '}' : ']';
            while (cursor != end && IsSpace(*cursor)) ++cursor;
            if (cursor != end && *cursor == closing) { ++cursor; return true; }
            for (;;) {
                if (object) {
                    if (!JsonStringSyntax(cursor, end, unsupported_nul)) return false;
                    while (cursor != end && IsSpace(*cursor)) ++cursor;
                    if (cursor == end || *cursor++ != ':') return false;
                }
                if (!JsonValue(cursor, end, depth + 1, unsupported_nul)) return false;
                while (cursor != end && IsSpace(*cursor)) ++cursor;
                if (cursor == end) return false;
                if (*cursor == closing) { ++cursor; return true; }
                if (*cursor++ != ',') return false;
                while (cursor != end && IsSpace(*cursor)) ++cursor;
            }
        }
        if (*cursor == '-' || (*cursor >= '0' && *cursor <= '9')) {
            const char* next = NumberEnd(cursor, end);
            if (next == nullptr) return false;
            cursor = next;
            return true;
        }
        for (const char* literal : {"true", "false", "null"}) {
            const size_t size = std::strlen(literal);
            if (static_cast<size_t>(end - cursor) >= size
                    && std::memcmp(cursor, literal, size) == 0) {
                cursor += size;
                return true;
            }
        }
        return false;
    }

    // ─── CLASSES ──────────────────────────────────────────────────────

    // Os nomes minúsculos são overrides exigidos pela API de ArduinoJson.
    // Conta também o prefixo alinhado; destruição do JsonDocument libera tudo.
    class LimitedAllocator : public ArduinoJson::Allocator {
    public:
        size_t used = 0;
        size_t peak = 0;

        // Público: reserva limitada, com alinhamento adequado a qualquer objeto.
        void* allocate(
            size_t size
        ) override {
            if (size > parser_memory_bytes - used
                    || sizeof(BLOCK) > parser_memory_bytes - used - size) return nullptr;
            BLOCK* block = static_cast<BLOCK*>(std::malloc(size + sizeof(BLOCK)));
            if (block == nullptr) return nullptr;
            block->size = size + sizeof(BLOCK);
            used += block->size;
            if (used > peak) peak = used;
            return block + 1;
        }

        // Público: usa o tamanho guardado, sem depender de malloc_usable_size.
        void deallocate(
            void* pointer
        ) override {
            if (pointer == nullptr) return;
            BLOCK* block = static_cast<BLOCK*>(pointer) - 1;
            used -= block->size;
            std::free(block);
        }

        // Público: falha mantém o bloco anterior, conforme contrato de realloc.
        void* reallocate(
            void* pointer,
            size_t size
        ) override {
            if (pointer == nullptr) return allocate(size);
            if (size == 0) { deallocate(pointer); return nullptr; }
            BLOCK* old_block = static_cast<BLOCK*>(pointer) - 1;
            const size_t remaining = used - old_block->size;
            if (size > parser_memory_bytes - remaining
                    || sizeof(BLOCK) > parser_memory_bytes - remaining - size) return nullptr;
            BLOCK* block = static_cast<BLOCK*>(std::realloc(old_block, size + sizeof(BLOCK)));
            if (block == nullptr) return nullptr;
            block->size = size + sizeof(BLOCK);
            used = remaining + block->size;
            if (used > peak) peak = used;
            return block + 1;
        }

    private:
        union BLOCK {
            size_t size;
            std::max_align_t alignment;
        };
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Recusa NUL embutido, que não tem representação nos modelos C existentes.
    bool IsText(
        JsonString text
    ) {
        return !text.isNull() && std::memchr(text.c_str(), '\0', text.size()) == nullptr;
    }

    // Copia texto completo; comprimento em bytes, incluindo espaço para NUL.
    STATUS CopyText(
        JsonString text,
        char* target,
        size_t capacity
    ) {
        if (!IsText(text)) return STATUS::INVALID_VALUE;
        if (text.size() >= capacity) return STATUS::LIMIT_EXCEEDED;
        std::memcpy(target, text.c_str(), text.size());
        target[text.size()] = '\0';
        return STATUS::READY;
    }

    // Interpreta exatamente seis hexadecimais, sem associação a ordem da lista.
    bool ReadIcao(
        JsonString text,
        uint32_t& value
    ) {
        if (!IsText(text) || text.size() != 6) return false;
        value = 0;
        for (size_t index = 0; index < 6; ++index) {
            const int digit = HexDigit(text.c_str()[index]);
            if (digit < 0) return false;
            value = (value << 4) | static_cast<unsigned>(digit);
        }
        return true;
    }

    // Exige o número inteiro seguido apenas do sufixo acordado; conserva o
    // texto no extra quando não há uma conversão válida, sem aproveitar prefixo.
    bool ReadNumber(
        JsonVariantConst value,
        const char* suffix,
        double& number
    ) {
        if (!value.is<const char*>()) return false;
        const JsonString text = value.as<JsonString>();
        if (!IsText(text)) return false;
        const char* end = text.c_str() + text.size();
        const char* number_end = NumberEnd(text.c_str(), end);
        if (number_end == nullptr || std::strcmp(number_end, suffix) != 0) return false;
        errno = 0;
        char* converted_end = nullptr;
        number = std::strtod(text.c_str(), &converted_end);
        return converted_end == number_end && errno != ERANGE && std::isfinite(number);
    }

    // Adiciona par textual sem truncamento ou conversão implícita de tipos.
    STATUS AddExtra(
        obc_data::AIRCRAFT_STATE& aircraft,
        JsonString key,
        JsonVariantConst value
    ) {
        if (!value.is<const char*>()) return STATUS::INVALID_VALUE;
        if (aircraft.extra_count >= obc_data::max_aircraft_extras) return STATUS::LIMIT_EXCEEDED;
        obc_data::EXTRA_FIELD& extra = aircraft.extras[aircraft.extra_count];
        STATUS status = CopyText(key, extra.key, sizeof(extra.key));
        if (status != STATUS::READY) return status;
        status = CopyText(value.as<JsonString>(), extra.value, sizeof(extra.value));
        if (status == STATUS::READY) ++aircraft.extra_count;
        return status;
    }

    // Converte uma aeronave. O mesmo helper serve às duas passagens, sem
    // alocar ou modificar o documento, garantindo publicação após preflight.
    STATUS ReadAircraft(
        JsonString key,
        JsonObjectConst object,
        obc_data::AIRCRAFT_STATE& aircraft
    ) {
        std::memset(&aircraft, 0, sizeof(aircraft));
        if (object.isNull() || !ReadIcao(key, aircraft.icao)) return STATUS::INVALID_VALUE;
        double latitude = 0;
        double longitude = 0;
        const bool position = ReadNumber(object["Latitude"], "", latitude)
            && ReadNumber(object["Longitude"], "", longitude)
            && latitude >= -90 && latitude <= 90 && longitude >= -180 && longitude <= 180;
        if (position) {
            aircraft.latitude = latitude;
            aircraft.longitude = longitude;
            aircraft.available_fields |= obc_data::SNAPSHOT_LATITUDE | obc_data::SNAPSHOT_LONGITUDE;
        }
        for (JsonPairConst pair : object) {
            const JsonString field = pair.key();
            if (!IsText(field) || field.size() == 0) return STATUS::INVALID_VALUE;
            if (field.size() > obc_data::extra_key_chars) return STATUS::LIMIT_EXCEEDED;
            const char* name = field.c_str();
            const JsonVariantConst value = pair.value();
            const bool icao = std::strcmp(name, "ICAO Address") == 0;
            const bool identification = std::strcmp(name, "Identification") == 0;
            const bool tick = std::strcmp(name, "tick") == 0;
            const bool coordinates = std::strcmp(name, "Latitude") == 0 || std::strcmp(name, "Longitude") == 0;
            const bool altitude = std::strcmp(name, "Altitude") == 0;
            if (icao || identification || tick || coordinates || altitude) {
                if (value.isNull()) continue;
                if (!value.is<const char*>() || !IsText(value.as<JsonString>())) return STATUS::INVALID_VALUE;
                if (icao) {
                    uint32_t attribute = 0;
                    if (!ReadIcao(value.as<JsonString>(), attribute) || attribute != aircraft.icao) return STATUS::INVALID_VALUE;
                    continue;
                }
                if (identification || tick) {
                    const STATUS status = identification
                        ? CopyText(value.as<JsonString>(), aircraft.identification, sizeof(aircraft.identification))
                        : CopyText(value.as<JsonString>(), aircraft.tick, sizeof(aircraft.tick));
                    if (status != STATUS::READY) return status;
                    aircraft.available_fields |= identification ? obc_data::SNAPSHOT_IDENTIFICATION : obc_data::SNAPSHOT_TICK;
                    continue;
                }
                if (coordinates && position) continue;
                if (altitude && ReadNumber(value, " feet", aircraft.altitude)) {
                    aircraft.available_fields |= obc_data::SNAPSHOT_ALTITUDE;
                    continue;
                }
            }
            const STATUS status = AddExtra(aircraft, field, value);
            if (status != STATUS::READY) return status;
        }
        return obc_data::Validate(aircraft) == obc_data::STATUS::OK ? STATUS::READY : STATUS::INVALID_VALUE;
    }

} // namespace

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool Receiver::Init(
    uint64_t boot_id
) {
    if (boot_id == 0) return false;
    if (!frame_) {
        // Duas reservas limitadas permitem usar bancos separados da RAM.
        // Falha na segunda libera a primeira, sem inicialização parcial.
        std::unique_ptr<uint8_t[]> frame(new (std::nothrow) uint8_t[max_frame_bytes]);
        if (!frame) return false;
        std::unique_ptr<obc_data::PAYLOAD_SNAPSHOT> snapshot(
            new (std::nothrow) obc_data::PAYLOAD_SNAPSHOT{});
        if (!snapshot) return false;
        frame_.swap(frame);
        snapshot_.swap(snapshot);
    }
    boot_id_ = boot_id;
    received_ = 0;
    last_now_ms_ = 0;
    session_ = 0;
    sequence_ = 0;
    accepted_crc_ = 0;
    missing_high_ = 0;
    published_ = false;
    statistics_ = {};
    return true;
}

// Público:
RESULT Receiver::Feed(
    const uint8_t* data,
    size_t size,
    uint64_t now_ms
) {
    RESULT result = {};
    result.status = CheckTimeout(now_ms);
    if (result.status != STATUS::WAITING) return result;
    if (data == nullptr && size != 0) { result.status = STATUS::INVALID_ARGUMENT; return result; }
    for (;;) {
        result.status = Examine(now_ms, result);
        if (result.status != STATUS::WAITING || result.consumed == size) return result;
        if (received_ == 0) started_ms_ = now_ms;
        frame_[received_++] = data[result.consumed++];
        last_byte_ms_ = now_ms;
    }
}

// Privado:
STATUS Receiver::Examine(
    uint64_t now_ms,
    RESULT& result
) {
    if (received_ < 4) return STATUS::WAITING;
    if (std::memcmp(frame_.get(), "FCPU", 4) != 0) {
        Resynchronize(now_ms);
        Increment(statistics_.rejected);
        return STATUS::INVALID_FRAME;
    }
    if (received_ < header_bytes) return STATUS::WAITING;
    const uint64_t session = ReadUnsigned(frame_.get() + 6, 8);
    const uint32_t sequence = static_cast<uint32_t>(ReadUnsigned(frame_.get() + 14, 4));
    const size_t length = static_cast<size_t>(ReadUnsigned(frame_.get() + 18, 4));
    if (frame_[4] != 1 || frame_[5] != static_cast<uint8_t>(FRAME_TYPE::DATA)
            || session == 0 || sequence == 0 || length > max_json_bytes) {
        Resynchronize(now_ms);
        Increment(statistics_.rejected);
        return length > max_json_bytes ? STATUS::LIMIT_EXCEEDED : STATUS::INVALID_FRAME;
    }
    const size_t total = header_bytes + length + crc_bytes;
    if (received_ < total) return STATUS::WAITING;
    Increment(statistics_.complete_candidates);
    const uint32_t crc = static_cast<uint32_t>(ReadUnsigned(frame_.get() + total - crc_bytes, crc_bytes));
    if (crc != obc_record::ComputeCrc32(frame_.get(), total - crc_bytes)) {
        Resynchronize(now_ms);
        Increment(statistics_.rejected);
        return STATUS::INVALID_FRAME;
    }

    // Apenas o último quadro aceito pode ser repetido no stop-and-wait.
    STATUS status = STATUS::WAITING;
    if (session_ == session && sequence_ == sequence && accepted_crc_ == crc) {
        Increment(statistics_.duplicates);
        status = STATUS::DUPLICATE;
    } else if (session_ != 0 && ((session != session_ && sequence != 1)
            || (session == session_ && (sequence_ == UINT32_MAX || sequence != sequence_ + 1)))) {
        if (session == session_ && sequence_ != UINT32_MAX && sequence > sequence_ + 1) {
            Increment(statistics_.gaps);
            const uint64_t expected = static_cast<uint64_t>(sequence_) + 1;
            const uint64_t unseen = static_cast<uint64_t>(missing_high_) + 1;
            const uint64_t begin = expected > unseen ? expected : unseen;
            if (sequence > begin) {
                const uint64_t count = sequence - begin;
                const uint64_t maximum = std::numeric_limits<uint64_t>::max();
                statistics_.missing_positions = maximum - statistics_.missing_positions < count
                    ? maximum : statistics_.missing_positions + count;
            }
            if (sequence - 1 > missing_high_) missing_high_ = sequence - 1;
        }
        status = STATUS::SEQUENCE_ERROR;
    } else {
        status = ParseSnapshot(length, now_ms);
        if (status == STATUS::READY) {
            if (session_ != session) {
                if (session_ != 0) Increment(statistics_.session_changes);
                missing_high_ = sequence;
            }
            session_ = session;
            sequence_ = sequence;
            accepted_crc_ = crc;
            published_ = true;
            Increment(statistics_.accepted);
        }
    }
    if (status != STATUS::READY && status != STATUS::DUPLICATE) Increment(statistics_.rejected);
    MakeReply(result, session, sequence, status);
    received_ -= total;
    std::memmove(frame_.get(), frame_.get() + total, received_);
    started_ms_ = now_ms;
    return status;
}

// Privado:
void Receiver::Resynchronize(
    uint64_t now_ms
) {
    size_t offset = 1;
    for (; offset < received_; ++offset) {
        const size_t remaining = received_ - offset;
        const size_t prefix = remaining < 4 ? remaining : 4;
        if (std::memcmp(frame_.get() + offset, "FCPU", prefix) == 0) break;
    }
    received_ -= offset;
    std::memmove(frame_.get(), frame_.get() + offset, received_);
    started_ms_ = now_ms;
}

// Privado:
STATUS Receiver::ParseSnapshot(
    size_t length,
    uint64_t now_ms
) {
    const char* start = reinterpret_cast<const char*>(frame_.get() + header_bytes);
    const char* end = start + length;
    const char* cursor = start;
    bool unsupported_nul = false;
    if (!JsonValue(cursor, end, 0, unsupported_nul)) return STATUS::INVALID_JSON;
    while (cursor != end && IsSpace(*cursor)) ++cursor;
    if (cursor != end) return STATUS::INVALID_JSON;
    if (unsupported_nul) return STATUS::INVALID_VALUE;

    LimitedAllocator allocator;
    JsonDocument document(&allocator);
    const DeserializationError error = deserializeJson(document,
        reinterpret_cast<char*>(frame_.get() + header_bytes), length,
        DeserializationOption::NestingLimit(4));
    if (allocator.peak > statistics_.parser_peak_bytes) statistics_.parser_peak_bytes = allocator.peak;
    if (error) return error == DeserializationError::NoMemory ? STATUS::NO_MEMORY : STATUS::INVALID_JSON;
    if (!document.is<JsonObject>()) return STATUS::INVALID_VALUE;
    const JsonObjectConst root = document.as<JsonObjectConst>();
    if (root.size() != 2 || !root["id"].is<uint64_t>() || !root["messages"].is<JsonObjectConst>()) return STATUS::INVALID_VALUE;
    const JsonObjectConst messages = root["messages"].as<JsonObjectConst>();
    if (messages.size() > obc_data::max_snapshot_aircraft) return STATUS::LIMIT_EXCEEDED;

    obc_data::AIRCRAFT_STATE temporary;
    uint32_t icaos[obc_data::max_snapshot_aircraft] = {};
    size_t count = 0;
    for (JsonPairConst pair : messages) {
        if (!pair.value().is<JsonObjectConst>()) return STATUS::INVALID_VALUE;
        const STATUS status = ReadAircraft(pair.key(), pair.value().as<JsonObjectConst>(), temporary);
        if (status != STATUS::READY) return status;
        for (size_t index = 0; index < count; ++index) {
            if (icaos[index] == temporary.icao) return STATUS::INVALID_VALUE;
        }
        icaos[count++] = temporary.icao;
    }

    // Documento estável, sem I/O, concorrência ou alocação na publicação.
    // A segunda passagem repete exatamente as conversões já verificadas.
    std::memset(snapshot_.get(), 0, sizeof(*snapshot_));
    snapshot_->id = root["id"].as<uint64_t>();
    snapshot_->stamp = {boot_id_, now_ms};
    snapshot_->aircraft_count = static_cast<uint8_t>(count);
    size_t index = 0;
    for (JsonPairConst pair : messages) {
        ReadAircraft(pair.key(), pair.value().as<JsonObjectConst>(), snapshot_->aircraft[index++]);
    }
    return STATUS::READY;
}

// Privado:
void Receiver::MakeReply(
    RESULT& result,
    uint64_t session,
    uint32_t sequence,
    STATUS status
) {
    const bool ack = status == STATUS::READY || status == STATUS::DUPLICATE;
    std::memcpy(result.reply, "FCPU", 4);
    result.reply[4] = 1;
    result.reply[5] = static_cast<uint8_t>(ack ? FRAME_TYPE::ACK : FRAME_TYPE::NACK);
    WriteUnsigned(result.reply + 6, session, 8);
    WriteUnsigned(result.reply + 14, sequence, 4);
    WriteUnsigned(result.reply + 18, ack ? 0 : 5, 4);
    if (!ack) {
        NACK_REASON reason = NACK_REASON::INVALID_VALUE;
        if (status == STATUS::INVALID_JSON) reason = NACK_REASON::INVALID_JSON;
        else if (status == STATUS::LIMIT_EXCEEDED) reason = NACK_REASON::LIMIT_EXCEEDED;
        else if (status == STATUS::NO_MEMORY) reason = NACK_REASON::NO_MEMORY;
        else if (status == STATUS::SEQUENCE_ERROR) reason = NACK_REASON::SEQUENCE_ERROR;
        result.reply[header_bytes] = static_cast<uint8_t>(reason);
        const uint32_t expected = session_ == 0 ? sequence
            : session != session_ ? 1 : sequence_ == UINT32_MAX ? 0 : sequence_ + 1;
        WriteUnsigned(result.reply + header_bytes + 1, expected, 4);
    }
    result.reply_size = header_bytes + (ack ? 0 : 5) + crc_bytes;
    WriteUnsigned(result.reply + result.reply_size - crc_bytes,
        obc_record::ComputeCrc32(result.reply, result.reply_size - crc_bytes), crc_bytes);
}

// Público:
STATUS Receiver::CheckTimeout(
    uint64_t now_ms
) {
    if (boot_id_ == 0) return STATUS::NOT_INITIALIZED;
    if (now_ms < last_now_ms_) return STATUS::INVALID_ARGUMENT;
    last_now_ms_ = now_ms;
    if (received_ != 0 && (now_ms - last_byte_ms_ >= inter_byte_timeout_ms
            || now_ms - started_ms_ >= frame_timeout_ms)) {
        received_ = 0;
        Increment(statistics_.timeouts);
        Increment(statistics_.rejected);
        return STATUS::TIMEOUT;
    }
    return STATUS::WAITING;
}

// ─── GETTERS ──────────────────────────────────────────────────────

// Público:
const obc_data::PAYLOAD_SNAPSHOT* Receiver::GetSnapshot(
) const {
    return published_ ? snapshot_.get() : nullptr;
}

// Público:
const STATISTICS& Receiver::GetStatistics(
) const {
    return statistics_;
}

} // namespace obc_payload
