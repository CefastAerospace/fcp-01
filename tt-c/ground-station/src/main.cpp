#include <Arduino.h>     // Framework Arduino.
#include "LoraRadio.h"   // para Sx1276Radio, RADIO_PINS
#include "TtcProtocol.h" // para quadros, opcodes e Crc16

// ─── STRUCTS ──────────────────────────────────────────────────────

enum FRAME_KIND {
    FRAME_NONE,
    FRAME_INVALID,
    FRAME_ACK,
    FRAME_TELEMETRY,
    FRAME_DATA
};

// Teto da remontagem; o satélite comporta 16 fragmentos na fila de descida.
constexpr size_t gs_max_fragments = 32;

struct REASSEMBLY {
    bool     active;
    uint16_t message_id;
    uint8_t  fragment_count;
    uint8_t  received_count;
    size_t   total_len;
    bool     received[gs_max_fragments];
    uint8_t  buffer[gs_max_fragments * ttc::max_data_chunk + 1];
};

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

// Mesma ligação do satélite (VSPI).
static Sx1276Radio radio({18, 19, 23, 5, 14, 4});

static constexpr uint32_t command_timeout_ms = 6000; // Cobre uma rajada de fragmentos do satélite.
static constexpr uint32_t default_unix_time  = 1750000000UL;
static constexpr uint16_t exit_safe_key      = 0xA55A;

static uint16_t uplink_sequence = 0;
static bool     downlink_seen = false;
static uint16_t last_downlink_sequence = 0;
static REASSEMBLY reassembly = {};

// ─── HELPERS ──────────────────────────────────────────────────────

// Nome legível de um ACK_STATUS.
static const char* StatusName(
    uint8_t status
) {
    switch (status) {
        case ttc::ACK_OK:             return "ACK_OK";
        case ttc::ERR_CRC:            return "ERR_CRC";
        case ttc::ERR_INVALID_CMD:    return "ERR_INVALID_CMD";
        case ttc::ERR_INVALID_PARAM:  return "ERR_INVALID_PARAM";
        case ttc::ERR_STATE_REJECTED: return "ERR_STATE_REJECTED";
        case ttc::ERR_UNKNOWN_CMD:    return "ERR_UNKNOWN_CMD";
        default:                      return "???";
    }
}

// Monta e transmite um TC; corrupt altera um byte após o CRC para testar o NACK.
static uint16_t SendTelecommand(
    uint8_t command_id,
    const uint8_t* args,
    size_t args_len,
    bool corrupt
) {
    ttc::TELECOMMAND_PACKET tc = {};
    tc.sequence_id = uplink_sequence++;
    tc.command_id = command_id;
    if (args != nullptr && args_len > 0) {
        memcpy(tc.arguments, args, min(args_len, sizeof(tc.arguments)));
    }
    tc.checksum = ttc::Crc16((const uint8_t*)&tc, sizeof(tc) - sizeof(uint16_t));
    if (corrupt) tc.arguments[0] ^= 0xFF;

    bool sent = radio.Send((const uint8_t*)&tc, sizeof(tc));
    Serial.printf("[GS] TX cmd=0x%02X seq=%u %s\n", command_id, tc.sequence_id, sent ? "" : "(FALHA NO TX)");
    return tc.sequence_id;
}

// Detecta perda pelo sequence_id compartilhado entre TM e quadros de dados.
static void TrackDownlinkSequence(
    uint16_t sequence_id
) {
    if (downlink_seen && (uint16_t)(sequence_id - last_downlink_sequence) != 1) {
        Serial.printf("[GS] PERDA: esperado seq=%u, veio %u\n", (uint16_t)(last_downlink_sequence + 1), sequence_id);
    }
    last_downlink_sequence = sequence_id;
    downlink_seen = true;
}

// Identifica o tipo do quadro pelo tamanho e packet_type, após conferir o CRC.
static FRAME_KIND ClassifyFrame(
    const uint8_t* frame,
    int len
) {
    if (len <= 5) return FRAME_INVALID;

    uint16_t header = (uint16_t)frame[0] | ((uint16_t)frame[1] << 8);
    if (header != ttc::downlink_header) return FRAME_INVALID;
    if (!ttc::CheckFrameCrc(frame, len)) {
        Serial.printf("[GS] CRC invalido em quadro de %d bytes.\n", len);
        return FRAME_INVALID;
    }

    if (len == (int)sizeof(ttc::ACK_PACKET)) return FRAME_ACK;
    if (frame[4] == ttc::PACKET_TELEMETRY && len == (int)sizeof(ttc::TELEMETRY_PACKET)) return FRAME_TELEMETRY;
    if (frame[4] == ttc::PACKET_DATA && len >= (int)(sizeof(ttc::DATA_HEADER) + sizeof(uint16_t))) return FRAME_DATA;
    return FRAME_INVALID;
}

