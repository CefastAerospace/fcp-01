#include "TtcProtocol.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "TtcProtocol.h".
 */

namespace ttc {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    struct COMMAND_ENTRY {
        COMMAND_ID  id;
        const char* name;
    };

    static constexpr COMMAND_ENTRY command_table[] = {
        {CMD_PING,              "ping"},
        {CMD_SET_MODE,          "set_mode"},
        {CMD_START_MISSION,     "start_mission"},
        {CMD_STOP_MISSION,      "stop_mission"},
        {CMD_GET_STATUS,        "get_status"},
        {CMD_ENTER_SAFE,        "enter_safe"},
        {CMD_EXIT_SAFE,         "exit_safe"},
        {CMD_REQUEST_TELEMETRY, "request_tm"},
        {CMD_SET_TIME,          "set_time"},
        {CMD_ADCS_START,        "adcs_start"},
        {CMD_ADCS_STOP,         "adcs_stop"},
        {CMD_DEPLOY_ANTENNA,    "deploy_antenna"},
        {CMD_REQUEST_LOG,       "request_log"},
        {CMD_TEST_TM,           "test_tm"},
        {CMD_RESET_OBC,         "reset_obc"}
    };

    // Arredonda para o número de casas exibido na TM; evita dígitos
    // espúrios da conversão float -> texto e limita o tamanho do quadro.
    static double Round(
        double value,
        int decimals
    ) {
        double scale = 1.0;
        for (int i = 0; i < decimals; i++) scale *= 10.0;
        return round(value * scale) / scale;
    }

    // Serializa o documento se couber em max_body_len e em capacity.
    static size_t Serialize(
        const JsonDocument& doc,
        char* out,
        size_t capacity
    ) {
        if (out == nullptr || capacity == 0) return 0;

        size_t needed = measureJson(doc);
        if (needed > max_body_len || needed + 1 > capacity) {
            out[0] = '\0';
            return 0;
        }
        return serializeJson(doc, out, capacity);
    }

    // Componentes de ID maiores que zero, como no diário FCD1 do OBC.
    static bool ValidId(
        const DELIVERY_ID& id
    ) {
        return id.boot > 0 && id.sequence > 0;
    }

    // ICAO de 6 hexadecimais.
    static bool ValidIcao(
        const char* icao
    ) {
        if (icao == nullptr) return false;
        for (size_t i = 0; i < 6; i++) {
            char c = icao[i];
            bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
            if (!hex) return false;
        }
        return icao[6] == '\0';
    }

    // Nome de arquivo aceito pelo request_log: sem diretórios nem "..".
    static bool ValidLogName(
        const char* name,
        bool allow_empty
    ) {
        if (name == nullptr) return false;
        size_t len = strlen(name);
        if (len == 0) return allow_empty;
        if (len >= log_name_len || name[0] == '.' || strstr(name, "..") != nullptr) return false;
        for (size_t i = 0; i < len; i++) {
            char c = name[i];
            bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                   || c == '_' || c == '.' || c == '-';
            if (!ok) return false;
        }
        return true;
    }

    // Base64 padrão (RFC 4648), com preenchimento. out recebe '\0'.
    static bool EncodeBase64(
        const uint8_t* bytes,
        size_t count,
        char* out,
        size_t capacity
    ) {
        static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        size_t needed = ((count + 2) / 3) * 4;
        if (needed + 1 > capacity) return false;
        size_t o = 0;
        for (size_t i = 0; i < count; i += 3) {
            uint32_t chunk = (uint32_t)bytes[i] << 16;
            if (i + 1 < count) chunk |= (uint32_t)bytes[i + 1] << 8;
            if (i + 2 < count) chunk |= bytes[i + 2];
            out[o++] = alphabet[(chunk >> 18) & 0x3F];
            out[o++] = alphabet[(chunk >> 12) & 0x3F];
            out[o++] = i + 1 < count ? alphabet[(chunk >> 6) & 0x3F] : '=';
            out[o++] = i + 2 < count ? alphabet[chunk & 0x3F] : '=';
        }
        out[o] = '\0';
        return true;
    }

    // ─── HELPERS ──────────────────────────────────────────────────────

