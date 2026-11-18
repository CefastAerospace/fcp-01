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
static constexpr uint32_t    unacked_expiry_ms = 300000; // TC sem ACK: OBC travado não prende o slot.
static constexpr uint32_t    health_period_ms = 1000;

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── CONSTRUTORES ─────────────────────────────────────────────────

// Público:
Ttc::Ttc(
    LoraRadio& radio,
    LinkTransport* umbilical
) : radio(radio), umbilical(umbilical) {
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
LINK_DIAGNOSTICS Ttc::GetDiagnostics(
) {
    portENTER_CRITICAL(&stats_lock);
    LINK_DIAGNOSTICS copy = diagnostics;
    copy.sampled_ms = millis();
    copy.radio_resets = stats.radio_resets_count;
    TaskHandle_t task = task_handle;
    portEXIT_CRITICAL(&stats_lock);
    // A tarefa não é destruída em produção; nunca varrer sua pilha sob lock.
    if (task != nullptr) copy.stack_free_bytes = uxTaskGetStackHighWaterMark(task);
    return copy;
}

// Público:
bool Ttc::IsHealthy(
) {
    return radio_healthy && rx_queue != nullptr && ack_queue != nullptr && tx_queue != nullptr
        && event_queue != nullptr && confirm_queue != nullptr && dedup_mutex != nullptr;
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
    portENTER_CRITICAL(&stats_lock);
    diagnostics.init_attempted = true;
    portEXIT_CRITICAL(&stats_lock);
    if (rx_queue == nullptr) rx_queue = xQueueCreate(rx_queue_len, sizeof(TC_FRAME));
    if (ack_queue == nullptr) ack_queue = xQueueCreate(ack_queue_len, sizeof(TX_FRAME));
    if (tx_queue == nullptr) tx_queue = xQueueCreate(tx_queue_len, sizeof(TX_FRAME));
    if (event_queue == nullptr) event_queue = xQueueCreate(ttc_tracked_slots, sizeof(TX_EVENT));
    if (confirm_queue == nullptr) confirm_queue = xQueueCreate(ttc_confirm_slots, sizeof(ttc::DELIVERY_ID));
    if (dedup_mutex == nullptr) dedup_mutex = xSemaphoreCreateMutex();
    if (rx_queue == nullptr || ack_queue == nullptr || tx_queue == nullptr || event_queue == nullptr
            || confirm_queue == nullptr || dedup_mutex == nullptr) {
        Serial.print("[TT&C] ERRO: falha ao criar filas.\n");
        return false;
    }

    portENTER_CRITICAL(&stats_lock);
    stats = {};
    stats.last_rssi = -120;
    portEXIT_CRITICAL(&stats_lock);

    if (umbilical != nullptr && !umbilical->Init()) Serial.print("[TT&C] ERRO: falha no umbilical.\n");

    // Rádio ausente não impede o umbilical; a verificação de saúde tenta de novo.
    radio_healthy = radio.Init();
    const bool wired_ready = umbilical != nullptr && umbilical->IsReady();
    portENTER_CRITICAL(&stats_lock);
    diagnostics.initialized = true;
    diagnostics.radio_ready = radio_healthy;
    diagnostics.wired_ready = wired_ready;
    portEXIT_CRITICAL(&stats_lock);
    return radio_healthy;
}

// Público:
void Ttc::SetUmbilicalEnabled(
    bool enabled
) {
    umbilical_enabled = enabled;
}

// Público:
bool Ttc::Start(
    BaseType_t core,
    UBaseType_t priority
) {
    if (task_handle != nullptr) return true;

    portENTER_CRITICAL(&stats_lock);
    diagnostics.progress_ms = millis();
    portEXIT_CRITICAL(&stats_lock);
    TaskHandle_t task = nullptr;
    BaseType_t created = xTaskCreatePinnedToCore(TaskEntry, "ttc_link", task_stack_size, this, priority, &task, core);
    if (created != pdPASS) {
        Serial.print("[TT&C] ERRO: falha ao criar a task do enlace.\n");
        return false;
    }
    portENTER_CRITICAL(&stats_lock);
    task_handle = task;
    diagnostics.task_started = true;
    portEXIT_CRITICAL(&stats_lock);
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
    portENTER_CRITICAL(&stats_lock);
    diagnostics.updating = true;
    diagnostics.progress_ms = millis();
    portEXIT_CRITICAL(&stats_lock);
    MaintainLink();
    PollTransport(radio, LINK_RF);
    if (umbilical != nullptr) PollTransport(*umbilical, LINK_WIRED);
    TransmitNext();
    const bool pending = HasPendingTransmit();
    portENTER_CRITICAL(&stats_lock);
    diagnostics.updating = false;
    diagnostics.progress_ms = millis();
    if (diagnostics.completions != UINT64_MAX) ++diagnostics.completions;
    portEXIT_CRITICAL(&stats_lock);
    return pending;
}

// Privado:
void Ttc::MaintainLink(
) {
    uint32_t now_ms = millis();
    if (now_ms - last_health_ms >= health_period_ms) {
        last_health_ms = now_ms;
        CheckRadio();
    }
    SendBeaconIfDue();
}

// Privado:
void Ttc::CheckRadio(
) {
    if (radio.CheckHealth()) {
        radio_healthy = true;
        portENTER_CRITICAL(&stats_lock);
        diagnostics.radio_ready = true;
        portEXIT_CRITICAL(&stats_lock);
        return;
    }

    // Init() pulsa o RST (GPIO 14) e reconfigura o rádio.
    Serial.print("[TT&C] ERRO: SX1276 sem resposta ou fora de RX, reinicializando.\n");
    radio_healthy = radio.Init();
    portENTER_CRITICAL(&stats_lock);
    diagnostics.radio_ready = radio_healthy;
    stats.radio_resets_count++;
    portEXIT_CRITICAL(&stats_lock);
}

// Privado:
void Ttc::SendBeaconIfDue(
) {
    uint32_t period_ms = beacon_period_ms;
    BEACON_BUILDER builder = beacon_builder;
    if (period_ms == 0 || builder == nullptr) return;
    if (millis() - last_beacon_ms < period_ms) return;
    last_beacon_ms = millis();

    char json[ttc::max_frame_len + 1];
    size_t len = builder(json, sizeof(json));
    if (len > 0) Send(json, len);
}

// Privado:
void Ttc::PollTransport(
    LinkTransport& transport,
    LINK_ORIGIN origin
) {
    char buffer[ttc::max_frame_len + 1];
    int len = transport.Receive((uint8_t*)buffer, ttc::max_frame_len);
    if (len == 0) return;

    if (len < 0) {
        CountRxError();
        Serial.print("[TT&C] Quadro corrompido ou longo demais descartado.\n");
        return;
    }
    buffer[len] = '\0';

    // Sinal só no RF, lido já (vale até o próximo pacote) e fora da seção
    // crítica (o SPI usa mutex). Só é guardado se o quadro for um TC: o
    // contrato define rssi/snr da TM como os do último TC recebido.
    bool has_signal = origin == LINK_RF;
    int16_t rssi = has_signal ? radio.GetRssi() : 0;
    float snr = has_signal ? radio.GetSnr() : 0.0f;

    // Confirmação de entrega do solo: não é TC, não tem seq nem ACK.
    ttc::DELIVERY_ID confirmed;
    ttc::CONFIRMATION_PARSE confirmation = ttc::ParseConfirmation(buffer, (size_t)len, confirmed);
    if (confirmation == ttc::CF_VALID) {
        HandleConfirmation(confirmed, origin);
        return;
    }
    if (confirmation == ttc::CF_INVALID) {
        CountRxError();
        Serial.printf("[TT&C] Confirmacao de entrega invalida descartada, %d bytes.\n", len);
        return;
    }

    // Envelope inválido: NACK se o seq é legível; senão, só descarta.
    ttc::TC_INFO info;
    bool valid = ttc::ParseTelecommand(buffer, (size_t)len, info);
    if (has_signal && (valid || info.has_seq)) {
        portENTER_CRITICAL(&stats_lock);
        stats.last_rssi = rssi;
        stats.last_snr = snr;
        portEXIT_CRITICAL(&stats_lock);
    }
    if (!valid) {
        CountRxError();
        Serial.printf("[TT&C] TC rejeitado (%s), %d bytes.\n", ttc::StatusName(info.status), len);
        if (info.has_seq) {
            const char* name = info.cmd_name[0] != '\0' ? info.cmd_name : "unknown";
            EnqueueAck(info.seq, name, info.status, -1, origin);
        }
        return;
    }

    HandleTelecommand(info, buffer, (size_t)len, origin);
}

// Privado:
void Ttc::HandleConfirmation(
    const ttc::DELIVERY_ID& id,
    LINK_ORIGIN origin
) {
    MarkContact(origin);
    // Contato válido com o solo (REQ-OBC-011), mesmo que repetido.
    bool queued = xQueueSend(confirm_queue, &id, 0) == pdTRUE;
    portENTER_CRITICAL(&stats_lock);
    stats.last_tc_ms = millis();
    if (queued) stats.confirmations_count++;
    else stats.confirmations_dropped++;
    portEXIT_CRITICAL(&stats_lock);
    if (!queued) Serial.print("[TT&C] Fila de confirmacoes cheia, confirmacao descartada.\n");
}

// Privado:
void Ttc::MarkContact(
    LINK_ORIGIN origin
) {
    portENTER_CRITICAL(&stats_lock);
    LINK_CONTACT& contact = origin == LINK_RF ? diagnostics.rf_contact : diagnostics.wired_contact;
    contact = {true, millis()};
    portEXIT_CRITICAL(&stats_lock);
}

// Privado:
void Ttc::HandleTelecommand(
    const ttc::TC_INFO& info,
    const char* json,
    size_t len,
    LINK_ORIGIN origin
) {
    MarkContact(origin);
    // Contato válido com o solo, mesmo que o TC seja repetido (REQ-OBC-011).
    portENTER_CRITICAL(&stats_lock);
    stats.last_tc_ms = millis();
    portEXIT_CRITICAL(&stats_lock);

    xSemaphoreTake(dedup_mutex, portMAX_DELAY);
    int slot = FindDedupSlot(info.seq);

    // TC repetido (retentativa da estação terrena): não reexecuta.
    // - Já respondido: reenvia o mesmo ACK pelo meio da retentativa.
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
        if (acked) EnqueueFrame(ack_queue, cached.json, cached.len, origin);
        return;
    }

    // Registra o TC num slot livre, expirado ou já respondido, nessa ordem.
    size_t index = FreeDedupSlot();
    dedup[index] = {};
    dedup[index].used = true;
    dedup[index].seq = info.seq;
    dedup[index].origin = origin;
    dedup[index].rx_ms = millis();
    uint32_t rx_ms = dedup[index].rx_ms;
    xSemaphoreGive(dedup_mutex);

    TC_FRAME frame;
    frame.seq = info.seq;
    frame.cmd = info.cmd;
    frame.rx_ms = rx_ms;
    frame.origin = origin;
    frame.len = (uint8_t)len;
    memcpy(frame.json, json, len);
    frame.json[len] = '\0';

    if (xQueueSend(rx_queue, &frame, 0) != pdTRUE) {
        // Libera o slot para que a retentativa seja aceita.
        xSemaphoreTake(dedup_mutex, portMAX_DELAY);
        dedup[index].used = false;
        xSemaphoreGive(dedup_mutex);
        CountRxError();
        Serial.print("[TT&C] Fila de TC cheia, comando descartado.\n");
        EnqueueAck(info.seq, info.cmd_name, ttc::ERR_STATE_REJECTED, -1, origin);
        return;
    }

    portENTER_CRITICAL(&stats_lock);
    stats.rx_packets_count++;
    if (origin == LINK_WIRED) stats.wired_rx_packets_count++;
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
int Ttc::FindAckSlot(
    uint16_t seq
) {
    // Para o ACK, um TC ainda sem resposta continua valendo após dedup_expiry_ms:
    // execução lenta não pode perder o meio de origem nem o "q". Entre slots com
    // o mesmo seq, vale o mais recente.
    int found = -1;
    uint32_t now_ms = millis();
    for (size_t i = 0; i < ttc_dedup_slots; i++) {
        if (!dedup[i].used || dedup[i].seq != seq) continue;
        if (dedup[i].acked && now_ms - dedup[i].rx_ms >= dedup_expiry_ms) continue;
        if (found < 0 || now_ms - dedup[i].rx_ms < now_ms - dedup[found].rx_ms) found = (int)i;
    }
    return found;
}

// Privado:
size_t Ttc::FreeDedupSlot(
) {
    // Prioridade: livre > expirado > respondido mais antigo > mais antigo.
    // Um TC em execução só é sobrescrito com os 8 slots ocupados por TCs sem ACK.
    uint32_t now_ms = millis();
    int expired = -1, acked = -1, oldest = -1;
    for (size_t i = 0; i < ttc_dedup_slots; i++) {
        if (!dedup[i].used) return i;
        uint32_t age = now_ms - dedup[i].rx_ms;
        if (age >= dedup_expiry_ms && (dedup[i].acked || age >= unacked_expiry_ms)) {
            if (expired < 0) expired = (int)i;
            continue;
        }
        if (dedup[i].acked && (acked < 0 || age > now_ms - dedup[acked].rx_ms)) acked = (int)i;
        if (oldest < 0 || age > now_ms - dedup[oldest].rx_ms) oldest = (int)i;
    }
    if (expired >= 0) return (size_t)expired;
    if (acked >= 0) return (size_t)acked;
    return (size_t)oldest;
}

// Privado:
void Ttc::TransmitNext(
) {
    // Teste de hold_tx e reserva de transmitting são atômicos com HoldTransmit():
    // sem isso, HoldTransmit(true) poderia retornar true entre o teste e o
    // início do TX, liberando a carga de alto consumo junto com o pico (DP §5.2).
    portENTER_CRITICAL(&hold_lock);
    bool blocked = hold_tx;
    if (!blocked) transmitting = true;
    portEXIT_CRITICAL(&hold_lock);
    if (blocked) return;

    TransmitFrame();
    transmitting = false;
}

// Privado:
void Ttc::TransmitFrame(
) {
    // ACKs primeiro: uma rajada de telemetria não atrasa a confirmação de TC.
    TX_FRAME frame;
    bool has_frame = xQueueReceive(ack_queue, &frame, 0) == pdTRUE
                  || xQueueReceive(tx_queue, &frame, 0) == pdTRUE;
    if (!has_frame) return;

    // Cabo primeiro: é instantâneo e não espera o airtime do rádio.
    uint8_t sent_mask = 0;
    if ((frame.dest & LINK_WIRED) && umbilical != nullptr && TransmitTo(*umbilical, wired_counter, frame)) {
        sent_mask |= LINK_WIRED;
        portENTER_CRITICAL(&stats_lock);
        stats.wired_tx_packets_count++;
        portEXIT_CRITICAL(&stats_lock);
    }
    if ((frame.dest & LINK_RF) == 0) {
        PublishTransmitEvent(frame, sent_mask);
        return;
    }

    bool sent = TransmitTo(radio, rf_counter, frame);
    if (sent) sent_mask |= LINK_RF;
    // Falha de RF com o rádio tido como saudável (ex.: timeout de TX): verifica
    // já no próximo Update(). Rádio já em falha segue o período normal, sem
    // reinicializar a cada quadro.
    else if (radio_healthy) last_health_ms = millis() - health_period_ms;

    portENTER_CRITICAL(&stats_lock);
    if (sent) stats.tx_packets_count++;
    else stats.tx_errors_count++;
    portEXIT_CRITICAL(&stats_lock);
    PublishTransmitEvent(frame, sent_mask);
}

// Privado:
void Ttc::PublishTransmitEvent(
    const TX_FRAME& frame,
    uint8_t sent
) {
    if (!frame.tracked) return;

    // A vaga foi reservada em SendDelivery(); a fila comporta todas as reservas.
    // Com RF no destino, só o RF conta como sucesso: a cópia no cabo sempre
    // "termina" em voo, mesmo sem nada ligado, e esconderia a falha de RF.
    bool success = (frame.dest & LINK_RF) ? (sent & LINK_RF) != 0 : sent != 0;
    TX_EVENT event = {frame.id, success, frame.dest, sent};
    if (xQueueSend(event_queue, &event, 0) != pdTRUE) {
        portENTER_CRITICAL(&stats_lock);
        if (tracked_pending > 0) tracked_pending--;
        portEXIT_CRITICAL(&stats_lock);
        Serial.print("[TT&C] ERRO: evento de entrega sem vaga reservada.\n");
    }
}

// Privado:
bool Ttc::TransmitTo(
    LinkTransport& transport,
    uint16_t& counter,
    const TX_FRAME& frame
) {
    // Contador por meio, atribuído no TX: sequência estrita para a detecção de perdas.
    char json[sizeof(frame.json)];
    size_t len = frame.len;
    memcpy(json, frame.json, len + 1);
    if (!ttc::InsertCounter(json, len, sizeof(json), counter)) {
        Serial.printf("[TT&C] Pacote de %u bytes sem espaco para o contador.\n", frame.len);
        return false;
    }
    if (!transport.Send((const uint8_t*)json, len)) {
        Serial.printf("[TT&C] Falha no TX de %u bytes.\n", (unsigned)len);
        return false;
    }
    // Só avança com o quadro no ar: um "n" pulado conta como perda na estação.
    counter++;
    return true;
}

// Privado:
uint8_t Ttc::DownlinkDestinations(
) {
    uint8_t dest = LINK_RF;
    if (umbilical != nullptr && umbilical_enabled) dest |= LINK_WIRED;
    return dest;
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
    int slot = FindAckSlot(out.seq);
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
    int slot = FindAckSlot(seq);
    uint8_t dest = slot >= 0 ? dedup[slot].origin : DownlinkDestinations();
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
    return EnqueueFrame(ack_queue, ack, len, dest);
}

// Privado:
bool Ttc::EnqueueAck(
    uint16_t seq,
    const char* cmd_name,
    ttc::ACK_STATUS status,
    int32_t queue_ms,
    uint8_t dest
) {
    char ack[ttc::max_ack_len];
    size_t len = ttc::BuildAck(seq, cmd_name, status, queue_ms, ack, sizeof(ack));
    if (len == 0) return false;
    return EnqueueFrame(ack_queue, ack, len, dest);
}

// Privado:
bool Ttc::EnqueueFrame(
    QueueHandle_t queue,
    const char* json,
    size_t len,
    uint8_t dest
) {
    TX_FRAME item = {};
    item.dest = dest;
    item.len = (uint8_t)len;
    memcpy(item.json, json, len);
    item.json[len] = '\0';
    return EnqueueItem(queue, item);
}

// Privado:
bool Ttc::EnqueueItem(
    QueueHandle_t queue,
    const TX_FRAME& item
) {
    if (queue == nullptr) return false;
    // A task do enlace é a única consumidora das filas de descida: esperar
    // nela só a deixaria surda para RX/TX e descartaria o pacote do mesmo jeito.
    bool own_task = task_handle != nullptr && xTaskGetCurrentTaskHandle() == task_handle;
    if (xQueueSend(queue, &item, own_task ? 0 : enqueue_wait) != pdTRUE) {
        Serial.print("[TT&C] Fila de descida cheia, pacote descartado.\n");
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
    return EnqueueFrame(tx_queue, json, len, DownlinkDestinations());
}

// Público:
bool Ttc::Send(
    const char* json,
    size_t len,
    uint8_t dest
) {
    if (json == nullptr || len < 2 || len > ttc::max_body_len) {
        Serial.printf("[TT&C] Pacote de %u bytes rejeitado (limite %u).\n", (unsigned)len, (unsigned)ttc::max_body_len);
        return false;
    }
    bool valid = dest != 0 && (dest & ~(LINK_RF | LINK_WIRED)) == 0
              && ((dest & LINK_WIRED) == 0 || umbilical != nullptr);
    if (!valid) {
        Serial.printf("[TT&C] Destino 0x%02x indisponivel.\n", (unsigned)dest);
        return false;
    }
    return EnqueueFrame(tx_queue, json, len, dest);
}

// Público:
bool Ttc::SendDelivery(
    const char* json,
    size_t len,
    const ttc::DELIVERY_ID& id
) {
    if (json == nullptr || len < 2 || len > ttc::max_body_len || id.boot == 0 || id.sequence == 0) {
        Serial.printf("[TT&C] Entrega de %u bytes rejeitada.\n", (unsigned)len);
        return false;
    }

    // O "id" do JSON é o que a estação confirma: diferente do id do evento, o
    // OBC receberia confirmação de outra entrega e retransmitiria sem fim.
    ttc::DELIVERY_ID carried;
    if (!ttc::PacketDeliveryId(json, len, carried) || carried.boot != id.boot || carried.sequence != id.sequence) {
        Serial.print("[TT&C] Entrega rejeitada: \"id\" do JSON ausente ou diferente do ID informado.\n");
        return false;
    }

    // Reserva a vaga do evento antes de enfileirar: toda entrega aceita gera um evento.
    bool reserved = false;
    portENTER_CRITICAL(&stats_lock);
    if (tracked_pending < ttc_tracked_slots) {
        tracked_pending++;
        reserved = true;
    }
    portEXIT_CRITICAL(&stats_lock);
    if (!reserved) return false;

    TX_FRAME item = {};
    item.dest = DownlinkDestinations();
    item.tracked = true;
    item.id = id;
    item.len = (uint8_t)len;
    memcpy(item.json, json, len);
    item.json[len] = '\0';
    if (EnqueueItem(tx_queue, item)) return true;

    portENTER_CRITICAL(&stats_lock);
    tracked_pending--;
    portEXIT_CRITICAL(&stats_lock);
    return false;
}

// Público:
bool Ttc::SendAircraft(
    const ttc::AIRCRAFT_DATA& aircraft,
    uint32_t available,
    const ttc::DELIVERY_ID& id
) {
    char json[ttc::max_frame_len + 1];
    size_t len = ttc::BuildAircraft(aircraft, json, sizeof(json), available, &id);
    if (len == 0) {
        Serial.print("[TT&C] Aeronave invalida ou acima do limite do quadro.\n");
        return false;
    }
    return SendDelivery(json, len, id);
}

// Público:
bool Ttc::ReceiveTransmitEvent(
    TX_EVENT& out,
    TickType_t wait
) {
    if (event_queue == nullptr) return false;
    if (xQueueReceive(event_queue, &out, wait) != pdTRUE) return false;

    portENTER_CRITICAL(&stats_lock);
    if (tracked_pending > 0) tracked_pending--;
    portEXIT_CRITICAL(&stats_lock);
    return true;
}

// Público:
bool Ttc::ReceiveConfirmation(
    ttc::DELIVERY_ID& out,
    TickType_t wait
) {
    if (confirm_queue == nullptr) return false;
    return xQueueReceive(confirm_queue, &out, wait) == pdTRUE;
}

// Público:
bool Ttc::SendTelemetry(
    const ttc::TELEMETRY_DATA& tm,
    uint32_t available,
    ttc::TELEMETRY_DATA* queued_tm
) {
    // Campos do enlace vêm do próprio TT&C; contadores em 16 bits com volta.
    LINK_STATS link = GetLinkStats();
    ttc::TELEMETRY_DATA filled = tm;
    filled.rssi = link.last_rssi;
    filled.snr = link.last_snr;
    filled.rx_count = (uint16_t)link.rx_packets_count;
    filled.error_count = (uint16_t)link.rx_errors_count;
    filled.radio_resets = (uint16_t)link.radio_resets_count;

    char json[ttc::max_frame_len + 1];
    size_t len = ttc::BuildTelemetry(filled, json, sizeof(json), available);
    if (len == 0) {
        Serial.print("[TT&C] TM excede o limite do quadro.\n");
        return false;
    }
    if (!Send(json, len)) return false;
    if (queued_tm != nullptr) *queued_tm = filled;
    return true;
}

// Público:
void Ttc::EnableBeacon(
    uint32_t period_ms,
    BEACON_BUILDER builder
) {
    beacon_builder = builder;
    beacon_period_ms = period_ms;
    Notify();
}

// Público:
bool Ttc::HoldTransmit(
    bool hold,
    uint32_t wait_ms
) {
    // Contagem de pedidos: com duas tasks suspendendo, o TX só volta quando
    // as duas liberarem. Liberação sem pedido pendente não tem efeito.
    portENTER_CRITICAL(&hold_lock);
    if (hold && hold_count < UINT8_MAX) hold_count++;
    else if (!hold && hold_count > 0) hold_count--;
    hold_tx = hold_count > 0;
    bool still_held = hold_tx;
    portEXIT_CRITICAL(&hold_lock);
    if (!hold) {
        if (!still_held) Notify();
        return !transmitting;
    }

    // Um TX já iniciado termina em até radio_tx_timeout_ms.
    uint32_t start_ms = millis();
    while (transmitting && millis() - start_ms < wait_ms) vTaskDelay(pdMS_TO_TICKS(5));
    return !transmitting;
}
