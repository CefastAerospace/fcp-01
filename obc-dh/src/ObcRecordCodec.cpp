#include "ObcRecordCodec.h" // Tipos e API do codec, sem dependência de hardware.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcRecordCodec.h".
 */

namespace obc_record {

    namespace {

        // ─── STRUCTS ──────────────────────────────────────────────────────

        // Apenas um ponteiro fica ativo; os limites são verificados antes
        // de percorrer o layout fixo, comum à escrita e à leitura.
        struct CURSOR {
            uint8_t* output;
            const uint8_t* input;
        };

        // ─── HELPERS ──────────────────────────────────────────────────────

        // Serializa larguras de 1/2/3/4/8 bytes; o maior deslocamento é 56.
        template<typename INTEGER>
        void TransferUnsigned(
            CURSOR& cursor,
            INTEGER& value,
            size_t width = sizeof(INTEGER)
        ) {
            uint64_t bits = cursor.output ? static_cast<uint64_t>(value) : 0;
            for (size_t index = 0; index < width; ++index) {
                if (cursor.output) {
                    *cursor.output++ = static_cast<uint8_t>(bits >> (index * 8));
                } else {
                    bits |= static_cast<uint64_t>(*cursor.input++) << (index * 8);
                }
            }
            if (cursor.input) value = static_cast<INTEGER>(bits);
        }

        // Evita depender de conversão unsigned->signed fora da faixa.
        void TransferSigned16(
            CURSOR& cursor,
            int16_t& value
        ) {
            uint16_t bits = static_cast<uint16_t>(value);
            TransferUnsigned(cursor, bits);
            if (cursor.input) {
                const int32_t decoded = bits <= 32767 ? bits : static_cast<int32_t>(bits) - 65536;
                value = static_cast<int16_t>(decoded);
            }
        }

        // memcpy preserva os bits IEEE sem aliasing ou acesso desalinhado.
        template<typename REAL, typename BITS>
        void TransferReal(
            CURSOR& cursor,
            REAL& value
        ) {
            BITS bits = 0;
            if (cursor.output) std::memcpy(&bits, &value, sizeof(bits));
            TransferUnsigned(cursor, bits);
            if (cursor.input) std::memcpy(&value, &bits, sizeof(bits));
        }

        // Enumerações do contrato têm exatamente um byte persistido.
        template<typename ENUM>
        void TransferEnum(
            CURSOR& cursor,
            ENUM& value
        ) {
            uint8_t bits = static_cast<uint8_t>(value);
            TransferUnsigned(cursor, bits);
            if (cursor.input) value = static_cast<ENUM>(bits);
        }

        // O arquivo não inclui NUL; a RAM reserva uma posição adicional.
        // Na escrita, não copia bytes residuais após o fim da descrição.
        void TransferText(
            CURSOR& cursor,
            char* value,
            size_t width
        ) {
            if (cursor.output) {
                size_t length = 0;
                while (length < width && value[length] != '\0') ++length;
                std::memset(cursor.output, 0, width);
                std::memcpy(cursor.output, value, length);
                cursor.output += width;
            } else {
                std::memcpy(value, cursor.input, width);
                value[width] = '\0';
                cursor.input += width;
            }
        }

        // Mantém as causas públicas da validação sem modificar os validadores.
        STATUS MapStatus(
            obc_data::STATUS status
        ) {
            switch (status) {
                case obc_data::STATUS::OK: return STATUS::OK;
                case obc_data::STATUS::UNSUPPORTED_VERSION: return STATUS::UNSUPPORTED_VERSION;
                case obc_data::STATUS::UNSUPPORTED_TYPE: return STATUS::UNSUPPORTED_TYPE;
                case obc_data::STATUS::INVALID_LENGTH: return STATUS::INVALID_LENGTH;
                case obc_data::STATUS::INVALID_VALUE: return STATUS::INVALID_DATA;
            }
            return STATUS::INVALID_DATA;
        }

        // Só lê o membro ativo, identificado pelo tipo já validado.
        STATUS ValidateRecord(
            const RECORD& record
        ) {
            using namespace obc_data;
            const obc_data::STATUS header_status = ValidateHeader(record.header);
            if (header_status != obc_data::STATUS::OK) return MapStatus(header_status);
            switch (record.header.type) {
                case RECORD_TYPE::MISSION: return MapStatus(Validate(record.header, record.body.mission));
                case RECORD_TYPE::TELEMETRY: return MapStatus(Validate(record.header, record.body.telemetry));
                case RECORD_TYPE::EVENT:
                case RECORD_TYPE::FAULT: return MapStatus(Validate(record.header, record.body.event));
                case RECORD_TYPE::STATISTICS: return MapStatus(Validate(record.header, record.body.statistics));
                case RECORD_TYPE::BOOT: return MapStatus(Validate(record.header, record.body.boot));
                case RECORD_TYPE::TIME_REFERENCE: return MapStatus(Validate(record.header, record.body.time_reference));
            }
            return STATUS::UNSUPPORTED_TYPE;
        }

