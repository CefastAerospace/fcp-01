#include "ObcTtc.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcTtc.h".
 */

namespace obc_ttc {
namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

#ifdef OBC_WATCHDOG_BENCH_CASE
    obc_watchdog_bench::Radio radio;
#elif defined(OBC_RESOURCES_BENCH)
    obc_resources_bench::Radio radio;
#else
    const RADIO_PINS radio_pins = {
        pinout::radio_sck, pinout::radio_miso, pinout::radio_mosi,
        pinout::radio_cs, pinout::radio_rst, pinout::radio_dio0
    };
    Sx1276Radio radio(radio_pins);
#endif
#ifdef OBC_BUFFERED_FRAMEWORK_LOG
    UmbilicalLink umbilical(Serial, 115200, UmbilicalLink::PORT_MODE::REUSE);
#else
    UmbilicalLink umbilical(Serial, 115200);
#endif
    Ttc link(radio, &umbilical);
    bool init_attempted = false;
    bool active = false;
    bool rf_seen = false;
    constexpr uint32_t service_wait_ms = 20;
    obc_modes::Coordinator operating_modes;
    MODE_SERVICES mode_services = {};
    obc_modes::ACTIONS mode_actions = {};

    struct MODE_SAMPLE {
        uint64_t now_ms;
        obc_modes::SUBSYSTEM_STATUS subsystems;
        obc_modes::MISSION_STATUS mission;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Usa os valores do modelo local após verificar todos os modos do enlace.
    constexpr ttc::OP_MODE ToTtcMode(
        obc_data::MODE mode
    ) {
        return static_cast<ttc::OP_MODE>(mode);
    }

    static_assert(ToTtcMode(obc_data::MODE::BENCH) == ttc::MODE_BENCH
        && ToTtcMode(obc_data::MODE::SAFE) == ttc::MODE_SAFE
        && ToTtcMode(obc_data::MODE::DETUMBLING) == ttc::MODE_DETUMBLING
        && ToTtcMode(obc_data::MODE::SUN_POINTING) == ttc::MODE_SUN_POINTING
        && ToTtcMode(obc_data::MODE::MISSION) == ttc::MODE_MISSION
        && ToTtcMode(obc_data::MODE::LOW_POWER) == ttc::MODE_LOW_POWER,
        "Os modos da TM e do diário devem corresponder");

    // O diagnóstico serial informa inclusive uma recusa do escritor.
    bool RecordMode(
        const obc_modes::EVENT& event
    ) {
        char description[sizeof(obc_data::EVENT_DATA::description)];
        std::snprintf(description, sizeof(description), "%s>%s %s",
            ttc::ModeName(ToTtcMode(event.from)), ttc::ModeName(ToTtcMode(event.to)),
            obc_modes::CauseName(event.cause));
        const bool queued = obc_flow::esp32::RecordObcEvent(obc_modes::event_code,
            obc_modes::IsFault(event.cause), event.at_ms, description);
        Serial.printf("[MODES] ms=%llu queued=%u %s\n",
            static_cast<unsigned long long>(event.at_ms), queued ? 1U : 0U, description);
        return queued;
    }

    // Só registra mudanças relevantes do episódio, antes de entregar o novo estado ao coordenador.
    void ApplyProtection(
        MODE_SAMPLE& sample
    ) {
        static obc_protection::RESULT previous = {};
        const auto current = mode_services.read_protection();
        const bool boot_ready = sample.subsystems.boot_ready;
        sample.subsystems.boot_ready = boot_ready && current.starts_allowed;
        sample.subsystems.protection_active = current.protection_active;
        sample.subsystems.recovery_allowed = boot_ready && current.recovery_allowed;

        const bool fault = current.protection_active && (!previous.protection_active
            || current.unavailable != previous.unavailable || current.below_limit != previous.below_limit
            || current.above_limit != previous.above_limit);
        const bool recovery_changed = operating_modes.IsProtectionInEffect()
            && current.recovery_allowed != previous.recovery_allowed;
        previous = current;
        if (!fault && !recovery_changed) return;

        char description[sizeof(obc_data::EVENT_DATA::description)];
        const char* change = fault ? "active" : current.recovery_allowed ? "recovery_allowed" : "recovery_lost";
        std::snprintf(description, sizeof(description), "%s unavailable=%02x low=%02x high=%02x",
            change, static_cast<unsigned>(current.unavailable), static_cast<unsigned>(current.below_limit),
            static_cast<unsigned>(current.above_limit));
        const uint64_t now_ms = sample.now_ms;
        const bool queued = obc_flow::esp32::RecordObcEvent(obc_protection::event_code, fault, now_ms, description);
        Serial.printf("[PROTECTION] ms=%llu queued=%u %s\n",
            static_cast<unsigned long long>(now_ms), queued ? 1U : 0U, description);
    }

