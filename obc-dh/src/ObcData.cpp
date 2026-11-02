#include "models/ObcData.h" // Modelos, tamanhos e validadores do contrato proposto.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "models/ObcData.h".
 */

namespace obc_data {

    namespace {

        // ─── HELPERS ──────────────────────────────────────────────────────

        // Reutiliza a validação comum sem aceitar corpo de outro tipo.
        STATUS ValidateFor(
            const RECORD_HEADER& header,
            RECORD_TYPE type
        ) {
            const STATUS result = ValidateHeader(header);
            if (result != STATUS::OK) {
                return result;
            }
            return header.type == type ? STATUS::OK : STATUS::INVALID_VALUE;
        }

        // Lê apenas posições já verificadas como dígitos no timestamp fixo.
        unsigned ReadDecimal(
            const char* text,
            size_t count
        ) {
            unsigned value = 0;
            for (size_t index = 0; index < count; ++index) {
                value = value * 10 + static_cast<unsigned>(text[index] - '0');
            }
            return value;
        }

        // Valida o formato UTC do log de bancada sem time_t, fuso local ou
        // conversão para a referência de voo. Rejeita datas impossíveis.
        bool IsUtcTimestamp(
            const char* text
        ) {
            for (size_t index = 0; index < timestamp_chars; ++index) {
                if (index == 4 || index == 7) {
                    if (text[index] != '-') return false;
                } else if (index == 10) {
                    if (text[index] != 'T') return false;
                } else if (index == 13 || index == 16) {
                    if (text[index] != ':') return false;
                } else if (index == 19) {
                    if (text[index] != 'Z') return false;
                } else if (text[index] < '0' || text[index] > '9') {
                    return false;
                }
            }
            if (text[timestamp_chars] != '\0') return false;

            const unsigned year = ReadDecimal(text, 4);
            const unsigned month = ReadDecimal(text + 5, 2);
            const unsigned day = ReadDecimal(text + 8, 2);
            if (year == 0 || month == 0 || month > 12 || day == 0) return false;
            const unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
            const unsigned max_day = days[month - 1] + (month == 2 && leap ? 1 : 0);
            return day <= max_day && ReadDecimal(text + 11, 2) < 24
                && ReadDecimal(text + 14, 2) < 60 && ReadDecimal(text + 17, 2) < 60;
        }

    } // namespace

    // ─── HELPERS ──────────────────────────────────────────────────────

    uint16_t GetRecordSize(
        RECORD_TYPE type
    ) {
        // Soma de larguras explícitas, não do layout/padding da RAM.
        switch (type) {
            case RECORD_TYPE::MISSION: return header_bytes + 77 + crc_bytes;
            case RECORD_TYPE::TELEMETRY: return header_bytes + 71 + crc_bytes;
            case RECORD_TYPE::EVENT:
            case RECORD_TYPE::FAULT: return header_bytes + 68 + crc_bytes;
            case RECORD_TYPE::STATISTICS: return header_bytes + 56 + crc_bytes;
            case RECORD_TYPE::BOOT: return header_bytes + 12 + crc_bytes;
            case RECORD_TYPE::TIME_REFERENCE: return header_bytes + 8 + crc_bytes;
        }
        return 0;
    }

    STATUS ValidateHeader(
        const RECORD_HEADER& header
    ) {
        if (header.version != contract_version) return STATUS::UNSUPPORTED_VERSION;
        const uint16_t expected_bytes = GetRecordSize(header.type);
        if (expected_bytes == 0) return STATUS::UNSUPPORTED_TYPE;
        if (header.total_bytes > max_record_bytes || header.total_bytes != expected_bytes) {
            return STATUS::INVALID_LENGTH;
        }
        return header.stamp.boot_id != 0 ? STATUS::OK : STATUS::INVALID_VALUE;
    }

