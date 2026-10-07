#include <Arduino.h> // Framework Arduino.
#include "Pinout.h"  // para pinout::radio_*
#include "Ttc.h"     // para Ttc, Sx1276Radio

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

static constexpr uint32_t json_period_ms     = 30000;
static constexpr int      json_default_count = 5;      // ~4 fragmentos.
static constexpr size_t   json_buffer_len    = 8192;

static int64_t  time_offset_ms = 0;
static uint32_t last_json_ms = 0;
static uint16_t aircraft_counter = 0;
static char     json_buffer[json_buffer_len];

// ─── HELPERS ──────────────────────────────────────────────────────

// Única task que acessa o rádio, como exigido pelo Ttc.
static void LinkTask(
    void* parameters
) {
    (void)parameters;
    for (;;) {
        ttc_link.Update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Telemetria com o tempo de bordo e as estatísticas do enlace; demais campos zerados.
static ttc::TELEMETRY_PACKET BuildTelemetry(
) {
    LINK_STATS stats = ttc_link.GetLinkStats();
    ttc::TELEMETRY_PACKET tm = {};
    tm.timestamp = (uint32_t)((time_offset_ms + (int64_t)millis()) / 1000);
    tm.last_rssi = stats.last_rssi;
    tm.last_snr = stats.last_snr;
    tm.rx_packets_count = stats.rx_packets_count;
    tm.tx_packets_count = stats.tx_packets_count;
    tm.rx_errors_count = stats.rx_errors_count;
    return tm;
}

// Responde ao TC simulando o OBC; opcodes sem simulação recebem ERR_UNKNOWN_CMD.
static void HandleTelecommand(
    const ttc::TELECOMMAND_PACKET& tc
) {
    Serial.printf("[SAT] TC cmd=0x%02X seq=%u\n", tc.command_id, tc.sequence_id);

    ttc::ACK_STATUS status = ttc::ACK_OK;
    switch (tc.command_id) {
        case ttc::CMD_PING:
            break;

        case ttc::CMD_REQUEST_TELEMETRY:
        case ttc::CMD_GET_STATUS:
            ttc_link.SendTelemetry(BuildTelemetry());
            break;

        case ttc::CMD_SET_TIME: {
            uint32_t unix_time;
            memcpy(&unix_time, tc.arguments, sizeof(unix_time));
            time_offset_ms = (int64_t)unix_time * 1000 - (int64_t)millis();
            break;
        }

        default:
            status = ttc::ERR_UNKNOWN_CMD;
            break;
    }
    ttc_link.SendAck(tc.sequence_id, tc.command_id, status);
}

// Gera um array JSON no formato do Payload (payload/README.md) com aeronaves fictícias.
static size_t BuildAircraftJson(
    int count
) {
    size_t len = snprintf(json_buffer, json_buffer_len, "[");
    for (int i = 0; i < count && len < json_buffer_len; i++) {
        uint16_t id = aircraft_counter++;
        len += snprintf(json_buffer + len, json_buffer_len - len,
                        "%s{\"icao\":\"E4%04X\",\"lat\":%.5f,\"lon\":%.5f,\"alt\":%.1f,\"vel\":%.1f,\"heading\":%.1f,\"seq_id\":%u}",
                        (i == 0) ? "" : ",", id,
                        -19.9 + (id % 50) * 0.01, -43.9 + (id % 30) * 0.01,
                        30000.0 + (id % 10) * 500.0, 450.0 + (id % 7) * 5.0,
                        (double)((id * 37) % 360), id);
    }
    if (len < json_buffer_len - 1) len += snprintf(json_buffer + len, json_buffer_len - len, "]");
    return min(len, json_buffer_len - 1);
}

static void SendAircraftJson(
    int count
) {
    size_t len = BuildAircraftJson(count);
    size_t fragments = (len + ttc::max_data_chunk - 1) / ttc::max_data_chunk;
    bool queued = ttc_link.SendData((const uint8_t*)json_buffer, len);
    Serial.printf("[SAT] JSON %d aeronaves, %u bytes, %u fragmentos: %s\n",
                  count, (unsigned)len, (unsigned)fragments, queued ? "enfileirado" : "REJEITADO");
}

static void HandleSerialLine(
    String input
) {
    input.trim();
    if (input.startsWith("json")) {
        int space = input.indexOf(' ');
        int count = (space > 0) ? input.substring(space + 1).toInt() : json_default_count;
        SendAircraftJson(max(count, 1));
    }
    else if (input.equalsIgnoreCase("stats")) {
        LINK_STATS stats = ttc_link.GetLinkStats();
        Serial.printf("[SAT] rx=%lu tx=%lu rx_err=%lu tx_err=%lu rssi=%d snr=%.1f\n",
                      (unsigned long)stats.rx_packets_count, (unsigned long)stats.tx_packets_count,
                      (unsigned long)stats.rx_errors_count, (unsigned long)stats.tx_errors_count,
                      stats.last_rssi, stats.last_snr);
    }
    else if (input.length() > 0) {
        Serial.println("[SAT] Comandos: json [n] | stats");
    }
}

// setup() e loop() são funções obrigatórias do framework Arduino.

/**
 * @brief Inicializa o enlace e a task do rádio.
 */
void setup(
) {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    Serial.println("\n=== [SAT] Teste do enlace TT&C ===");

    if (!ttc_link.Init()) {
        Serial.println("[SAT] ERRO: falha ao inicializar o TT&C.");
        while (true) delay(1000);
    }
    xTaskCreatePinnedToCore(LinkTask, "ttc_link", 4096, nullptr, 2, nullptr, 1);
    Serial.println("[SAT] Pronto. Comandos: json [n] | stats");
}

/**
 * @brief Simula o OBC: atende TCs e envia JSON periodicamente.
 */
void loop(
) {
    ttc::TELECOMMAND_PACKET tc;
    if (ttc_link.ReceiveTelecommand(tc, pdMS_TO_TICKS(50))) HandleTelecommand(tc);

    if (millis() - last_json_ms >= json_period_ms) {
        last_json_ms = millis();
        SendAircraftJson(json_default_count);
    }

    if (Serial.available() > 0) HandleSerialLine(Serial.readStringUntil('\n'));
}