        // A disponibilidade determina o significado, mesmo com lixo nos
        // bytes de campos ausentes. Nunca promove esse lixo a dado válido.
        void Normalize(
            RECORD& record
        ) {
            using namespace obc_data;
            if (record.header.type == RECORD_TYPE::MISSION) {
                PAYLOAD_DATA& data = record.body.mission;
                double* values[] = {&data.latitude, &data.longitude, &data.altitude, &data.velocity, &data.heading};
                for (size_t index = 0; index < 5; ++index) {
                    if ((data.available_fields & (1U << index)) == 0) *values[index] = 0;
                }
                if ((data.available_fields & PAYLOAD_TIMESTAMP) == 0) {
                    std::memset(data.timestamp, 0, sizeof(data.timestamp));
                }
            } else if (record.header.type == RECORD_TYPE::TELEMETRY) {
                TELEMETRY_DATA& data = record.body.telemetry;
                if ((data.available_fields & TM_TIMESTAMP) == 0) data.timestamp = 0;
                float* values[] = {&data.vbat, &data.ibat, &data.vsolar, &data.isolar,
                    &data.temps[0], &data.temps[1], &data.temps[2], &data.temps[3], &data.temps[4],
                    &data.rate_dps, &data.azimuth_deg};
                for (size_t index = 0; index < 11; ++index) {
                    if ((data.available_fields & (1UL << (index + 1))) == 0) *values[index] = 0;
                }
                if ((data.available_fields & TM_RSSI) == 0) data.rssi = 0;
                if ((data.available_fields & TM_SNR) == 0) data.snr = 0;
            }
        }

        // A assinatura é tratada externamente; os demais campos somam 20 bytes.
        void TransferHeader(
            CURSOR& cursor,
            obc_data::RECORD_HEADER& header
        ) {
            TransferUnsigned(cursor, header.version);
            TransferEnum(cursor, header.type);
            TransferUnsigned(cursor, header.total_bytes);
            TransferUnsigned(cursor, header.stamp.boot_id);
            TransferUnsigned(cursor, header.stamp.monotonic_ms);
        }

        // Compartilha a ordem dos campos entre os dois sentidos. Na leitura,
        // ativa e inicializa o membro antes de acessar qualquer campo dele.
        void TransferBody(
            CURSOR& cursor,
            RECORD& record
        ) {
            using namespace obc_data;
            switch (record.header.type) {
                case RECORD_TYPE::MISSION: {
                    if (cursor.input) record.body.mission = {};
                    PAYLOAD_DATA& data = record.body.mission;
                    TransferUnsigned(cursor, data.session_id);
                    TransferUnsigned(cursor, data.seq_id);
                    TransferUnsigned(cursor, data.icao, 3);
                    TransferUnsigned(cursor, data.msg_type);
                    TransferUnsigned(cursor, data.available_fields);
                    TransferReal<double, uint64_t>(cursor, data.latitude);
                    TransferReal<double, uint64_t>(cursor, data.longitude);
                    TransferReal<double, uint64_t>(cursor, data.altitude);
                    TransferReal<double, uint64_t>(cursor, data.velocity);
                    TransferReal<double, uint64_t>(cursor, data.heading);
                    TransferText(cursor, data.timestamp, timestamp_chars);
                    break;
                }
                case RECORD_TYPE::TELEMETRY: {
                    if (cursor.input) record.body.telemetry = {};
                    TELEMETRY_DATA& data = record.body.telemetry;
                    TransferUnsigned(cursor, data.available_fields);
                    TransferUnsigned(cursor, data.timestamp);
                    TransferEnum(cursor, data.mode);
                    float* values[] = {&data.vbat, &data.ibat, &data.vsolar, &data.isolar,
                        &data.temps[0], &data.temps[1], &data.temps[2], &data.temps[3], &data.temps[4],
                        &data.rate_dps, &data.azimuth_deg};
                    for (size_t index = 0; index < 11; ++index) TransferReal<float, uint32_t>(cursor, *values[index]);
                    TransferUnsigned(cursor, data.aircraft_count);
                    TransferUnsigned(cursor, data.message_count);
                    TransferSigned16(cursor, data.rssi);
                    TransferReal<float, uint32_t>(cursor, data.snr);
                    TransferUnsigned(cursor, data.rx_count);
                    TransferUnsigned(cursor, data.error_count);
                    TransferUnsigned(cursor, data.radio_resets);
                    break;
                }
                case RECORD_TYPE::EVENT:
                case RECORD_TYPE::FAULT: {
                    if (cursor.input) record.body.event = {};
                    EVENT_DATA& data = record.body.event;
                    TransferEnum(cursor, data.source);
                    TransferUnsigned(cursor, data.code);
                    TransferEnum(cursor, data.severity);
                    TransferText(cursor, data.description, description_chars);
                    break;
                }
                case RECORD_TYPE::STATISTICS: {
                    if (cursor.input) record.body.statistics = {};
                    STATISTICS_DATA& data = record.body.statistics;
                    uint64_t* values[] = {&data.received, &data.invalid, &data.gaps, &data.duplicates,
                        &data.records_written, &data.storage_failures, &data.records_dropped};
                    for (size_t index = 0; index < 7; ++index) TransferUnsigned(cursor, *values[index]);
                    break;
                }
                case RECORD_TYPE::BOOT:
                    if (cursor.input) record.body.boot = {};
                    TransferUnsigned(cursor, record.body.boot.restart_count);
                    TransferUnsigned(cursor, record.body.boot.reset_reason);
                    break;
                case RECORD_TYPE::TIME_REFERENCE:
                    if (cursor.input) record.body.time_reference = {};
                    record.body.time_reference.reception = record.header.stamp;
                    TransferUnsigned(cursor, record.body.time_reference.unix_s);
                    TransferUnsigned(cursor, record.body.time_reference.delay_ms);
                    break;
            }
        }

    } // namespace