    const char* CommandName(
        COMMAND_ID cmd
    ) {
        for (const COMMAND_ENTRY& entry : command_table) {
            if (entry.id == cmd) return entry.name;
        }
        return "unknown";
    }

    COMMAND_ID CommandFromName(
        const char* name
    ) {
        if (name == nullptr) return CMD_UNKNOWN;
        for (const COMMAND_ENTRY& entry : command_table) {
            if (strcmp(entry.name, name) == 0) return entry.id;
        }
        return CMD_UNKNOWN;
    }

    const char* StatusName(
        ACK_STATUS status
    ) {
        switch (status) {
            case ACK_OK:             return "ok";
            case ERR_BAD_JSON:       return "bad_json";
            case ERR_INVALID_CMD:    return "invalid_cmd";
            case ERR_INVALID_PARAM:  return "invalid_param";
            case ERR_STATE_REJECTED: return "state_rejected";
            case ERR_UNKNOWN_CMD:    return "unknown_cmd";
        }
        return "unknown";
    }

    const char* ModeName(
        OP_MODE mode
    ) {
        switch (mode) {
            case MODE_BENCH:        return "bench";
            case MODE_SAFE:         return "safe";
            case MODE_DETUMBLING:   return "detumb";
            case MODE_SUN_POINTING: return "sun";
            case MODE_MISSION:      return "mission";
            case MODE_LOW_POWER:    return "lowpwr";
        }
        return "unknown";
    }

    bool ParseTelecommand(
        const char* json,
        size_t len,
        TC_INFO& out
    ) {
        out = {};
        out.cmd = CMD_UNKNOWN;
        out.status = ERR_BAD_JSON;
        if (json == nullptr || len == 0 || len > max_frame_len) return false;

        JsonDocument doc;
        if (deserializeJson(doc, json, len) != DeserializationError::Ok) return false;
        if (!doc.is<JsonObject>()) return false;

        // seq antes do tipo: com seq legível, até um envelope ruim recebe NACK.
        if (doc["seq"].is<uint16_t>()) {
            out.has_seq = true;
            out.seq = doc["seq"].as<uint16_t>();
        }
        if (!out.has_seq || doc["t"] != "tc") return false;

        const char* name = doc["cmd"];
        if (name == nullptr) return false;
        strncpy(out.cmd_name, name, cmd_name_len - 1);
        out.cmd_name[cmd_name_len - 1] = '\0';

        // args, se presente, precisa ser objeto; o conteúdo é do OBC.
        if (!doc["args"].isNull() && !doc["args"].is<JsonObject>()) {
            out.status = ERR_INVALID_PARAM;
            return false;
        }

        out.cmd = CommandFromName(name);
        if (out.cmd == CMD_UNKNOWN) {
            out.status = ERR_UNKNOWN_CMD;
            return false;
        }

        out.status = ACK_OK;
        return true;
    }

    size_t BuildAck(
        uint16_t seq,
        const char* cmd_name,
        ACK_STATUS status,
        int32_t queue_ms,
        char* out,
        size_t capacity
    ) {
        JsonDocument doc;
        doc["t"] = "ack";
        doc["seq"] = seq;
        doc["cmd"] = cmd_name != nullptr ? cmd_name : "unknown";
        doc["st"] = StatusName(status);
        if (queue_ms >= 0) doc["q"] = queue_ms;
        return Serialize(doc, out, capacity);
    }

