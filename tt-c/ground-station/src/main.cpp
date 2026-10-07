// ============================================================
// Estacao Terrena improvisada FCP-01
// 2o ESP32 + SX1276. Fala com o OBC (tt-c/src/ttc.h) via LoRa.
// Parâmetros de rádio e protocolo DEVEM ser idênticos aos do satélite.
//
// Menu serial: ping | tm | safe | esafe | mission | stop | status | time [unix] | help
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// --- Pinagem (igual ao satélite: obc-dh/src/drivers/lora_sx1276.h) ---
#define LORA_SCK_PIN   18
#define LORA_MISO_PIN  19
#define LORA_MOSI_PIN  23
#define LORA_CS_PIN    5
#define LORA_RST_PIN   14
#define LORA_DIO0_PIN  4

// --- Parâmetros de rádio (idênticos ao satélite) ---
#define LORA_FREQ       915E6
#define LORA_SF         9
#define LORA_BW         125E3
#define LORA_CR         6
#define LORA_TXPOWER    17
#define LORA_SYNC_WORD  0x12

// --- Opcodes (iguais a CommandID_t do satélite) ---
enum : uint8_t {
    CMD_PING = 0x01,
    CMD_SET_MODE = 0x02,
    CMD_START_MISSION = 0x03,
    CMD_STOP_MISSION = 0x04,
    CMD_GET_STATUS = 0x05,
    CMD_ENTER_SAFE = 0x0A,
    CMD_EXIT_SAFE = 0x0B,
    CMD_REQUEST_TELEMETRY = 0x0C,
    CMD_SET_TIME = 0x10,
    CMD_ADCS_START = 0x20,
    CMD_ADCS_STOP = 0x21,
    CMD_DEPLOY_ANTENNA = 0x30,
    CMD_REQUEST_LOG = 0x40,
    CMD_RESET_OBC = 0xFF
};

// --- Status de ACK (iguais a CmdStatus_t do satélite) ---
static const char* status_name(uint8_t s) {
    switch (s) {
        case 0x00: return "ACK_OK";
        case 0x01: return "ERR_CRC";
        case 0x02: return "ERR_INVALID_CMD";
        case 0x03: return "ERR_INVALID_PARAM";
        case 0x04: return "ERR_STATE_REJECTED";
        case 0xFF: return "ERR_UNKNOWN_CMD";
        default:   return "???";
    }
}

// --- Pacotes (layouts idênticos aos do satélite, packed) ---
typedef struct __attribute__((packed)) {
    uint16_t sequence_id;
    uint8_t  command_id;
    uint8_t  flags;
    uint32_t timestamp;
    uint8_t  arguments[8];
    uint16_t checksum;
} TelecommandPacket_t;  // 18 bytes

typedef struct __attribute__((packed)) {
    uint16_t header;        // 0xAA55
    uint16_t sequence_id;
    uint8_t  command_id;
    uint8_t  status_code;
    uint16_t checksum;
} ACKPacket_t;  // 8 bytes

typedef struct __attribute__((packed)) {
    uint16_t header;        // 0xAA55
    uint16_t sequence_id;
    uint8_t  packet_type;   // 0x01
    uint8_t  system_status;
    uint32_t timestamp;
    float vbat;
    float ibat;
    float temp_obc;
    float rpm;
    float target_rpm;
    int16_t  last_rssi;
    float    last_snr;
    uint32_t rx_packets_count;
    uint32_t tx_packets_count;
    uint32_t rx_errors_count;
    uint16_t checksum;
} TelemetryPacket_t;  // 38 bytes

static uint16_t gs_seq = 0;
static uint16_t last_tm_seq = 0;
static bool last_tm_valid = false;

// --- CRC16-CCITT (mesmo algoritmo do satélite) ---
static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}

static void radio_send(const uint8_t* data, size_t len) {
    LoRa.idle();
    LoRa.beginPacket();
    LoRa.write(data, len);
    LoRa.endPacket();
    LoRa.receive();
}

// Espera um pacote por até timeout_ms. Retorna nº de bytes ou 0.
static int radio_wait(uint8_t* buf, size_t maxlen, uint32_t timeout_ms) {
    uint32_t t0 = millis();
    while (millis() - t0 < timeout_ms) {
        int sz = LoRa.parsePacket();
        if (sz > 0) {
            int n = 0;
            while (LoRa.available() && n < (int)maxlen) buf[n++] = LoRa.read();
            LoRa.receive();  // Volta a RX contínuo IMEDIATO: parsePacket deixa em STANDBY
            return n;
        }
        delay(20);
    }
    return 0;
}