    // Captura uma vez por ciclo e aplica eventos internos antes do próximo TC.
    MODE_SAMPLE ServiceModes(
    ) {
        MODE_SAMPLE sample = {};
        if (mode_services.read_status) {
            sample.subsystems = mode_services.read_status();
            sample.mission = obc_flow::esp32::GetMissionStatus(mode_services.max_snapshot_age_ms, sample.now_ms);
            if (mode_services.read_protection) ApplyProtection(sample);
            operating_modes.Service(sample.now_ms, sample.subsystems, sample.mission, mode_actions);
        }
        return sample;
    }

    // Seleciona a mesma ação para o comando direto e seu alias set_mode.
    ttc::ACK_STATUS ExecuteModeTelecommand(
        const TC_FRAME& tc,
        const MODE_SAMPLE& sample
    ) {
        if (!mode_services.read_status) return ttc::ERR_INVALID_CMD;

        JsonDocument doc;
        if (deserializeJson(doc, tc.json, tc.len) != DeserializationError::Ok) return ttc::ERR_BAD_JSON;
        const JsonVariantConst args = doc["args"];
        using obc_modes::COMMAND;
        COMMAND command;
        if (tc.cmd == ttc::CMD_SET_MODE) {
            const JsonString name = args["mode"].as<JsonString>();
            if (args.size() != 1 || name.isNull()) return ttc::ERR_INVALID_PARAM;
            if (name == JsonString(ttc::ModeName(ttc::MODE_MISSION))) command = COMMAND::START_MISSION;
            else if (name == JsonString(ttc::ModeName(ttc::MODE_DETUMBLING))) command = COMMAND::START_ADCS;
            else if (name == JsonString(ttc::ModeName(ttc::MODE_SAFE))) command = COMMAND::ENTER_SAFE;
            else if (name == JsonString(ttc::ModeName(ttc::MODE_BENCH))
                    || name == JsonString(ttc::ModeName(ttc::MODE_SUN_POINTING))
                    || name == JsonString(ttc::ModeName(ttc::MODE_LOW_POWER))) return ttc::ERR_STATE_REJECTED;
            else return ttc::ERR_INVALID_PARAM;
        } else {
            if (!args.isNull() && (!args.is<JsonObjectConst>() || args.size() != 0)) return ttc::ERR_INVALID_PARAM;
            switch (tc.cmd) {
                case ttc::CMD_START_MISSION: command = COMMAND::START_MISSION; break;
                case ttc::CMD_STOP_MISSION: command = COMMAND::STOP_MISSION; break;
                case ttc::CMD_ADCS_START: command = COMMAND::START_ADCS; break;
                case ttc::CMD_ADCS_STOP: command = COMMAND::STOP_ADCS; break;
                case ttc::CMD_ENTER_SAFE: command = COMMAND::ENTER_SAFE; break;
                case ttc::CMD_EXIT_SAFE: command = COMMAND::EXIT_SAFE; break;
                default: return ttc::ERR_INVALID_CMD;
            }
        }
        return operating_modes.Execute(command, sample.now_ms, sample.subsystems, sample.mission, mode_actions)
            ? ttc::ACK_OK : ttc::ERR_STATE_REJECTED;
    }

