#include "Ttc.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "Ttc.h".
 */

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

static constexpr UBaseType_t rx_queue_len  = 8;
static constexpr UBaseType_t tx_queue_len  = 16;
static constexpr TickType_t  enqueue_wait  = pdMS_TO_TICKS(100);
static constexpr size_t      max_fragments = 255;

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── CONSTRUTORES ─────────────────────────────────────────────────

// Público:
Ttc::Ttc(
    LoraRadio& radio
) : radio(radio) {
}

// ─── GETTERS ──────────────────────────────────────────────────────

// Público:
LINK_STATS Ttc::GetLinkStats(
) {
    portENTER_CRITICAL(&stats_lock);
    LINK_STATS copy = stats;
    portEXIT_CRITICAL(&stats_lock);
    return copy;
}

// Público:
bool Ttc::IsHealthy(
) {
    return radio.IsReady() && rx_queue != nullptr && tx_queue != nullptr && tx_mutex != nullptr;
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool Ttc::Init(
) {
    if (rx_queue == nullptr) rx_queue = xQueueCreate(rx_queue_len, sizeof(ttc::TELECOMMAND_PACKET));
    if (tx_queue == nullptr) tx_queue = xQueueCreate(tx_queue_len, sizeof(TX_FRAME));
    if (tx_mutex == nullptr) tx_mutex = xSemaphoreCreateMutex();
    if (rx_queue == nullptr || tx_queue == nullptr || tx_mutex == nullptr) {
        Serial.println("[TT&C] ERRO: falha ao criar filas.");
        return false;
    }

    portENTER_CRITICAL(&stats_lock);
    stats = {};
    stats.last_rssi = -120;
    portEXIT_CRITICAL(&stats_lock);

    return radio.Init();
}

// Público:
void Ttc::Update(
) {
    PollReceive();
    FlushTransmit();
}

// Privado:
void Ttc::PollReceive(
) {
    uint8_t buffer[ttc::max_frame_len];
    int len = radio.Receive(buffer, sizeof(buffer));
    if (len == 0) return;

    if (len < 0) {
        portENTER_CRITICAL(&stats_lock);
        stats.rx_errors_count++;
        portEXIT_CRITICAL(&stats_lock);
        Serial.println("[TT&C] Quadro corrompido (CRC do radio) descartado.");
        return;
    }

    if (len != (int)sizeof(ttc::TELECOMMAND_PACKET)) {
        portENTER_CRITICAL(&stats_lock);
        stats.rx_errors_count++;
        portEXIT_CRITICAL(&stats_lock);
        Serial.printf("[TT&C] Quadro de %d bytes descartado.\n", len);
        return;
    }

    ttc::TELECOMMAND_PACKET tc;
    memcpy(&tc, buffer, sizeof(tc));

    // A estação terrena espera NACK com o sequence_id do quadro corrompido.
    if (!ttc::CheckFrameCrc(buffer, sizeof(tc))) {
        portENTER_CRITICAL(&stats_lock);
        stats.rx_errors_count++;
        portEXIT_CRITICAL(&stats_lock);
        SendAck(tc.sequence_id, tc.command_id, ttc::ERR_CRC);
        return;
    }

    // Leitura SPI fora da seção crítica: o SPI usa mutex, proibido nela.
    int16_t rssi = radio.GetRssi();
    float snr = radio.GetSnr();
    bool delivered = xQueueSend(rx_queue, &tc, 0) == pdTRUE;
    portENTER_CRITICAL(&stats_lock);
    stats.last_rssi = rssi;
    stats.last_snr = snr;
    if (delivered) stats.rx_packets_count++;
    else stats.rx_errors_count++;
    portEXIT_CRITICAL(&stats_lock);
    if (!delivered) Serial.println("[TT&C] Fila de TC cheia, comando descartado.");
}

// Privado:
void Ttc::FlushTransmit(
) {
    TX_FRAME frame;
    while (xQueueReceive(tx_queue, &frame, 0) == pdTRUE) {
        bool sent = radio.Send(frame.data, frame.len);
        portENTER_CRITICAL(&stats_lock);
        if (sent) stats.tx_packets_count++;
        else stats.tx_errors_count++;
        portEXIT_CRITICAL(&stats_lock);
        if (!sent) Serial.printf("[TT&C] Falha no TX de %u bytes.\n", frame.len);
    }
}

// Público:
bool Ttc::ReceiveTelecommand(
    ttc::TELECOMMAND_PACKET& out,
    TickType_t wait
) {
    if (rx_queue == nullptr) return false;
    return xQueueReceive(rx_queue, &out, wait) == pdTRUE;
}

// Público:
bool Ttc::SendAck(
    uint16_t sequence_id,
    uint8_t command_id,
    ttc::ACK_STATUS status
) {
    ttc::ACK_PACKET ack;
    ack.header = ttc::downlink_header;
    ack.sequence_id = sequence_id;
    ack.command_id = command_id;
    ack.status_code = status;
    SealFrame((uint8_t*)&ack, sizeof(ack));

    if (!LockTransmit()) return false;
    bool queued = EnqueueFrame((const uint8_t*)&ack, sizeof(ack));
    xSemaphoreGive(tx_mutex);
    return queued;
}

// Privado:
bool Ttc::LockTransmit(
) {
    if (tx_mutex == nullptr) return false;
    if (xSemaphoreTake(tx_mutex, enqueue_wait) != pdTRUE) {
        Serial.println("[TT&C] Timeout aguardando a fila de descida.");
        return false;
    }
    return true;
}

// Privado:
void Ttc::SealFrame(
    uint8_t* frame,
    size_t len
) {
    size_t body_len = len - sizeof(uint16_t);
    uint16_t crc = ttc::Crc16(frame, body_len);
    frame[body_len] = (uint8_t)(crc & 0xFF);
    frame[body_len + 1] = (uint8_t)(crc >> 8);
}

// Privado:
bool Ttc::EnqueueFrame(
    const uint8_t* frame,
    size_t len
) {
    TX_FRAME item;
    item.len = (uint8_t)len;
    memcpy(item.data, frame, len);
    if (xQueueSend(tx_queue, &item, enqueue_wait) != pdTRUE) {
        Serial.println("[TT&C] Fila de descida cheia, quadro descartado.");
        return false;
    }
    return true;
}

// Público:
bool Ttc::SendTelemetry(
    const ttc::TELEMETRY_PACKET& tm
) {
    if (!LockTransmit()) return false;

    ttc::TELEMETRY_PACKET frame = tm;
    frame.header = ttc::downlink_header;
    frame.sequence_id = downlink_sequence++;
    frame.packet_type = ttc::PACKET_TELEMETRY;
    SealFrame((uint8_t*)&frame, sizeof(frame));

    bool queued = EnqueueFrame((const uint8_t*)&frame, sizeof(frame));
    xSemaphoreGive(tx_mutex);
    return queued;
}

// Público:
bool Ttc::SendData(
    const uint8_t* data,
    size_t len
) {
    if (data == nullptr || len == 0) return false;
    size_t fragment_count = (len + ttc::max_data_chunk - 1) / ttc::max_data_chunk;
    if (fragment_count > max_fragments) {
        Serial.printf("[TT&C] Mensagem de %u bytes excede o limite de fragmentos.\n", (unsigned)len);
        return false;
    }

    if (!LockTransmit()) return false;

    // Só a task do enlace retira da fila: o espaço não diminui sob o mutex.
    if (uxQueueSpacesAvailable(tx_queue) < fragment_count) {
        xSemaphoreGive(tx_mutex);
        Serial.printf("[TT&C] Sem espaco para %u fragmentos.\n", (unsigned)fragment_count);
        return false;
    }

    uint16_t message_id = data_message_id++;
    bool queued = true;

    for (size_t index = 0; index < fragment_count && queued; index++) {
        size_t offset = index * ttc::max_data_chunk;
        size_t chunk_len = min(ttc::max_data_chunk, len - offset);

        ttc::DATA_HEADER header;
        header.header = ttc::downlink_header;
        header.sequence_id = downlink_sequence++;
        header.packet_type = ttc::PACKET_DATA;
        header.message_id = message_id;
        header.fragment_index = (uint8_t)index;
        header.fragment_count = (uint8_t)fragment_count;
        header.chunk_len = (uint8_t)chunk_len;

        uint8_t frame[ttc::max_frame_len];
        size_t frame_len = sizeof(header) + chunk_len + sizeof(uint16_t);
        memcpy(frame, &header, sizeof(header));
        memcpy(frame + sizeof(header), data + offset, chunk_len);
        SealFrame(frame, frame_len);

        queued = EnqueueFrame(frame, frame_len);
    }

    xSemaphoreGive(tx_mutex);
    return queued;
}
