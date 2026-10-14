#include <Arduino.h>     // Framework Arduino.
#include <ArduinoJson.h> // para JsonDocument, deserializeJson
#include "LoraRadio.h"   // para Sx1276Radio, RADIO_PINS
#include "TtcProtocol.h" // para max_frame_len

// Ponte transparente entre o notebook (serial) e o enlace LoRa.
// - Linha da serial iniciada por '{' é transmitida como um quadro.
// - Quadro recebido vira uma linha JSON com metadados da recepção:
//   {"gs":{"rssi":-80,"snr":9.2,"ms":123},"pkt":{...}}   JSON válido
//   {"gs":{...},"raw":"7b22..."}                         JSON inválido
//   {"gs":{...},"err":"crc"}                             CRC do rádio
// - Linhas de log nunca começam com '{'; o software do notebook as ignora.

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

// Mesma ligação do satélite (VSPI).
static Sx1276Radio radio({18, 19, 23, 5, 14, 4});

static constexpr uint32_t serial_baud = 115200;

static char   line_buffer[ttc::max_frame_len + 2];
static size_t line_len = 0;
static bool   line_overflow = false;

// ─── HELPERS ──────────────────────────────────────────────────────

// Escreve o cabeçalho {"gs":{...}, comum a todas as linhas de recepção.
static void PrintReceptionHeader(
) {
    Serial.printf("{\"gs\":{\"rssi\":%d,\"snr\":%.1f,\"ms\":%lu},",
                  radio.GetRssi(), radio.GetSnr(), (unsigned long)millis());
}

// Repassa ao notebook um quadro recebido pelo rádio.
static void ForwardFrame(
    const uint8_t* frame,
    int len
) {
    PrintReceptionHeader();
    if (len < 0) {
        Serial.println("\"err\":\"crc\"}");
        return;
    }

    // JSON válido segue literal; inválido segue em hexadecimal para análise.
    JsonDocument doc;
    bool valid = deserializeJson(doc, (const char*)frame, (size_t)len) == DeserializationError::Ok
              && doc.is<JsonObject>();
    if (valid) {
        Serial.print("\"pkt\":");
        Serial.write(frame, (size_t)len);
        Serial.println("}");
        return;
    }

    Serial.print("\"raw\":\"");
    for (int i = 0; i < len; i++) Serial.printf("%02x", frame[i]);
    Serial.println("\"}");
}

// Transmite uma linha recebida do notebook.
static void TransmitLine(
    const char* line,
    size_t len
) {
    if (len == 0 || line[0] != '{') return;
    if (!radio.Send((const uint8_t*)line, len)) Serial.println("[GS] ERRO: falha no TX.");
}

// Acumula a serial sem bloquear e transmite cada linha completa.
static void PollSerial(
) {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r') continue;

        if (c == '\n') {
            if (line_overflow) Serial.printf("[GS] ERRO: linha excede %u bytes.\n", (unsigned)ttc::max_frame_len);
            else TransmitLine(line_buffer, line_len);
            line_len = 0;
            line_overflow = false;
            continue;
        }

        if (line_len < ttc::max_frame_len) line_buffer[line_len++] = c;
        else line_overflow = true;
    }
}

// setup() e loop() são funções obrigatórias do framework Arduino.

/**
 * @brief Inicializa serial e rádio.
 */
void setup(
) {
    Serial.begin(serial_baud);
    while (!Serial && millis() < 2000);
    Serial.println("\n[GS] Ponte da estacao terrena FCP-01.");

    if (!radio.Init()) {
        Serial.println("[GS] ERRO: SX1276 nao responde. Confira fiacao/alimentacao.");
        while (true) delay(1000);
    }
    Serial.println("[GS] Pronto.");
}

/**
 * @brief Encaminha serial -> rádio e rádio -> serial.
 */
void loop(
) {
    PollSerial();

    uint8_t frame[ttc::max_frame_len];
    int len = radio.Receive(frame, sizeof(frame));
    if (len != 0) ForwardFrame(frame, len);
    else delay(2);
}
