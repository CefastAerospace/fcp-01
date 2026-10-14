#include "Ttc.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "Ttc.h".
 */

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

static constexpr UBaseType_t rx_queue_len     = 8;
static constexpr UBaseType_t ack_queue_len    = 4;
static constexpr UBaseType_t tx_queue_len     = 16;
static constexpr TickType_t  enqueue_wait     = pdMS_TO_TICKS(100);
static constexpr TickType_t  idle_wait        = pdMS_TO_TICKS(20); // Polling de reserva.
static constexpr uint32_t    task_stack_size  = 8192;
static constexpr uint32_t    dedup_expiry_ms  = 30000; // Cobre as retentativas da estação terrena.

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
    return radio.IsReady() && rx_queue != nullptr && ack_queue != nullptr && tx_queue != nullptr
        && dedup_mutex != nullptr;
}

// Público:
bool Ttc::IsTransmitting(
) {
    return transmitting;
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool Ttc::Init(
) {
    if (rx_queue == nullptr) rx_queue = xQueueCreate(rx_queue_len, sizeof(TC_FRAME));
    if (ack_queue == nullptr) ack_queue = xQueueCreate(ack_queue_len, sizeof(TX_FRAME));
    if (tx_queue == nullptr) tx_queue = xQueueCreate(tx_queue_len, sizeof(TX_FRAME));
    if (dedup_mutex == nullptr) dedup_mutex = xSemaphoreCreateMutex();
    if (rx_queue == nullptr || ack_queue == nullptr || tx_queue == nullptr || dedup_mutex == nullptr) {
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
bool Ttc::Start(
    BaseType_t core,
    UBaseType_t priority
) {
    if (task_handle != nullptr) return true;

    BaseType_t created = xTaskCreatePinnedToCore(TaskEntry, "ttc_link", task_stack_size, this, priority, &task_handle, core);
    if (created != pdPASS) {
        task_handle = nullptr;
        Serial.println("[TT&C] ERRO: falha ao criar a task do enlace.");
        return false;
    }
    radio.SetNotifyTask(task_handle);
    return true;
}

// Privado:
void Ttc::TaskEntry(
    void* parameters
) {
    Ttc* self = static_cast<Ttc*>(parameters);
    for (;;) {
        // Com descida pendente, segue sem dormir; senão espera um evento.
        if (!self->Update()) ulTaskNotifyTake(pdTRUE, idle_wait);
    }
}

// Público:
bool Ttc::Update(
) {
    PollReceive();
    TransmitNext();
    return HasPendingTransmit();
}

// Privado:
void Ttc::PollReceive(
) {
    char buffer[ttc::max_frame_len + 1];
    int len = radio.Receive((uint8_t*)buffer, ttc::max_frame_len);
    if (len == 0) return;

    if (len < 0) {
        CountRxError();
        Serial.println("[TT&C] Quadro corrompido (CRC do radio) descartado.");
        return;
    }
    buffer[len] = '\0';

    // Leitura SPI fora da seção crítica: o SPI usa mutex, proibido nela.
    int16_t rssi = radio.GetRssi();
    float snr = radio.GetSnr();
    portENTER_CRITICAL(&stats_lock);
    stats.last_rssi = rssi;
    stats.last_snr = snr;
    portEXIT_CRITICAL(&stats_lock);

    // Envelope inválido: NACK se o seq é legível; senão, só descarta.
    ttc::TC_INFO info;
    if (!ttc::ParseTelecommand(buffer, (size_t)len, info)) {
        CountRxError();
        Serial.printf("[TT&C] TC rejeitado (%s), %d bytes.\n", ttc::StatusName(info.status), len);
        if (info.has_seq) {
            const char* name = info.cmd_name[0] != '\0' ? info.cmd_name : "unknown";
            EnqueueAck(info.seq, name, info.status, -1);
        }
        return;
    }

    HandleTelecommand(info, buffer, (size_t)len);
}

// Privado:
void Ttc::HandleTelecommand(
    const ttc::TC_INFO& info,
    const char* json,
    size_t len
) {
    xSemaphoreTake(dedup_mutex, portMAX_DELAY);
    int slot = FindDedupSlot(info.seq);

    // TC repetido (retentativa da estação terrena): não reexecuta.
    // - Já respondido: reenvia o mesmo ACK.
    // - Ainda em execução: ignora; o ACK sairá quando o OBC responder.
    if (slot >= 0) {
        TX_FRAME cached = {};
        bool acked = dedup[slot].acked;
        if (acked) {
            cached.len = dedup[slot].ack_len;
            memcpy(cached.json, dedup[slot].ack, cached.len);
        }
        xSemaphoreGive(dedup_mutex);

        portENTER_CRITICAL(&stats_lock);
        stats.duplicates_count++;
        portEXIT_CRITICAL(&stats_lock);
        if (acked) EnqueueFrame(ack_queue, cached.json, cached.len);
        return;
    }

    // Registra o TC no slot mais antigo.
    size_t index = dedup_next;
    dedup_next = (dedup_next + 1) % ttc_dedup_slots;
    dedup[index] = {};
    dedup[index].used = true;
    dedup[index].seq = info.seq;
    dedup[index].rx_ms = millis();
    uint32_t rx_ms = dedup[index].rx_ms;
    xSemaphoreGive(dedup_mutex);

    TC_FRAME frame;
    frame.seq = info.seq;
    frame.cmd = info.cmd;
    frame.rx_ms = rx_ms;
    frame.len = (uint8_t)len;
    memcpy(frame.json, json, len);
    frame.json[len] = '\0';

    if (xQueueSend(rx_queue, &frame, 0) != pdTRUE) {
        // Libera o slot para que a retentativa seja aceita.
        xSemaphoreTake(dedup_mutex, portMAX_DELAY);
        dedup[index].used = false;
        xSemaphoreGive(dedup_mutex);
        CountRxError();
        Serial.println("[TT&C] Fila de TC cheia, comando descartado.");
        EnqueueAck(info.seq, info.cmd_name, ttc::ERR_STATE_REJECTED, -1);
        return;
    }

    portENTER_CRITICAL(&stats_lock);
    stats.rx_packets_count++;
    portEXIT_CRITICAL(&stats_lock);
}

// Privado:
int Ttc::FindDedupSlot(
    uint16_t seq
) {
    uint32_t now_ms = millis();
    for (size_t i = 0; i < ttc_dedup_slots; i++) {
        if (dedup[i].used && dedup[i].seq == seq && now_ms - dedup[i].rx_ms < dedup_expiry_ms) return (int)i;
    }
    return -1;
}

// Privado:
void Ttc::TransmitNext(
) {
    if (hold_tx) return;

    // ACKs primeiro: uma rajada de telemetria não atrasa a confirmação de TC.
    TX_FRAME frame;
    bool has_frame = xQueueReceive(ack_queue, &frame, 0) == pdTRUE
                  || xQueueReceive(tx_queue, &frame, 0) == pdTRUE;
    if (!has_frame) return;

    // Contador atribuído no TX: sequência estrita no ar para a detecção de perdas.
    size_t len = frame.len;
    if (!ttc::InsertCounter(frame.json, len, sizeof(frame.json), downlink_counter)) {
        portENTER_CRITICAL(&stats_lock);
        stats.tx_errors_count++;
        portEXIT_CRITICAL(&stats_lock);
        Serial.printf("[TT&C] Pacote de %u bytes sem espaco para o contador.\n", frame.len);
        return;
    }
    downlink_counter++;

    transmitting = true;
    bool sent = radio.Send((const uint8_t*)frame.json, len);
    transmitting = false;

    portENTER_CRITICAL(&stats_lock);
    if (sent) stats.tx_packets_count++;
    else stats.tx_errors_count++;
    portEXIT_CRITICAL(&stats_lock);
    if (!sent) Serial.printf("[TT&C] Falha no TX de %u bytes.\n", (unsigned)len);
}

// Privado:
bool Ttc::HasPendingTransmit(
) {
    if (hold_tx) return false;
    return uxQueueMessagesWaiting(ack_queue) > 0 || uxQueueMessagesWaiting(tx_queue) > 0;
}

// Público:
bool Ttc::ReceiveTelecommand(
    TC_FRAME& out,
    TickType_t wait
) {
    if (rx_queue == nullptr) return false;
    if (xQueueReceive(rx_queue, &out, wait) != pdTRUE) return false;

    // Instante da entrega ao OBC, base do campo "q" do ACK.
    xSemaphoreTake(dedup_mutex, portMAX_DELAY);
    int slot = FindDedupSlot(out.seq);
    if (slot >= 0) dedup[slot].delivered_ms = millis();
    xSemaphoreGive(dedup_mutex);
    return true;
}

// Público:
bool Ttc::SendAck(
    uint16_t seq,
    ttc::COMMAND_ID cmd,
    ttc::ACK_STATUS status
) {
    if (dedup_mutex == nullptr) return false;

    xSemaphoreTake(dedup_mutex, portMAX_DELAY);
    int slot = FindDedupSlot(seq);
    int32_t queue_ms = -1;
    if (slot >= 0 && dedup[slot].delivered_ms != 0) {
        queue_ms = (int32_t)(dedup[slot].delivered_ms - dedup[slot].rx_ms);
    }

    char ack[ttc::max_ack_len];
    size_t len = ttc::BuildAck(seq, ttc::CommandName(cmd), status, queue_ms, ack, sizeof(ack));

    // Guarda o ACK para reenviá-lo a um TC repetido.
    if (slot >= 0 && len > 0) {
        memcpy(dedup[slot].ack, ack, len);
        dedup[slot].ack_len = (uint8_t)len;
        dedup[slot].acked = true;
    }
    xSemaphoreGive(dedup_mutex);

    if (len == 0) return false;
    return EnqueueFrame(ack_queue, ack, len);
}

// Privado:
bool Ttc::EnqueueAck(
    uint16_t seq,
    const char* cmd_name,
    ttc::ACK_STATUS status,
    int32_t queue_ms
) {
    char ack[ttc::max_ack_len];
    size_t len = ttc::BuildAck(seq, cmd_name, status, queue_ms, ack, sizeof(ack));
    if (len == 0) return false;
    return EnqueueFrame(ack_queue, ack, len);
}

// Privado:
bool Ttc::EnqueueFrame(
    QueueHandle_t queue,
    const char* json,
    size_t len
) {
    if (queue == nullptr) return false;

    TX_FRAME item;
    item.len = (uint8_t)len;
    memcpy(item.json, json, len);
    item.json[len] = '\0';
    if (xQueueSend(queue, &item, enqueue_wait) != pdTRUE) {
        Serial.println("[TT&C] Fila de descida cheia, pacote descartado.");
        return false;
    }
    Notify();
    return true;
}

// Privado:
void Ttc::Notify(
) {
    if (task_handle != nullptr) xTaskNotifyGive(task_handle);
}

// Privado:
void Ttc::CountRxError(
) {
    portENTER_CRITICAL(&stats_lock);
    stats.rx_errors_count++;
    portEXIT_CRITICAL(&stats_lock);
}

// Público:
bool Ttc::Send(
    const char* json,
    size_t len
) {
    if (json == nullptr || len < 2 || len > ttc::max_body_len) {
        Serial.printf("[TT&C] Pacote de %u bytes rejeitado (limite %u).\n", (unsigned)len, (unsigned)ttc::max_body_len);
        return false;
    }
    return EnqueueFrame(tx_queue, json, len);
}

// Público:
bool Ttc::SendTelemetry(
    const ttc::TELEMETRY_DATA& tm
) {
    // Campos do enlace vêm do próprio TT&C; contadores em 16 bits com volta.
    LINK_STATS link = GetLinkStats();
    ttc::TELEMETRY_DATA filled = tm;
    filled.rssi = link.last_rssi;
    filled.snr = link.last_snr;
    filled.rx_count = (uint16_t)link.rx_packets_count;
    filled.error_count = (uint16_t)link.rx_errors_count;

    char json[ttc::max_frame_len + 1];
    size_t len = ttc::BuildTelemetry(filled, json, sizeof(json));
    if (len == 0) {
        Serial.println("[TT&C] TM excede o limite do quadro.");
        return false;
    }
    return Send(json, len);
}

// Público:
bool Ttc::HoldTransmit(
    bool hold,
    uint32_t wait_ms
) {
    hold_tx = hold;
    if (!hold) {
        Notify();
        return !transmitting;
    }

    // Um TX já iniciado termina em até radio_tx_timeout_ms.
    uint32_t start_ms = millis();
    while (transmitting && millis() - start_ms < wait_ms) vTaskDelay(pdMS_TO_TICKS(5));
    return !transmitting;
}