    STATUS Validate(
        const RECORD_HEADER& header,
        const PAYLOAD_DATA& data
    ) {
        const STATUS result = ValidateFor(header, RECORD_TYPE::MISSION);
        if (result != STATUS::OK) return result;

        // Pelo menos um dado de voo foi atualizado; o horário isolado não
        // transforma um pacote vazio numa observação. Bits 6/7 são reservados.
        const uint8_t fields = data.available_fields;
        const uint8_t position = PAYLOAD_LATITUDE | PAYLOAD_LONGITUDE;
        if (data.session_id == 0 || data.icao > 0xFFFFFFUL || data.msg_type == 0
                || (fields & 0xC0U) != 0 || (fields & 0x1FU) == 0
                || ((fields & position) != 0 && (fields & position) != position)) {
            return STATUS::INVALID_VALUE;
        }

        // Não aplica limiares operacionais dos setores nem presume alt > 0.
        if ((fields & PAYLOAD_LATITUDE) && (!std::isfinite(data.latitude)
                || data.latitude < -90 || data.latitude > 90)) return STATUS::INVALID_VALUE;
        if ((fields & PAYLOAD_LONGITUDE) && (!std::isfinite(data.longitude)
                || data.longitude < -180 || data.longitude > 180)) return STATUS::INVALID_VALUE;
        if ((fields & PAYLOAD_ALTITUDE) && !std::isfinite(data.altitude)) return STATUS::INVALID_VALUE;
        if ((fields & PAYLOAD_VELOCITY) && (!std::isfinite(data.velocity)
                || data.velocity < 0)) return STATUS::INVALID_VALUE;
        if ((fields & PAYLOAD_HEADING) && (!std::isfinite(data.heading)
                || data.heading < 0 || data.heading >= 360)) return STATUS::INVALID_VALUE;
        if ((fields & PAYLOAD_TIMESTAMP) && !IsUtcTimestamp(data.timestamp)) return STATUS::INVALID_VALUE;
        return STATUS::OK;
    }

    STATUS Validate(
        const RECORD_HEADER& header,
        const TELEMETRY_DATA& data
    ) {
        const STATUS result = ValidateFor(header, RECORD_TYPE::TELEMETRY);
        if (result != STATUS::OK) return result;
        if (static_cast<uint8_t>(data.mode) > static_cast<uint8_t>(MODE::LOW_POWER)
                || (data.available_fields & ~0x3FFFUL) != 0) return STATUS::INVALID_VALUE;

        // Disponibilidade é independente por leitura; ausência não vira zero
        // válido. Sem limites elétricos/térmicos inventados nesta camada.
        const float values[] = {data.vbat, data.ibat, data.vsolar, data.isolar,
            data.temps[0], data.temps[1], data.temps[2], data.temps[3], data.temps[4],
            data.rate_dps, data.azimuth_deg};
        for (size_t index = 0; index < sizeof(values) / sizeof(values[0]); ++index) {
            if ((data.available_fields & (1UL << (index + 1)))
                    && !std::isfinite(values[index])) return STATUS::INVALID_VALUE;
        }
        if ((data.available_fields & TM_SNR) && !std::isfinite(data.snr)) return STATUS::INVALID_VALUE;
        return STATUS::OK;
    }

    STATUS Validate(
        const RECORD_HEADER& header,
        const EVENT_DATA& data
    ) {
        const STATUS result = ValidateHeader(header);
        if (result != STATUS::OK) return result;
        if (header.type != RECORD_TYPE::EVENT && header.type != RECORD_TYPE::FAULT) {
            return STATUS::INVALID_VALUE;
        }
        const uint8_t source = static_cast<uint8_t>(data.source);
        const uint8_t severity = static_cast<uint8_t>(data.severity);
        if (source < 1 || source > 8 || severity > 3
                || ((header.type == RECORD_TYPE::FAULT) != (severity >= 2))
                || std::memchr(data.description, '\0', sizeof(data.description)) == nullptr) {
            return STATUS::INVALID_VALUE;
        }
        return STATUS::OK;
    }

    STATUS Validate(
        const RECORD_HEADER& header,
        const STATISTICS_DATA& data
    ) {
        (void)data;
        return ValidateFor(header, RECORD_TYPE::STATISTICS);
    }

    STATUS Validate(
        const RECORD_HEADER& header,
        const BOOT_DATA& data
    ) {
        const STATUS result = ValidateFor(header, RECORD_TYPE::BOOT);
        if (result != STATUS::OK) return result;
        return data.restart_count == header.stamp.boot_id - 1
            ? STATUS::OK : STATUS::INVALID_VALUE;
    }

    STATUS Validate(
        const RECORD_HEADER& header,
        const obc_time::TIME_REFERENCE& data
    ) {
        const STATUS result = ValidateFor(header, RECORD_TYPE::TIME_REFERENCE);
        if (result != STATUS::OK) return result;
        return data.reception.boot_id == header.stamp.boot_id
                && data.reception.monotonic_ms == header.stamp.monotonic_ms
            ? STATUS::OK : STATUS::INVALID_VALUE;
    }

} // namespace obc_data