    size_t BuildTelemetry(
        const TELEMETRY_DATA& tm,
        char* out,
        size_t capacity,
        uint32_t available
    ) {
        JsonDocument doc;
        doc["t"] = "tm";
        doc["ts"] = tm.timestamp;
        doc["md"] = ModeName(tm.mode);

        // Energia.
        doc["vb"] = Round(tm.vbat, 2);
        doc["ib"] = Round(tm.ibat, 1);
        doc["vs"] = Round(tm.vsolar, 2);
        doc["is"] = Round(tm.isolar, 1);

        // Controle térmico.
        JsonArray temps = doc["tp"].to<JsonArray>();
        for (size_t i = 0; i < thermal_sensors; i++) temps.add(Round(tm.temps[i], 1));

        // Atitude e missão.
        doc["w"] = Round(tm.rate_dps, 1);
        doc["az"] = Round(tm.azimuth_deg, 1);
        doc["ac"] = tm.aircraft_count;
        doc["msg"] = tm.message_count;

        // Enlace.
        doc["rssi"] = tm.rssi;
        doc["snr"] = Round(tm.snr, 1);
        doc["rx"] = tm.rx_count;
        doc["err"] = tm.error_count;
        doc["rst"] = tm.radio_resets;

        // Conserva as chaves e distingue indisponibilidade de zero medido.
        if (!(available & TM_TIMESTAMP)) doc["ts"] = nullptr;
        if (!(available & TM_MODE)) doc["md"] = nullptr;
        if (!(available & TM_VBAT)) doc["vb"] = nullptr;
        if (!(available & TM_IBAT)) doc["ib"] = nullptr;
        if (!(available & TM_VSOLAR)) doc["vs"] = nullptr;
        if (!(available & TM_ISOLAR)) doc["is"] = nullptr;
        if (!(available & TM_TEMPS)) doc["tp"] = nullptr;
        if (!(available & TM_RATE)) doc["w"] = nullptr;
        if (!(available & TM_AZIMUTH)) doc["az"] = nullptr;
        if (!(available & TM_AIRCRAFT)) doc["ac"] = nullptr;
        if (!(available & TM_MESSAGES)) doc["msg"] = nullptr;
        if (!(available & TM_RSSI)) doc["rssi"] = nullptr;
        if (!(available & TM_SNR)) doc["snr"] = nullptr;
        if (!(available & TM_RX_COUNT)) doc["rx"] = nullptr;
        if (!(available & TM_ERROR_COUNT)) doc["err"] = nullptr;
        if (!(available & TM_RADIO_RESETS)) doc["rst"] = nullptr;

        return Serialize(doc, out, capacity);
    }

    size_t BuildAircraft(
        const AIRCRAFT_DATA& aircraft,
        char* out,
        size_t capacity,
        uint32_t available,
        const DELIVERY_ID* id
    ) {
        if (out != nullptr && capacity > 0) out[0] = '\0';
        if (!ValidIcao(aircraft.icao)) return 0;
        if (id != nullptr && !ValidId(*id)) return 0;

        JsonDocument doc;
        doc["t"] = "ac";
        if (id != nullptr) {
            JsonArray pair = doc["id"].to<JsonArray>();
            pair.add(id->boot);
            pair.add(id->sequence);
        }
        doc["ts"] = aircraft.timestamp;
        doc["icao"] = aircraft.icao;
        doc["lat"] = Round(aircraft.latitude, 5);
        doc["lon"] = Round(aircraft.longitude, 5);
        doc["alt"] = Round(aircraft.altitude, 0);
        doc["vel"] = Round(aircraft.velocity, 1);
        doc["hdg"] = Round(aircraft.heading, 1);
        doc["cnt"] = aircraft.message_count;

        // Conserva as chaves e distingue indisponibilidade de zero medido.
        if (!(available & AC_TIMESTAMP)) doc["ts"] = nullptr;
        if (!(available & AC_POSITION)) {
            doc["lat"] = nullptr;
            doc["lon"] = nullptr;
        }
        if (!(available & AC_ALTITUDE)) doc["alt"] = nullptr;
        if (!(available & AC_VELOCITY)) doc["vel"] = nullptr;
        if (!(available & AC_HEADING)) doc["hdg"] = nullptr;
        if (!(available & AC_COUNT)) doc["cnt"] = nullptr;

        return Serialize(doc, out, capacity);
    }

    size_t BuildConfirmation(
        const DELIVERY_ID& id,
        char* out,
        size_t capacity
    ) {
        if (out != nullptr && capacity > 0) out[0] = '\0';
        if (!ValidId(id)) return 0;

        JsonDocument doc;
        doc["t"] = "cf";
        JsonArray pair = doc["id"].to<JsonArray>();
        pair.add(id.boot);
        pair.add(id.sequence);
        return Serialize(doc, out, capacity);
    }