static void send_tc(uint8_t cmd, const uint8_t* args, size_t args_len) {
    TelecommandPacket_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.sequence_id = gs_seq++;
    pkt.command_id = cmd;
    if (args != NULL && args_len > 0) {
        size_t n = (args_len > sizeof(pkt.arguments)) ? sizeof(pkt.arguments) : args_len;
        memcpy(pkt.arguments, args, n);
    }
    pkt.checksum = crc16((const uint8_t*)&pkt, sizeof(pkt) - sizeof(uint16_t));
    Serial.printf("[GS] TX cmd=0x%02X seq=%u (%u bytes no ar)\n", cmd, pkt.sequence_id, (unsigned)sizeof(pkt));
    radio_send((const uint8_t*)&pkt, sizeof(pkt));
}

// Valida e imprime um ACK. Retorna status_code ou -1.
static int handle_ack_buf(const uint8_t* buf, int len, uint16_t want_seq) {
    if (len != (int)sizeof(ACKPacket_t)) {
        Serial.printf("[GS] Tamanho inesperado: %d bytes (ACK tem %u)\n", len, (unsigned)sizeof(ACKPacket_t));
        return -1;
    }
    ACKPacket_t ack;
    memcpy(&ack, buf, sizeof(ack));
    if (ack.header != 0xAA55) { Serial.println("[GS] Header do ACK inválido!"); return -1; }
    if (ack.sequence_id != want_seq) {
        Serial.printf("[GS] Sequence divergente: esperado %u, veio %u\n", want_seq, ack.sequence_id);
        return -1;
    }
    if (crc16(buf, sizeof(ack) - sizeof(uint16_t)) != ack.checksum) {
        Serial.println("[GS] CRC do ACK inválido!");
        return -1;
    }
    Serial.printf("[GS] ACK cmd=0x%02X status=%s RSSI=%d dBm SNR=%.1f dB\n",
                  ack.command_id, status_name(ack.status_code),
                  LoRa.packetRssi(), LoRa.packetSnr());
    return ack.status_code;
}

// Valida e imprime uma telemetria. Retorna true se íntegra.
static bool handle_tm_buf(const uint8_t* buf, int len) {
    if (len != (int)sizeof(TelemetryPacket_t)) {
        Serial.printf("[GS] Tamanho inesperado: %d bytes (TM tem %u)\n", len, (unsigned)sizeof(TelemetryPacket_t));
        return false;
    }
    TelemetryPacket_t tm;
    memcpy(&tm, buf, sizeof(tm));
    if (tm.header != 0xAA55 || tm.packet_type != 0x01) {
        Serial.println("[GS] Header/tipo da TM inválidos!");
        return false;
    }
    if (crc16(buf, sizeof(tm) - sizeof(uint16_t)) != tm.checksum) {
        Serial.println("[GS] CRC da TM inválido!");
        return false;
    }
    Serial.printf("[GS] TM seq=%u estado=%u t=%lus Vbat=%.2fV I=%.0fmA T=%.1fC rpm=%.0f/%.0f rssi=%d snr=%.1f rx=%lu tx=%lu err=%lu\n",
                  tm.sequence_id, tm.system_status, (unsigned long)tm.timestamp,
                  tm.vbat, tm.ibat, tm.temp_obc, tm.rpm, tm.target_rpm,
                  tm.last_rssi, tm.last_snr,
                  (unsigned long)tm.rx_packets_count, (unsigned long)tm.tx_packets_count,
                  (unsigned long)tm.rx_errors_count);
    if (last_tm_valid) {
        uint16_t delta = tm.sequence_id - last_tm_seq;
        Serial.printf("[GS] Delta de sequência desde a última TM: %u%s\n",
                      delta, (delta == 1) ? " (sem perda)" : " (HOUVE PERDA!)");
    }
    last_tm_seq = tm.sequence_id;
    last_tm_valid = true;
    return true;
}

static void cmd_ping() {
    uint16_t seq = gs_seq;
    send_tc(CMD_PING, NULL, 0);
    uint8_t buf[64];
    int n = radio_wait(buf, sizeof(buf), 3000);
    if (n == 0) { Serial.println("[GS] TIMEOUT: nenhum ACK em 3 s."); return; }
    int st = handle_ack_buf(buf, n, seq);
    Serial.printf("[GS] PING: %s\n", (st == 0x00) ? "OK (enlace ida-e-volta funciona!)" : "FALHOU");
}