static void PrintAck(
    const ttc::ACK_PACKET& ack
) {
    Serial.printf("[GS] ACK seq=%u cmd=0x%02X status=%s RSSI=%d dBm SNR=%.1f dB\n",
                  ack.sequence_id, ack.command_id, StatusName(ack.status_code),
                  radio.GetRssi(), radio.GetSnr());
}

static void PrintTelemetry(
    const ttc::TELEMETRY_PACKET& tm
) {
    Serial.printf("[GS] TM seq=%u estado=%u t=%lus Vbat=%.2fV I=%.0fmA T=%.1fC rpm=%.0f/%.0f\n",
                  tm.sequence_id, tm.system_status, (unsigned long)tm.timestamp,
                  tm.vbat, tm.ibat, tm.temp_obc, tm.rpm, tm.target_rpm);
    Serial.printf("[GS]    enlace a bordo: rssi=%d snr=%.1f rx=%lu tx=%lu err=%lu | aqui: RSSI=%d SNR=%.1f\n",
                  tm.last_rssi, tm.last_snr,
                  (unsigned long)tm.rx_packets_count, (unsigned long)tm.tx_packets_count,
                  (unsigned long)tm.rx_errors_count, radio.GetRssi(), radio.GetSnr());
}

// Remonta mensagens fragmentadas; uma mensagem nova descarta a incompleta anterior.
static void HandleData(
    const uint8_t* frame,
    int len
) {
    ttc::DATA_HEADER header;
    memcpy(&header, frame, sizeof(header));
    const uint8_t* chunk = frame + sizeof(header);

    bool is_last = header.fragment_index + 1 == header.fragment_count;
    bool valid = len == (int)(sizeof(header) + header.chunk_len + sizeof(uint16_t))
              && header.fragment_index < header.fragment_count
              && header.fragment_count <= gs_max_fragments
              && (is_last || header.chunk_len == ttc::max_data_chunk);
    if (!valid) {
        Serial.println("[GS] Quadro de dados inconsistente, descartado.");
        return;
    }

    if (!reassembly.active || reassembly.message_id != header.message_id) {
        if (reassembly.active) {
            Serial.printf("[GS] Mensagem %u incompleta (%u/%u fragmentos), descartada.\n",
                          reassembly.message_id, reassembly.received_count, reassembly.fragment_count);
        }
        memset(&reassembly, 0, sizeof(reassembly));
        reassembly.active = true;
        reassembly.message_id = header.message_id;
        reassembly.fragment_count = header.fragment_count;
    }

    if (!reassembly.received[header.fragment_index]) {
        memcpy(reassembly.buffer + header.fragment_index * ttc::max_data_chunk, chunk, header.chunk_len);
        reassembly.received[header.fragment_index] = true;
        reassembly.received_count++;
        if (is_last) reassembly.total_len = header.fragment_index * ttc::max_data_chunk + header.chunk_len;
    }

    Serial.printf("[GS] DADOS msg=%u frag %u/%u (%u bytes) RSSI=%d dBm\n",
                  header.message_id, header.fragment_index + 1, header.fragment_count,
                  header.chunk_len, radio.GetRssi());

    if (reassembly.received_count == reassembly.fragment_count) {
        reassembly.buffer[reassembly.total_len] = '\0';
        Serial.printf("[GS] Mensagem %u completa (%u bytes):\n%s\n",
                      reassembly.message_id, (unsigned)reassembly.total_len, (const char*)reassembly.buffer);
        reassembly.active = false;
    }
}

// Lê um quadro, se houver, e trata conforme o tipo. Copia o ACK para ack_out.
static FRAME_KIND PollRadio(
    ttc::ACK_PACKET* ack_out
) {
    uint8_t frame[ttc::max_frame_len];
    int len = radio.Receive(frame, sizeof(frame));
    if (len == 0) return FRAME_NONE;
    if (len < 0) {
        Serial.printf("[GS] Quadro corrompido (CRC do radio) RSSI=%d dBm SNR=%.1f dB\n", radio.GetRssi(), radio.GetSnr());
        return FRAME_INVALID;
    }

    FRAME_KIND kind = ClassifyFrame(frame, len);
    switch (kind) {
        case FRAME_ACK: {
            ttc::ACK_PACKET ack;
            memcpy(&ack, frame, sizeof(ack));
            PrintAck(ack);
            if (ack_out != nullptr) *ack_out = ack;
            break;
        }
        case FRAME_TELEMETRY: {
            ttc::TELEMETRY_PACKET tm;
            memcpy(&tm, frame, sizeof(tm));
            TrackDownlinkSequence(tm.sequence_id);
            PrintTelemetry(tm);
            break;
        }
        case FRAME_DATA: {
            TrackDownlinkSequence((uint16_t)frame[2] | ((uint16_t)frame[3] << 8));
            HandleData(frame, len);
            break;
        }
        default:
            Serial.printf("[GS] Quadro invalido de %d bytes.\n", len);
            break;
    }
    return kind;
}