    CONFIRMATION_PARSE ParseConfirmation(
        const char* json,
        size_t len,
        DELIVERY_ID& out
    ) {
        out = {};
        if (json == nullptr || len == 0 || len > max_frame_len) return CF_NOT_CONFIRMATION;

        JsonDocument doc;
        if (deserializeJson(doc, json, len) != DeserializationError::Ok) return CF_NOT_CONFIRMATION;
        if (!doc.is<JsonObject>() || doc["t"] != "cf") return CF_NOT_CONFIRMATION;

        // Somente "t" e "id"; o ID é um par de inteiros positivos de 64 bits.
        JsonObject object = doc.as<JsonObject>();
        JsonArray pair = object["id"];
        if (object.size() != 2 || pair.isNull() || pair.size() != 2) return CF_INVALID;
        if (!pair[0].is<uint64_t>() || !pair[1].is<uint64_t>()) return CF_INVALID;

        DELIVERY_ID id = {pair[0].as<uint64_t>(), pair[1].as<uint64_t>()};
        if (!ValidId(id)) return CF_INVALID;
        out = id;
        return CF_VALID;
    }

    const char* LogErrorName(
        LOG_ERROR error
    ) {
        switch (error) {
            case LOG_NOT_FOUND: return "not_found";
            case LOG_BUSY:      return "busy";
            case LOG_RANGE:     return "range";
            case LOG_IO:        return "io";
            case LOG_NO_MEDIA:  return "no_sd";
        }
        return "unknown";
    }

    ACK_STATUS ParseLogRequest(
        const char* json,
        size_t len,
        LOG_REQUEST& out
    ) {
        out = {};
        out.op = LOG_LIST;
        out.dest = LOG_DEST_ORIGIN;
        if (json == nullptr || len == 0 || len > max_frame_len) return ERR_BAD_JSON;

        JsonDocument doc;
        if (deserializeJson(doc, json, len) != DeserializationError::Ok) return ERR_BAD_JSON;
        JsonObject args = doc["args"];
        if (args.isNull()) return ERR_INVALID_PARAM;

        const char* op = args["op"];
        if (op == nullptr) return ERR_INVALID_PARAM;
        bool read = strcmp(op, "read") == 0;
        if (!read && strcmp(op, "list") != 0) return ERR_INVALID_PARAM;
        out.op = read ? LOG_READ : LOG_LIST;

        // Rejeita chaves desconhecidas: erro de digitação não vira pedido diferente.
        for (JsonPair pair : args) {
            const char* key = pair.key().c_str();
            bool known = strcmp(key, "op") == 0 || strcmp(key, "to") == 0
                      || (!read && strcmp(key, "after") == 0)
                      || (read && (strcmp(key, "name") == 0 || strcmp(key, "off") == 0 || strcmp(key, "len") == 0));
            if (!known) return ERR_INVALID_PARAM;
        }

        if (!args["to"].isNull()) {
            const char* to = args["to"];
            if (to == nullptr) return ERR_INVALID_PARAM;
            if (strcmp(to, "rf") == 0) out.dest = LOG_DEST_RF;
            else if (strcmp(to, "wired") == 0) out.dest = LOG_DEST_WIRED;
            else return ERR_INVALID_PARAM;
        }

        if (!read) {
            if (args["after"].isNull()) return ACK_OK;
            const char* after = args["after"];
            if (!ValidLogName(after, true)) return ERR_INVALID_PARAM;
            strncpy(out.name, after, log_name_len - 1);
            return ACK_OK;
        }

        const char* name = args["name"];
        if (!ValidLogName(name, false)) return ERR_INVALID_PARAM;
        if (!args["off"].is<uint32_t>() || !args["len"].is<uint16_t>()) return ERR_INVALID_PARAM;
        uint16_t length = args["len"].as<uint16_t>();
        if (length == 0 || length > log_block_max) return ERR_INVALID_PARAM;
        strncpy(out.name, name, log_name_len - 1);
        out.offset = args["off"].as<uint32_t>();
        out.length = length;
        return ACK_OK;
    }

