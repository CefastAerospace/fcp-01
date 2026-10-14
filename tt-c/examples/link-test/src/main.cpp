#include <Arduino.h>     // Framework Arduino.
#include <ArduinoJson.h> // para JsonDocument, deserializeJson
#include "Pinout.h"      // para pinout::radio_*
#include "Ttc.h"         // para Ttc, Sx1276Radio, contrato JSON

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

static Sx1276Radio radio({
    pinout::radio_sck,
    pinout::radio_miso,
    pinout::radio_mosi,
    pinout::radio_cs,
    pinout::radio_rst,
    pinout::radio_dio0
});
static Ttc ttc_link(radio);

static constexpr uint32_t tm_period_ms       = 10000;
static constexpr uint32_t aircraft_period_ms = 30000;
static constexpr size_t   aircraft_count     = 3;
static constexpr uint32_t max_burst          = 2000; // Teto do test_tm.
static constexpr uint32_t default_unix_time  = 1760000000UL;

static int64_t       time_offset_ms = (int64_t)default_unix_time * 1000;
static ttc::OP_MODE  mode = ttc::MODE_BENCH;
static uint32_t      last_tm_ms = 0;
static uint32_t      last_aircraft_ms = 0;
static uint32_t      burst_remaining = 0;
static uint32_t      adsb_messages = 0;

// ─── HELPERS ──────────────────────────────────────────────────────

// Tempo de bordo em segundos unix, ajustável por set_time.
static uint32_t Now(
) {
    return (uint32_t)((time_offset_ms + (int64_t)millis()) / 1000);
}

// TM com dados simulados de EPS, TC, ADCS e Payload (REQ-TTC-001).
static ttc::TELEMETRY_DATA BuildTelemetry(
) {
    float t = millis() / 1000.0f;
    ttc::TELEMETRY_DATA tm = {};
    tm.timestamp = Now();
    tm.mode = mode;
    tm.vbat = 7.8f + 0.1f * sinf(t / 60.0f);
    tm.ibat = 420.0f + 30.0f * sinf(t / 7.0f);
    tm.vsolar = 9.1f;
    tm.isolar = 310.0f;
    for (size_t i = 0; i < ttc::thermal_sensors; i++) tm.temps[i] = 21.0f + i + 0.5f * sinf(t / 30.0f);
    tm.rate_dps = 360.0f * expf(-t / 120.0f); // Detumbling a partir de 60 rpm.
    tm.azimuth_deg = fmodf(t * 3.0f, 360.0f);
    tm.aircraft_count = aircraft_count;
    tm.message_count = adsb_messages;
    return tm;
}

// Estado agregado de uma aeronave fictícia, no formato do Payload.
static void SendAircraft(
    size_t index
) {
    float t = millis() / 1000.0f;
    ttc::AIRCRAFT_DATA aircraft = {};
    aircraft.timestamp = Now();
    snprintf(aircraft.icao, sizeof(aircraft.icao), "E4%04X", (unsigned)(0x953A + index));
    aircraft.latitude = -34.57 + 0.01 * index + 0.0001 * t;
    aircraft.longitude = -58.52 - 0.01 * index;
    aircraft.altitude = 30000.0f + 500.0f * index;
    aircraft.velocity = 450.0f + 5.0f * index;
    aircraft.heading = fmodf(37.0f * index, 360.0f);
    aircraft.message_count = (uint16_t)(adsb_messages / aircraft_count);

    char json[ttc::max_frame_len + 1];
    size_t len = ttc::BuildAircraft(aircraft, json, sizeof(json));
    if (len > 0) ttc_link.Send(json, len);
}

// Converte o nome de modo do argumento de set_mode.
static bool ParseMode(
    const char* name,
    ttc::OP_MODE& out
) {
    if (name == nullptr) return false;
    for (uint8_t value = ttc::MODE_BENCH; value <= ttc::MODE_LOW_POWER; value++) {
        if (strcmp(name, ttc::ModeName((ttc::OP_MODE)value)) == 0) {
            out = (ttc::OP_MODE)value;
            return true;
        }
    }
    return false;
}

// Executa o TC simulando o OBC; comandos sem simulação recebem ERR_INVALID_CMD.
static void HandleTelecommand(
    const TC_FRAME& tc
) {
    Serial.printf("[SAT] TC %s seq=%u\n", ttc::CommandName(tc.cmd), tc.seq);

    JsonDocument doc;
    deserializeJson(doc, tc.json, tc.len);
    JsonVariant args = doc["args"];

    ttc::ACK_STATUS status = ttc::ACK_OK;
    bool send_tm = false;
    switch (tc.cmd) {
        case ttc::CMD_PING:
            break;

        case ttc::CMD_REQUEST_TELEMETRY:
        case ttc::CMD_GET_STATUS:
            send_tm = true;
            break;

        case ttc::CMD_SET_TIME:
            if (!args["unix"].is<uint32_t>()) status = ttc::ERR_INVALID_PARAM;
            else time_offset_ms = (int64_t)args["unix"].as<uint32_t>() * 1000 - (int64_t)millis();
            break;

        case ttc::CMD_SET_MODE: {
            ttc::OP_MODE requested;
            if (!ParseMode(args["mode"], requested)) status = ttc::ERR_INVALID_PARAM;
            else mode = requested;
            break;
        }

        case ttc::CMD_ENTER_SAFE:    mode = ttc::MODE_SAFE; break;
        case ttc::CMD_EXIT_SAFE:     mode = ttc::MODE_BENCH; break;
        case ttc::CMD_START_MISSION: mode = ttc::MODE_MISSION; break;
        case ttc::CMD_STOP_MISSION:  mode = ttc::MODE_SUN_POINTING; break;

        case ttc::CMD_TEST_TM: {
            uint32_t count = args["count"] | 0u;
            if (count == 0 || count > max_burst) status = ttc::ERR_INVALID_PARAM;
            else burst_remaining = count;
            break;
        }

        default:
            status = ttc::ERR_INVALID_CMD;
            break;
    }

    ttc_link.SendAck(tc.seq, tc.cmd, status);
    if (send_tm) ttc_link.SendTelemetry(BuildTelemetry());
}

// setup() e loop() são funções obrigatórias do framework Arduino.

/**
 * @brief Inicializa o enlace e a task do rádio.
 */
void setup(
) {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    Serial.println("\n[SAT] Teste do enlace TT&C (simulador do OBC).");

    if (!ttc_link.Init() || !ttc_link.Start()) {
        Serial.println("[SAT] ERRO: falha ao inicializar o TT&C.");
        while (true) delay(1000);
    }
    Serial.println("[SAT] Pronto.");
}

/**
 * @brief Simula o OBC: atende TCs e decide quando enviar TM e aeronaves.
 */
void loop(
) {
    TC_FRAME tc;
    if (ttc_link.ReceiveTelecommand(tc, pdMS_TO_TICKS(20))) HandleTelecommand(tc);

    // Rajada do test_tm: Send() espera vaga na fila de descida.
    if (burst_remaining > 0 && ttc_link.SendTelemetry(BuildTelemetry())) burst_remaining--;

    adsb_messages += 1;

    if (millis() - last_tm_ms >= tm_period_ms) {
        last_tm_ms = millis();
        ttc_link.SendTelemetry(BuildTelemetry());
    }

    if (millis() - last_aircraft_ms >= aircraft_period_ms) {
        last_aircraft_ms = millis();
        for (size_t i = 0; i < aircraft_count; i++) SendAircraft(i);
    }
}
