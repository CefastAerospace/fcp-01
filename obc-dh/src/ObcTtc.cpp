#include "ObcTtc.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcTtc.h".
 */

namespace obc_ttc {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    const RADIO_PINS radio_pins = {
        pinout::radio_sck, pinout::radio_miso, pinout::radio_mosi,
        pinout::radio_cs, pinout::radio_rst, pinout::radio_dio0
    };
    Sx1276Radio radio(radio_pins);
    UmbilicalLink umbilical(Serial, 115200);
    Ttc link(radio, &umbilical);
    bool init_attempted = false;
    bool active = false;
    bool rf_seen = false;
    constexpr uint32_t service_wait_ms = 20;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Mantém diagnóstico de recusa separado da execução de cada comando.
    void Acknowledge(
        uint16_t sequence,
        ttc::COMMAND_ID command,
        ttc::ACK_STATUS status
    ) {
        if (!link.SendAck(sequence, command, status)) {
            Serial.printf("[OBC] TT&C: falha ao enfileirar ACK; seq=%u; status=%s\n",
                sequence, ttc::StatusName(status));
        }
    }

    // ACK ok significa resposta efetivamente enfileirada, incluindo lg de erro.
    void SendLogResponse(
        obc_downlink::LOG_RESPONSE& response
    ) {
        if (!response.length) {
            const uint16_t sequence = response.sequence;
            const ttc::LOG_REQUEST request = response.request;
            obc_downlink::BuildLogFailure(sequence, request, ttc::LOG_BUSY, response);
        }
        const bool queued = response.length
            && link.Send(response.json, response.length, response.request.dest);
        Acknowledge(response.sequence, ttc::CMD_REQUEST_LOG,
            queued ? ttc::ACK_OK : ttc::ERR_STATE_REJECTED);
    }

    // O escritor recebe só argumentos validados; origem resolve o destino padrão.
    void RequestLog(
        const TC_FRAME& tc
    ) {
        ttc::LOG_REQUEST request = {};
        const auto status = ttc::ParseLogRequest(tc.json, tc.len, request);
        if (status != ttc::ACK_OK) {
            Acknowledge(tc.seq, tc.cmd, status);
            return;
        }
        if (request.dest == ttc::LOG_DEST_ORIGIN) request.dest = tc.origin;
        const auto admission = obc_flow::esp32::RequestLog(tc.seq, request);
        if (admission == obc_storage::STATUS::OK) return;
        obc_downlink::LOG_RESPONSE response = {};
        obc_downlink::BuildLogFailure(tc.seq, request,
            admission == obc_storage::STATUS::BUSY ? ttc::LOG_BUSY : ttc::LOG_NO_MEDIA, response);
        SendLogResponse(response);
    }

    // Consome resultado sem esperar pelo SD; expiração é resolvida pela caixa.
    void ServiceLogResponse(
    ) {
        obc_downlink::LOG_RESPONSE response = {};
        if (obc_flow::esp32::TakeLogResponse(response)) SendLogResponse(response);
    }

    // As bancadas históricas conservam o consumidor simulado exclusivo.
    void ServiceDelivery(
    ) {
#if !defined(OBC_FLOW_BENCH) || defined(OBC_TTC_INTEGRATION_BENCH)
        TX_EVENT event = {};
        for (size_t index = 0; index < ttc_tracked_slots && link.ReceiveTransmitEvent(event, 0); ++index) {
            obc_flow::esp32::FinishDeliveryTransmit({event.id.boot, event.id.sequence}, event.success);
        }
        ttc::DELIVERY_ID confirmed = {};
        for (size_t index = 0; index < ttc_confirm_slots && link.ReceiveConfirmation(confirmed, 0); ++index) {
            obc_flow::esp32::ConfirmDelivery({confirmed.boot, confirmed.sequence});
        }
        obc_delivery::DELIVERY delivery = {};
        if (!obc_flow::esp32::BeginDeliveryTransmit(obc_downlink::confirmation_timeout_ms, delivery)) return;
        ttc::AIRCRAFT_DATA aircraft = {};
        uint32_t available = 0;
        const ttc::DELIVERY_ID id = {delivery.id.boot, delivery.id.sequence};
        const bool queued = obc_downlink::MapAircraft(delivery, aircraft, available)
            && link.SendAircraft(aircraft, available, id);
        // Uma recusa não reserva evento TX na biblioteca.
        if (!queued) obc_flow::esp32::FinishDeliveryTransmit(delivery.id, false);
#endif
    }