    size_t BuildLogList(
        uint16_t request_seq,
        const LOG_ENTRY* entries,
        size_t count,
        bool more,
        char* out,
        size_t capacity,
        size_t& used
    ) {
        used = 0;
        if (out != nullptr && capacity > 0) out[0] = '\0';
        if (out == nullptr || capacity == 0 || (count > 0 && entries == nullptr)) return 0;
        size_t limit = capacity - 1 < max_body_len ? capacity - 1 : max_body_len;

        JsonDocument doc;
        doc["t"] = "lg";
        doc["rq"] = request_seq;
        doc["op"] = "list";
        JsonArray list = doc["ls"].to<JsonArray>();
        doc["end"] = false; // Forma mais longa durante a medição.
        if (measureJson(doc) > limit) return 0;

        for (size_t i = 0; i < count; i++) {
            if (!ValidLogName(entries[i].name, false)) break;
            JsonArray item = list.add<JsonArray>();
            item.add(entries[i].name);
            item.add(entries[i].size);
            item.add(entries[i].open ? 1 : 0);
            if (measureJson(doc) > limit) {
                list.remove(list.size() - 1);
                break;
            }
            used++;
        }
        doc["end"] = used == count && !more;
        return Serialize(doc, out, capacity);
    }

    size_t BuildLogData(
        uint16_t request_seq,
        const char* name,
        uint32_t offset,
        uint32_t total,
        const uint8_t* bytes,
        size_t count,
        char* out,
        size_t capacity
    ) {
        if (out != nullptr && capacity > 0) out[0] = '\0';
        if (!ValidLogName(name, false) || count > log_block_max) return 0;
        if (count > 0 && bytes == nullptr) return 0;
        if (offset > total || count > (size_t)(total - offset)) return 0;

        char encoded[((log_block_max + 2) / 3) * 4 + 1];
        if (!EncodeBase64(bytes, count, encoded, sizeof(encoded))) return 0;

        JsonDocument doc;
        doc["t"] = "lg";
        doc["rq"] = request_seq;
        doc["op"] = "read";
        doc["name"] = name;
        doc["off"] = offset;
        doc["tot"] = total;
        doc["d"] = (const char*)encoded;
        return Serialize(doc, out, capacity);
    }

    size_t BuildLogError(
        uint16_t request_seq,
        LOG_OP op,
        const char* name,
        uint32_t offset,
        LOG_ERROR error,
        char* out,
        size_t capacity
    ) {
        if (out != nullptr && capacity > 0) out[0] = '\0';
        if (name != nullptr && !ValidLogName(name, false)) return 0;

        JsonDocument doc;
        doc["t"] = "lg";
        doc["rq"] = request_seq;
        doc["op"] = op == LOG_READ ? "read" : "list";
        if (name != nullptr) {
            doc["name"] = name;
            doc["off"] = offset;
        }
        doc["err"] = LogErrorName(error);
        return Serialize(doc, out, capacity);
    }

    size_t BuildEvent(
        const char* event,
        uint32_t timestamp,
        char* out,
        size_t capacity
    ) {
        JsonDocument doc;
        doc["t"] = "ev";
        doc["ts"] = timestamp;
        doc["ev"] = event != nullptr ? event : "unknown";
        return Serialize(doc, out, capacity);
    }

    size_t BuildBeacon(
        uint32_t timestamp,
        OP_MODE mode,
        float vbat,
        char* out,
        size_t capacity
    ) {
        JsonDocument doc;
        doc["t"] = "bcn";
        doc["ts"] = timestamp;
        doc["md"] = ModeName(mode);
        doc["vb"] = Round(vbat, 2);
        return Serialize(doc, out, capacity);
    }

    bool InsertCounter(
        char* json,
        size_t& len,
        size_t capacity,
        uint16_t counter
    ) {
        if (json == nullptr || len < 2 || json[0] != '{') return false;

        // Campo "n" seguido de vírgula, exceto em objeto vazio.
        char field[max_counter_len + 1];
        bool empty_object = json[1] == '}';
        int field_len = snprintf(field, sizeof(field), empty_object ? "\"n\":%u" : "\"n\":%u,", (unsigned)counter);
        if (field_len <= 0) return false;

        size_t new_len = len + (size_t)field_len;
        if (new_len > max_frame_len || new_len + 1 > capacity) return false;

        // Abre espaço após o '{' (inclui o '\0') e grava o campo.
        memmove(json + 1 + field_len, json + 1, len);
        memcpy(json + 1, field, (size_t)field_len);
        len = new_len;
        return true;
    }

} // namespace ttc