// Envia um TC e espera o ACK correspondente (e a TM, se expect_tm).
static void RunCommand(
    const char* name,
    uint8_t command_id,
    const uint8_t* args,
    size_t args_len,
    bool expect_tm,
    bool corrupt
) {
    uint16_t sequence_id = SendTelecommand(command_id, args, args_len, corrupt);

    bool saw_ack = false;
    bool saw_tm = false;
    uint8_t status = 0;
    uint32_t start_ms = millis();

    while (millis() - start_ms < command_timeout_ms && !(saw_ack && (saw_tm || !expect_tm))) {
        ttc::ACK_PACKET ack;
        FRAME_KIND kind = PollRadio(&ack);
        if (kind == FRAME_ACK && ack.sequence_id == sequence_id) {
            saw_ack = true;
            status = ack.status_code;
        }
        if (kind == FRAME_TELEMETRY) saw_tm = true;
        if (kind == FRAME_NONE) delay(5);
    }

    if (!saw_ack) Serial.printf("[GS] %s: TIMEOUT sem ACK.\n", name);
    else Serial.printf("[GS] %s: %s (%lu ms)\n", name, StatusName(status), (unsigned long)(millis() - start_ms));
    if (expect_tm && !saw_tm) Serial.printf("[GS] %s: TM nao recebida.\n", name);
}

static void PrintHelp(
) {
    Serial.println("\n=== ESTACAO TERRENA FCP-01 ===");
    Serial.println("  ping         -> PING (ida e volta)");
    Serial.println("  tm           -> Pede telemetria (TM + ACK)");
    Serial.println("  status       -> Pede status (TM + ACK)");
    Serial.println("  safe         -> Entra em modo seguro");
    Serial.println("  esafe        -> Sai do modo seguro (chave 0xA55A)");
    Serial.println("  mission      -> Inicia missao");
    Serial.println("  stop         -> Para missao");
    Serial.println("  time [unix]  -> Acerta relogio (padrao: 1750000000)");
    Serial.println("  badcrc       -> PING com CRC corrompido (espera ERR_CRC)");
    Serial.println("  help         -> Esta ajuda");
    Serial.println("Quadros nao solicitados (dados JSON, TM) sao exibidos automaticamente.");
}

static void HandleSerialLine(
    String input
) {
    input.trim();
    if (input.length() == 0) return;

    if (input.equalsIgnoreCase("ping")) RunCommand("PING", ttc::CMD_PING, nullptr, 0, false, false);
    else if (input.equalsIgnoreCase("tm")) RunCommand("REQUEST_TELEMETRY", ttc::CMD_REQUEST_TELEMETRY, nullptr, 0, true, false);
    else if (input.equalsIgnoreCase("status")) RunCommand("GET_STATUS", ttc::CMD_GET_STATUS, nullptr, 0, true, false);
    else if (input.equalsIgnoreCase("safe")) RunCommand("ENTER_SAFE", ttc::CMD_ENTER_SAFE, nullptr, 0, false, false);
    else if (input.equalsIgnoreCase("esafe")) {
        RunCommand("EXIT_SAFE", ttc::CMD_EXIT_SAFE, (const uint8_t*)&exit_safe_key, sizeof(exit_safe_key), false, false);
    }
    else if (input.equalsIgnoreCase("mission")) RunCommand("START_MISSION", ttc::CMD_START_MISSION, nullptr, 0, false, false);
    else if (input.equalsIgnoreCase("stop")) RunCommand("STOP_MISSION", ttc::CMD_STOP_MISSION, nullptr, 0, false, false);
    else if (input.startsWith("time")) {
        uint32_t unix_time = default_unix_time;
        int space = input.indexOf(' ');
        if (space > 0) unix_time = (uint32_t)input.substring(space + 1).toInt();
        Serial.printf("[GS] Enviando unix=%lu\n", (unsigned long)unix_time);
        RunCommand("SET_TIME", ttc::CMD_SET_TIME, (const uint8_t*)&unix_time, sizeof(unix_time), false, false);
    }
    else if (input.equalsIgnoreCase("badcrc")) RunCommand("PING (CRC corrompido)", ttc::CMD_PING, nullptr, 0, false, true);
    else if (input.equalsIgnoreCase("help")) PrintHelp();
    else Serial.println("[GS] Desconhecido. Digite help.");
}

// setup() e loop() são funções obrigatórias do framework Arduino.

/**
 * @brief Inicializa serial e rádio.
 */
void setup(
) {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    Serial.println("\n=== [GS] Estacao Terrena FCP-01 ===");

    if (!radio.Init()) {
        Serial.println("[GS] ERRO: SX1276 nao responde. Confira fiacao/alimentacao.");
        while (true) delay(1000);
    }
    PrintHelp();
}

/**
 * @brief Atende comandos do operador e exibe quadros recebidos.
 */
void loop(
) {
    if (Serial.available() > 0) HandleSerialLine(Serial.readStringUntil('\n'));
    if (PollRadio(nullptr) == FRAME_NONE) delay(5);
}