    // Usa UTC/ac do fluxo e a mesma amostra de enlace para envio e registro.
    bool SendCurrentTelemetry(
    ) {
        uint64_t sampled_ms = 0;
        obc_data::TELEMETRY_DATA local = obc_flow::esp32::ComposeTelemetry(sampled_ms);
        ttc::TELEMETRY_DATA tm = {};
        tm.mode = ttc::MODE_BENCH;
        tm.timestamp = local.timestamp;
        tm.aircraft_count = local.aircraft_count;
        uint32_t available = ttc::TM_MODE | ttc::TM_RX_COUNT
            | ttc::TM_ERROR_COUNT | ttc::TM_RADIO_RESETS;
        if (local.available_fields & obc_data::TM_TIMESTAMP) available |= ttc::TM_TIMESTAMP;
        if (local.available_fields & obc_data::TM_AIRCRAFT) available |= ttc::TM_AIRCRAFT;
        if (rf_seen) available |= ttc::TM_RSSI | ttc::TM_SNR;
        ttc::TELEMETRY_DATA queued = {};
        if (!link.SendTelemetry(tm, available, &queued)) return false;
        local.rx_count = queued.rx_count;
        local.error_count = queued.error_count;
        local.radio_resets = queued.radio_resets;
        local.available_fields |= obc_data::TM_RX_COUNT | obc_data::TM_ERROR_COUNT | obc_data::TM_RADIO_RESETS;
        if (rf_seen) {
            local.rssi = queued.rssi;
            local.snr = queued.snr;
            local.available_fields |= obc_data::TM_RSSI | obc_data::TM_SNR;
        }
        obc_flow::esp32::RecordTelemetry(local, sampled_ms);
        return true;
    }

    // O TT&C já valida o envelope; esta camada valida os argumentos e
    // confirma pedidos de TM somente quando a descida aceitou o pacote.
    ttc::ACK_STATUS ExecuteTelecommand(
        const TC_FRAME& tc
    ) {
        switch (tc.cmd) {
            case ttc::CMD_PING:
            case ttc::CMD_GET_STATUS:
            case ttc::CMD_REQUEST_TELEMETRY:
            case ttc::CMD_SET_TIME:
                break;
            default:
                return ttc::ERR_INVALID_CMD;
        }

        JsonDocument doc;
        if (deserializeJson(doc, tc.json, tc.len) != DeserializationError::Ok) {
            return ttc::ERR_BAD_JSON;
        }
        if (tc.cmd == ttc::CMD_SET_TIME) {
            if (!doc["args"]["unix"].is<uint32_t>() || doc["args"].size() != 1) {
                return ttc::ERR_INVALID_PARAM;
            }
            const uint32_t delay_ms = tc.origin == LINK_WIRED ? 0 : obc_time::reference_delay_ms;
            return obc_flow::esp32::SetTime(doc["args"]["unix"].as<uint32_t>(), tc.rx_ms, delay_ms)
                ? ttc::ACK_OK : ttc::ERR_STATE_REJECTED;
        }
        if (!doc["args"].isNull()
            && (!doc["args"].is<JsonObject>() || doc["args"].size() != 0)) {
            return ttc::ERR_INVALID_PARAM;
        }
        if (tc.cmd == ttc::CMD_PING) return ttc::ACK_OK;
        return SendCurrentTelemetry() ? ttc::ACK_OK : ttc::ERR_STATE_REJECTED;
    }

} // namespace

    // ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

    bool Init(
    ) {
        if (init_attempted) return active;
        init_attempted = true;

        const bool radio_ready = link.Init();
        const bool wired_ready = umbilical.IsReady();
        // Instância nova: Init() só inicializa o umbilical após criar todas
        // as filas e o mutex. Não confundir IsHealthy() com prontidão USB.
        if (radio_ready || wired_ready) active = link.Start();
        Serial.printf("[OBC] TT&C: radio=%s; umbilical=%s; tarefa=%s\n",
            radio_ready ? "OK" : "indisponivel",
            wired_ready ? "OK" : "indisponivel",
            active ? "OK" : "inativa");
        return active;
    }

    void Service(
    ) {
        if (!active) {
            delay(service_wait_ms);
            return;
        }
        ServiceLogResponse();
        TC_FRAME tc = {};
        if (link.ReceiveTelecommand(tc, pdMS_TO_TICKS(service_wait_ms))) {
            if (tc.origin == LINK_RF) rf_seen = true;
            if (tc.cmd == ttc::CMD_REQUEST_LOG) RequestLog(tc);
            else Acknowledge(tc.seq, tc.cmd, ExecuteTelecommand(tc));
        }
        ServiceLogResponse();
        ServiceDelivery();
    }

} // namespace obc_ttc