static void cmd_tm() {
    uint16_t seq = gs_seq;
    send_tc(CMD_REQUEST_TELEMETRY, NULL, 0);
    // Satélite manda TM + ACK; coleta tudo numa janela de 4 s
    uint32_t t0 = millis();
    bool saw_tm = false, saw_ack = false;
    while (millis() - t0 < 4000 && !(saw_tm && saw_ack)) {
        uint8_t buf[64];
        int n = radio_wait(buf, sizeof(buf), 500);
        if (n == 0) continue;
        if (n == (int)sizeof(TelemetryPacket_t)) saw_tm = handle_tm_buf(buf, n);
        else if (n == (int)sizeof(ACKPacket_t)) saw_ack = (handle_ack_buf(buf, n, seq) == 0x00);
        else Serial.printf("[GS] Pacote estranho: %d bytes\n", n);
    }
    if (!saw_tm) Serial.println("[GS] TM não recebida na janela.");
    if (!saw_ack) Serial.println("[GS] ACK não recebido na janela.");
}

static void cmd_simple(const char* nome, uint8_t cmd, const uint8_t* args, size_t args_len) {
    uint16_t seq = gs_seq;
    send_tc(cmd, args, args_len);
    uint8_t buf[64];
    int n = radio_wait(buf, sizeof(buf), 3000);
    if (n == 0) { Serial.printf("[GS] %s: TIMEOUT.\n", nome); return; }
    int st = handle_ack_buf(buf, n, seq);
    Serial.printf("[GS] %s: %s\n", nome, (st >= 0) ? status_name((uint8_t)st) : "resposta inválida");
}

static void print_help() {
    Serial.println("\n=== ESTACAO TERRENA FCP-01 ===");
    Serial.println("  ping         -> PING (testa ida-e-volta)");
    Serial.println("  tm           -> Pede telemetria (valida CRC + sequência)");
    Serial.println("  safe         -> Entra em modo seguro");
    Serial.println("  esafe        -> Sai do modo seguro (chave 0xA55A)");
    Serial.println("  mission      -> Inicia missão (só a partir de SAFE)");
    Serial.println("  stop         -> Para missão (volta a SAFE)");
    Serial.println("  status       -> Pede status");
    Serial.println("  time [unix]  -> Acerta relógio (padrão: 1750000000)");
    Serial.println("  help         -> Esta ajuda");
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    Serial.println("\n=== [GS] Estacao Terrena FCP-01 ===");

    SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_CS_PIN);
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(LORA_FREQ)) {
        Serial.println("[GS] ERRO: SX1276 não responde! Confira fiação/alimentação.");
        while (true) { delay(1000); }
    }
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setTxPower(LORA_TXPOWER);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.receive();
    Serial.println("[GS] Radio OK. Digite help.");
    print_help();
}

void loop() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() == 0) return;

        if (input.equalsIgnoreCase("ping")) cmd_ping();
        else if (input.equalsIgnoreCase("tm")) cmd_tm();
        else if (input.equalsIgnoreCase("safe")) cmd_simple("ENTER_SAFE", CMD_ENTER_SAFE, NULL, 0);
        else if (input.equalsIgnoreCase("esafe")) {
            uint8_t key[2] = {0x5A, 0xA5};
            cmd_simple("EXIT_SAFE", CMD_EXIT_SAFE, key, 2);
        }
        else if (input.equalsIgnoreCase("mission")) cmd_simple("START_MISSION", CMD_START_MISSION, NULL, 0);
        else if (input.equalsIgnoreCase("stop")) cmd_simple("STOP_MISSION", CMD_STOP_MISSION, NULL, 0);
        else if (input.equalsIgnoreCase("status")) cmd_simple("GET_STATUS", CMD_GET_STATUS, NULL, 0);
        else if (input.startsWith("time")) {
            uint32_t unix = 1750000000UL;
            int sp = input.indexOf(' ');
            if (sp > 0) unix = (uint32_t)input.substring(sp + 1).toInt();
            Serial.printf("[GS] Enviando unix=%lu\n", (unsigned long)unix);
            cmd_simple("SET_TIME", CMD_SET_TIME, (uint8_t*)&unix, sizeof(unix));
        }
        else if (input.equalsIgnoreCase("help")) print_help();
        else Serial.println("[GS] Desconhecido. Digite help.");
    }
    delay(50);
}