    // O consumidor encaminha uma cópia; o supervisor nunca acessa o rádio.
    void ServiceResourceHealth(
    ) {
        static bool sampled = false;
        static uint32_t last_ms = 0;
        const uint32_t now_ms = millis();
        if (sampled && now_ms - last_ms < obc_resource_health::ttc_period_ms) return;
        sampled = true;
        last_ms = now_ms;
        obc_flow::esp32::ReportResourceHealth(link.GetDiagnostics());
    }

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
        const TC_FRAME& tc,
        bool pause_admission
    ) {
        ttc::LOG_REQUEST request = {};
        const auto status = ttc::ParseLogRequest(tc.json, tc.len, request);
        if (status != ttc::ACK_OK) {
            Acknowledge(tc.seq, tc.cmd, status);
            return;
        }
        if (pause_admission) {
            Acknowledge(tc.seq, tc.cmd, ttc::ERR_STATE_REJECTED);
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
        bool pause_admission
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
        if (pause_admission) return;
        obc_delivery::DELIVERY delivery = {};
        if (!obc_flow::esp32::BeginDeliveryTransmit(obc_downlink::confirmation_timeout_ms, delivery)) return;
        ttc::AIRCRAFT_DATA aircraft = {};
        uint32_t available = 0;
        const ttc::DELIVERY_ID id = {delivery.id.boot, delivery.id.sequence};
        const bool queued = obc_downlink::MapAircraft(delivery, aircraft, available)
            && link.SendAircraft(aircraft, available, id);
        // Uma recusa não reserva evento TX na biblioteca.
        if (!queued) obc_flow::esp32::FinishDeliveryTransmit(delivery.id, false);
#else
        (void)pause_admission;
#endif
    }

    // Usa UTC/ac do fluxo e a mesma amostra de enlace para envio e registro.
    bool SendCurrentTelemetry(
    ) {
        uint64_t sampled_ms = 0;
        obc_data::TELEMETRY_DATA local = obc_flow::esp32::ComposeTelemetry(sampled_ms);
        local.mode = operating_modes.GetMode();
        ttc::TELEMETRY_DATA tm = {};
        tm.mode = ToTtcMode(local.mode);
        tm.timestamp = local.timestamp;
        tm.aircraft_count = local.aircraft_count;
        uint32_t available = ttc::TM_MODE | ttc::TM_RX_COUNT
            | ttc::TM_ERROR_COUNT | ttc::TM_RADIO_RESETS;
        if (local.available_fields & obc_data::TM_TIMESTAMP) available |= ttc::TM_TIMESTAMP;
        if (local.available_fields & obc_data::TM_AIRCRAFT) available |= ttc::TM_AIRCRAFT;
        if (rf_seen) available |= ttc::TM_RSSI | ttc::TM_SNR;
#ifdef OBC_HOUSEKEEPING_BENCH
        // A disponibilidade térmica do enlace é conjunta; a local continua individual.
        tm.vbat = local.vbat;
        tm.ibat = local.ibat;
        tm.vsolar = local.vsolar;
        tm.isolar = local.isolar;
        for (size_t index = 0; index < obc_data::thermal_sensors; ++index) tm.temps[index] = local.temps[index];
        if (local.available_fields & obc_data::TM_VBAT) available |= ttc::TM_VBAT;
        if (local.available_fields & obc_data::TM_IBAT) available |= ttc::TM_IBAT;
        if (local.available_fields & obc_data::TM_VSOLAR) available |= ttc::TM_VSOLAR;
        if (local.available_fields & obc_data::TM_ISOLAR) available |= ttc::TM_ISOLAR;
        if (obc_housekeeping::AllTemperaturesAvailable(local)) available |= ttc::TM_TEMPS;
#endif
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
        const TC_FRAME& tc,
        const MODE_SAMPLE& mode_sample
    ) {
        switch (tc.cmd) {
            case ttc::CMD_PING:
            case ttc::CMD_GET_STATUS:
            case ttc::CMD_REQUEST_TELEMETRY:
            case ttc::CMD_SET_TIME:
            case ttc::CMD_RESET_OBC:
                break;
            case ttc::CMD_SET_MODE:
            case ttc::CMD_START_MISSION:
            case ttc::CMD_STOP_MISSION:
            case ttc::CMD_ENTER_SAFE:
            case ttc::CMD_EXIT_SAFE:
            case ttc::CMD_ADCS_START:
            case ttc::CMD_ADCS_STOP:
                return ExecuteModeTelecommand(tc, mode_sample);
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
        if (tc.cmd == ttc::CMD_RESET_OBC) {
#ifdef OBC_WATCHDOG_BENCH_CASE
            obc_watchdog_bench::BeforeReset(link);
#endif
            if (!obc_watchdog::esp32::RequestReset()) return ttc::ERR_STATE_REJECTED;
#ifdef OBC_WATCHDOG_BENCH_CASE
            obc_watchdog_bench::AfterReset();
#endif
            return ttc::ACK_OK;
        }
        return SendCurrentTelemetry() ? ttc::ACK_OK : ttc::ERR_STATE_REJECTED;
    }

} // namespace

    // ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

    bool Init(
        const MODE_SERVICES* modes
    ) {
        if (init_attempted) return active;
        init_attempted = true;
        if (modes && modes->max_snapshot_age_ms && modes->read_status && modes->start_adcs && modes->stop_adcs) {
            mode_services = *modes;
            mode_actions = {modes->start_adcs, modes->stop_adcs, RecordMode};
            operating_modes = obc_modes::Coordinator(true);
        }

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
        if (active) ServiceLogResponse();
        TC_FRAME tc = {};
        const bool received = active && link.ReceiveTelecommand(tc, pdMS_TO_TICKS(service_wait_ms));
        const MODE_SAMPLE mode_sample = ServiceModes();
        const bool pause_downlink = mode_services.read_protection
            && (mode_sample.subsystems.protection_active || operating_modes.IsProtectionInEffect());
        if (!active) {
            ServiceResourceHealth();
            delay(service_wait_ms);
            return;
        }
        if (received) {
            if (tc.origin == LINK_RF) rf_seen = true;
            if (tc.cmd == ttc::CMD_REQUEST_LOG) RequestLog(tc, pause_downlink);
            else Acknowledge(tc.seq, tc.cmd, ExecuteTelecommand(tc, mode_sample));
        }
        ServiceLogResponse();
        ServiceDelivery(pause_downlink);
        ServiceResourceHealth();
    }

    LINK_DIAGNOSTICS GetDiagnostics(
    ) { return link.GetDiagnostics(); }

} // namespace obc_ttc
