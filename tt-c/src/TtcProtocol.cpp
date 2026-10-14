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
        size_t capacity
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

        return Serialize(doc, out, capacity);
    }

    size_t BuildAircraft(
        const AIRCRAFT_DATA& aircraft,
        char* out,
        size_t capacity
    ) {
        JsonDocument doc;
        doc["t"] = "ac";
        doc["ts"] = aircraft.timestamp;
        doc["icao"] = aircraft.icao;
        doc["lat"] = Round(aircraft.latitude, 5);
        doc["lon"] = Round(aircraft.longitude, 5);
        doc["alt"] = Round(aircraft.altitude, 0);
        doc["vel"] = Round(aircraft.velocity, 1);
        doc["hdg"] = Round(aircraft.heading, 1);
        doc["cnt"] = aircraft.message_count;
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