    // ─── HELPERS ──────────────────────────────────────────────────────

    uint32_t ComputeCrc32(
        const uint8_t* data,
        size_t size
    ) {
        uint32_t crc = obc_data::crc32_initial;
        for (size_t index = 0; index < size; ++index) {
            crc ^= data[index];
            for (unsigned bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ ((crc & 1U) ? obc_data::crc32_polynomial : 0);
            }
        }
        return crc ^ obc_data::crc32_final_xor;
    }

    STATUS Encode(
        const RECORD& record,
        uint8_t* buffer,
        size_t capacity,
        size_t& written
    ) {
        written = 0;
        if (buffer == nullptr) return STATUS::INVALID_ARGUMENT;
        const STATUS status = ValidateRecord(record);
        if (status != STATUS::OK) return status;
        if (capacity < record.header.total_bytes) return STATUS::BUFFER_TOO_SMALL;

        // Trabalha numa cópia para zerar ausência sem modificar o chamador.
        RECORD copy = record;
        Normalize(copy);
        std::memcpy(buffer, obc_data::record_magic, sizeof(obc_data::record_magic));
        CURSOR cursor = {buffer + 4, nullptr};
        TransferHeader(cursor, copy.header);
        TransferBody(cursor, copy);
        uint32_t crc = ComputeCrc32(buffer, record.header.total_bytes - obc_data::crc_bytes);
        TransferUnsigned(cursor, crc);
        written = record.header.total_bytes;
        return STATUS::OK;
    }

    STATUS Decode(
        const uint8_t* buffer,
        size_t available,
        RECORD& record,
        size_t& consumed
    ) {
        consumed = 0;
        if (buffer == nullptr) return STATUS::INVALID_ARGUMENT;
        if (available < obc_data::header_bytes) return STATUS::INCOMPLETE;
        if (std::memcmp(buffer, obc_data::record_magic, sizeof(obc_data::record_magic)) != 0) {
            return STATUS::BAD_MAGIC;
        }

        // Não usa comprimentos desconhecidos para avançar ou alocar memória.
        RECORD decoded = {};
        CURSOR cursor = {nullptr, buffer + 4};
        TransferHeader(cursor, decoded.header);
        const STATUS header_status = MapStatus(obc_data::ValidateHeader(decoded.header));
        if (header_status != STATUS::OK) return header_status;
        const size_t size = decoded.header.total_bytes;
        if (available < size) return STATUS::INCOMPLETE;
        CURSOR crc_cursor = {nullptr, buffer + size - obc_data::crc_bytes};
        uint32_t stored_crc = 0;
        TransferUnsigned(crc_cursor, stored_crc);
        if (stored_crc != ComputeCrc32(buffer, size - obc_data::crc_bytes)) {
            return STATUS::CRC_MISMATCH;
        }

        TransferBody(cursor, decoded);
        Normalize(decoded);
        const STATUS status = ValidateRecord(decoded);
        if (status != STATUS::OK) return status;
        record = decoded;
        consumed = size;
        return STATUS::OK;
    }

} // namespace obc_record
