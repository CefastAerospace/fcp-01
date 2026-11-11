#include "ReadRecords.h" // Leitura nativa, codec e exportação dos modelos.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ReadRecords.h".
 */

namespace {

    using obc_record::RECORD;
    using obc_record_tool::REFERENCE;
    using obc_record_tool::SCAN_SUMMARY;

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Nome estável da causa; corrupção nunca vira um registro exportado.
    const char* StatusName(
        obc_record::STATUS status
    ) {
        switch (status) {
            case obc_record::STATUS::INCOMPLETE: return "registro incompleto";
            case obc_record::STATUS::BAD_MAGIC: return "assinatura inválida";
            case obc_record::STATUS::UNSUPPORTED_VERSION: return "versão não suportada";
            case obc_record::STATUS::UNSUPPORTED_TYPE: return "tipo não suportado";
            case obc_record::STATUS::INVALID_LENGTH: return "comprimento inválido";
            case obc_record::STATUS::CRC_MISMATCH: return "CRC incorreto";
            default: return "conteúdo inválido";
        }
    }

    // Agrupa bytes sem validade entre dois registros; não estima quantas
    // mensagens estavam ali nem promete recuperar seu conteúdo.
    void DiscardRange(
        uint64_t begin,
        uint64_t end,
        obc_record::STATUS reason,
        bool incomplete,
        SCAN_SUMMARY& summary,
        bool report
    ) {
        ++summary.discarded_ranges;
        summary.discarded_bytes += end - begin;
        if (report) {
            std::cerr << "Descartados bytes [" << begin << ',' << end << "): " << StatusName(reason);
            if (incomplete && reason != obc_record::STATUS::INCOMPLETE) {
                std::cerr << "; registro final incompleto";
            }
            std::cerr << ".\n";
        }
    }

    // Busca por assinatura em blocos, conservando três bytes na fronteira.
    // Um candidato só avança seu comprimento depois de Decode aceitar tudo.
    // A janela comporta o maior snapshot; uma cauda incompleta não impede achar outro registro
    // íntegro que tenha começado dentro do trecho danificado.
    bool ReadNext(
        std::ifstream& input,
        uint64_t file_size,
        uint64_t& offset,
        RECORD& record,
        obc_data::PAYLOAD_SNAPSHOT& snapshot,
        bool& is_snapshot,
        obc_delivery::RECORD& delivery,
        bool& is_delivery,
        uint64_t& record_offset,
        SCAN_SUMMARY& summary,
        bool report
    ) {
        uint8_t buffer[obc_record::max_snapshot_record_bytes];
        const uint64_t begin = offset;
        bool discarded = false;
        bool incomplete = false;
        obc_record::STATUS reason = obc_record::STATUS::BAD_MAGIC;
        while (offset < file_size) {
            const size_t count = static_cast<size_t>(std::min<uint64_t>(sizeof(buffer), file_size - offset));
            input.seekg(static_cast<std::streamoff>(offset));
            input.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(count));
            if (!input || input.gcount() != static_cast<std::streamsize>(count)) {
                summary.io_error = true;
                return false;
            }

            const uint8_t* legacy = std::search(buffer, buffer + count,
                obc_data::record_magic, obc_data::record_magic + 4);
            const uint8_t* current = std::search(buffer, buffer + count,
                obc_record::snapshot_magic, obc_record::snapshot_magic + 4);
            const uint8_t* telemetry = std::search(buffer, buffer + count,
                obc_record::telemetry_magic, obc_record::telemetry_magic + 4);
            const char delivery_magic[] = "FCD1";
            const uint8_t* delivery_begin = std::search(buffer, buffer + count, delivery_magic, delivery_magic + 4);
            const uint8_t* found = std::min(std::min(legacy, current), std::min(telemetry, delivery_begin));
            if (found == buffer + count) {
                discarded = true;
                if (count < 4 && (std::memcmp(buffer, obc_data::record_magic, count) == 0
                        || std::memcmp(buffer, obc_record::snapshot_magic, count) == 0
                        || std::memcmp(buffer, obc_record::telemetry_magic, count) == 0
                        || std::memcmp(buffer, delivery_magic, count) == 0)) {
                    incomplete = true;
                    reason = obc_record::STATUS::INCOMPLETE;
                }
                offset += count > 3 ? count - 3 : count;
                continue;
            }
            if (found != buffer) {
                discarded = true;
                offset += static_cast<uint64_t>(found - buffer);
                continue;
            }

            size_t consumed = 0;
            const bool candidate_snapshot = buffer[3] == '2';
            const bool candidate_delivery = buffer[2] == 'D';
            obc_record::STATUS status = obc_record::STATUS::INVALID_DATA;
            if (candidate_delivery) {
                const size_t wanted = count >= 6 && buffer[5] == 2
                    ? obc_delivery::confirm_bytes : obc_delivery::data_bytes;
                if (count < wanted) status = obc_record::STATUS::INCOMPLETE;
                else if (obc_delivery::Decode(buffer, wanted, delivery)) {
                    consumed = wanted;
                    status = obc_record::STATUS::OK;
                }
            } else status = candidate_snapshot
                ? obc_record::DecodeSnapshot(buffer, count, snapshot, consumed)
                : buffer[3] == '3'
                    ? obc_record::DecodeTelemetryV3(buffer, count, record, consumed)
                    : obc_record::Decode(buffer, count, record, consumed);
            if (status == obc_record::STATUS::OK) {
                if (discarded) DiscardRange(begin, offset, reason, incomplete, summary, report);
                record_offset = offset;
                is_snapshot = candidate_snapshot;
                is_delivery = candidate_delivery;
                offset += consumed;
                ++summary.records;
                return true;
            }
            discarded = true;
            reason = status;
            incomplete = incomplete || status == obc_record::STATUS::INCOMPLETE;
            ++offset;
        }
        if (discarded) DiscardRange(begin, offset, reason, incomplete, summary, report);
        return false;
    }

    // Ordena por boot/instante, conservando a ordem do arquivo nos empates.
    bool ReferenceLess(
        const REFERENCE& left,
        const REFERENCE& right
    ) {
        const obc_time::EVENT_STAMP& a = left.value.reception;
        const obc_time::EVENT_STAMP& b = right.value.reception;
        return a.boot_id < b.boot_id || (a.boot_id == b.boot_id && a.monotonic_ms < b.monotonic_ms);
    }

    // Comparador de busca: nunca mistura referências de boots distintos.
    bool StampLess(
        const obc_time::EVENT_STAMP& stamp,
        const REFERENCE& reference
    ) {
        const obc_time::EVENT_STAMP& other = reference.value.reception;
        return stamp.boot_id < other.boot_id
            || (stamp.boot_id == other.boot_id && stamp.monotonic_ms < other.monotonic_ms);
    }

    // Última referência <= evento; antes da primeira, aplica a primeira
    // retroativamente. Nos dois casos, o último registro vence um empate.
    const REFERENCE* SelectReference(
        const std::vector<REFERENCE>& references,
        const obc_time::EVENT_STAMP& stamp
    ) {
        auto next = std::upper_bound(references.begin(), references.end(), stamp, StampLess);
        if (next != references.begin() && (next - 1)->value.reception.boot_id == stamp.boot_id) {
            return &*(next - 1);
        }
        if (next == references.end() || next->value.reception.boot_id != stamp.boot_id) return nullptr;
        const obc_time::EVENT_STAMP first = next->value.reception;
        return &*(std::upper_bound(next, references.end(), first, StampLess) - 1);
    }

    // Aspas, barras e controles são escapados; demais bytes são UTF-8
    // previamente verificado ou ASCII garantido pelo modelo.
    void WriteString(
        const char* text
    ) {
        const char digits[] = "0123456789abcdef";
        std::cout << '"';
        for (size_t index = 0; text[index] != '\0'; ++index) {
            const unsigned char value = static_cast<unsigned char>(text[index]);
            if (value == '"' || value == '\\') std::cout << '\\' << static_cast<char>(value);
            else if (value < 0x20) std::cout << "\\u00" << digits[value >> 4] << digits[value & 15];
            else std::cout << static_cast<char>(value);
        }
        std::cout << '"';
    }

    // Verificação limitada a 64 bytes; rejeita overlong, surrogates e
    // codepoints acima de U+10FFFF, sem mudar bytes da descrição original.
    bool IsUtf8(
        const char* text
    ) {
        const size_t length = std::strlen(text);
        for (size_t index = 0; index < length;) {
            const unsigned char lead = static_cast<unsigned char>(text[index]);
            if (lead < 0x80) { ++index; continue; }
            const size_t width = lead >= 0xC2 && lead <= 0xDF ? 2
                : lead >= 0xE0 && lead <= 0xEF ? 3
                : lead >= 0xF0 && lead <= 0xF4 ? 4 : 0;
            if (width == 0 || width > length - index) return false;
            const unsigned char second = static_cast<unsigned char>(text[index + 1]);
            if ((lead == 0xE0 && second < 0xA0) || (lead == 0xED && second >= 0xA0)
                    || (lead == 0xF0 && second < 0x90) || (lead == 0xF4 && second >= 0x90)) return false;
            for (size_t next = 1; next < width; ++next) {
                const unsigned char continuation = static_cast<unsigned char>(text[index + next]);
                if (continuation < 0x80 || continuation > 0xBF) return false;
            }
            index += width;
        }
        return true;
    }

    // Campos textuais usam nomes ASCII internos; seus valores podem conter
    // bytes não UTF-8. Mantém a política de descrição também para os snapshots.
    void WriteTextField(
        const char* name,
        const char* text,
        bool available
    ) {
        WriteString(name);
        std::cout << ':';
        if (!available) std::cout << "null";
        else if (IsUtf8(text)) WriteString(text);
        else {
            const char digits[] = "0123456789abcdef";
            std::cout << "null,\"" << name << "_encoding\":\"invalid_utf8\",\"" << name << "_hex\":\"";
            for (size_t index = 0; text[index] != '\0'; ++index) {
                const unsigned char value = static_cast<unsigned char>(text[index]);
                std::cout << digits[value >> 4] << digits[value & 15];
            }
            std::cout << '"';
        }
    }

    // A mesma representação de seis hexadecimais serve às duas versões.
    void WriteIcao(
        uint32_t value
    ) {
        char icao[7] = {};
        const char digits[] = "0123456789ABCDEF";
        for (unsigned index = 0; index < 6; ++index) icao[index] = digits[(value >> (4 * (5 - index))) & 15];
        WriteString(icao);
    }

    // JSON não tem NaN/Inf; o decoder já rejeita esses valores disponíveis.
    // max_digits10 permite representar o real original, inclusive -0.0.
    template<typename REAL>
    void WriteReal(
        REAL value,
        bool available
    ) {
        if (!available) std::cout << "null";
        else if (value == 0 && std::signbit(value)) std::cout << "-0.0";
        else std::cout << std::setprecision(std::numeric_limits<REAL>::max_digits10) << value;
    }

    // JSON legível usa os nomes do contrato; máscara permanece implícita.
    void WriteMission(
        const obc_data::PAYLOAD_DATA& data
    ) {
        std::cout << "{\"session_id\":" << data.session_id << ",\"seq_id\":" << data.seq_id << ",\"icao\":";
        WriteIcao(data.icao);
        std::cout << ",\"msg_type\":" << static_cast<unsigned>(data.msg_type);
        const char* names[] = {"lat", "lon", "alt", "vel", "heading"};
        const double values[] = {data.latitude, data.longitude, data.altitude, data.velocity, data.heading};
        for (size_t index = 0; index < 5; ++index) {
            std::cout << ','; WriteString(names[index]); std::cout << ':';
            WriteReal(values[index], (data.available_fields & (1U << index)) != 0);
        }
        std::cout << ",\"timestamp\":";
        if (data.available_fields & obc_data::PAYLOAD_TIMESTAMP) WriteString(data.timestamp);
        else std::cout << "null";
        std::cout << '}';
    }

    // Não aplica arredondamentos do rádio ao dado armazenado no SD.
    void WriteTelemetry(
        const obc_data::TELEMETRY_DATA& data,
        bool version_three = false
    ) {
        const char* modes[] = {"bench", "safe", "detumb", "sun", "mission", "lowpwr"};
        std::cout << "{\"ts\":";
        if (data.available_fields & obc_data::TM_TIMESTAMP) std::cout << data.timestamp;
        else std::cout << "null";
        std::cout << ",\"md\":"; WriteString(modes[static_cast<uint8_t>(data.mode)]);
        const char* names[] = {"vb", "ib", "vs", "is"};
        const float values[] = {data.vbat, data.ibat, data.vsolar, data.isolar};
        for (size_t index = 0; index < 4; ++index) {
            std::cout << ','; WriteString(names[index]); std::cout << ':';
            WriteReal(values[index], (data.available_fields & (1UL << (index + 1))) != 0);
        }
        std::cout << ",\"tp\":[";
        for (size_t index = 0; index < obc_data::thermal_sensors; ++index) {
            if (index != 0) std::cout << ',';
            WriteReal(data.temps[index], (data.available_fields & (1UL << (index + 5))) != 0);
        }
        std::cout << "],\"w\":"; WriteReal(data.rate_dps, (data.available_fields & obc_data::TM_RATE) != 0);
        std::cout << ",\"az\":"; WriteReal(data.azimuth_deg, (data.available_fields & obc_data::TM_AZIMUTH) != 0);
        std::cout << ",\"ac\":";
        if (!version_three || (data.available_fields & obc_data::TM_AIRCRAFT)) std::cout << data.aircraft_count;
        else std::cout << "null";
        std::cout << ",\"msg\":";
        if (!version_three || (data.available_fields & obc_data::TM_MESSAGES)) std::cout << data.message_count;
        else std::cout << "null";
        std::cout << ",\"rssi\":";
        if (data.available_fields & obc_data::TM_RSSI) std::cout << data.rssi;
        else std::cout << "null";
        std::cout << ",\"snr\":"; WriteReal(data.snr, (data.available_fields & obc_data::TM_SNR) != 0);
        std::cout << ",\"rx\":";
        if (!version_three || (data.available_fields & obc_data::TM_RX_COUNT)) std::cout << data.rx_count;
        else std::cout << "null";
        std::cout << ",\"err\":";
        if (!version_three || (data.available_fields & obc_data::TM_ERROR_COUNT)) std::cout << data.error_count;
        else std::cout << "null";
        std::cout << ",\"rst\":";
        if (!version_three || (data.available_fields & obc_data::TM_RADIO_RESETS)) std::cout << data.radio_resets;
        else std::cout << "null";
        std::cout << '}';
    }

    // Texto inválido em UTF-8 não invalida o registro binário: exporta
    // seus bytes em hex, sem produzir JSON inválido ou substituir bytes.
    void WriteEvent(
        const obc_data::EVENT_DATA& data
    ) {
        std::cout << "{\"source\":" << static_cast<unsigned>(data.source) << ",\"code\":" << data.code
            << ",\"severity\":" << static_cast<unsigned>(data.severity) << ',';
        WriteTextField("description", data.description, true);
        std::cout << '}';
    }

    // Exporta a lista inteira, inclusive vazia, sem inferir atualizações ADS-B.
    void WriteSnapshotBody(
        const obc_data::PAYLOAD_SNAPSHOT& snapshot
    ) {
        std::cout << "{\"id\":" << snapshot.id << ",\"aircraft\":[";
        for (size_t index = 0; index < snapshot.aircraft_count; ++index) {
            if (index != 0) std::cout << ',';
            const obc_data::AIRCRAFT_STATE& data = snapshot.aircraft[index];
            std::cout << "{\"icao\":"; WriteIcao(data.icao);
            std::cout << ',';
            WriteTextField("identification", data.identification,
                (data.available_fields & obc_data::SNAPSHOT_IDENTIFICATION) != 0);
            const char* names[] = {"lat", "lon", "alt"};
            const double values[] = {data.latitude, data.longitude, data.altitude};
            for (unsigned bit = 0; bit < 3; ++bit) {
                std::cout << ','; WriteString(names[bit]); std::cout << ':';
                WriteReal(values[bit], (data.available_fields & (1U << bit)) != 0);
            }
            std::cout << ',';
            WriteTextField("tick", data.tick, (data.available_fields & obc_data::SNAPSHOT_TICK) != 0);
            std::cout << ",\"extras\":[";
            for (size_t extra = 0; extra < data.extra_count; ++extra) {
                if (extra != 0) std::cout << ',';
                std::cout << '{'; WriteTextField("key", data.extras[extra].key, true);
                std::cout << ','; WriteTextField("value", data.extras[extra].value, true);
                std::cout << '}';
            }
            std::cout << "]}";
        }
        std::cout << "]}";
    }

    // Serialização JSON somente no programa nativo; firmware usa o codec.
    void WriteBody(
        const RECORD& record
    ) {
        using obc_data::RECORD_TYPE;
        switch (record.header.type) {
            case RECORD_TYPE::MISSION: WriteMission(record.body.mission); break;
            case RECORD_TYPE::TELEMETRY: WriteTelemetry(record.body.telemetry); break;
            case RECORD_TYPE::EVENT:
            case RECORD_TYPE::FAULT: WriteEvent(record.body.event); break;
            case RECORD_TYPE::STATISTICS: {
                const obc_data::STATISTICS_DATA& data = record.body.statistics;
                const char* names[] = {"received", "invalid", "gaps", "duplicates", "records_written", "storage_failures", "records_dropped"};
                const uint64_t values[] = {data.received, data.invalid, data.gaps, data.duplicates,
                    data.records_written, data.storage_failures, data.records_dropped};
                std::cout << '{';
                for (size_t index = 0; index < 7; ++index) {
                    if (index != 0) std::cout << ',';
                    WriteString(names[index]); std::cout << ':' << values[index];
                }
                std::cout << '}';
                break;
            }
            case RECORD_TYPE::BOOT:
                std::cout << "{\"restart_count\":" << record.body.boot.restart_count
                    << ",\"reset_reason\":" << record.body.boot.reset_reason << '}';
                break;
            case RECORD_TYPE::TIME_REFERENCE:
                std::cout << "{\"unix_s\":" << record.body.time_reference.unix_s
                    << ",\"delay_ms\":" << record.body.time_reference.delay_ms << '}';
                break;
        }
    }

    // Acrescenta horário estimado e proveniência, sem sobrescrever o horário
    // original do Payload ou o ts armazenado na TM.
    void WriteEnvelope(
        uint8_t version,
        const char* type,
        const obc_time::EVENT_STAMP& stamp,
        uint64_t offset,
        const std::vector<REFERENCE>& references
    ) {
        const REFERENCE* reference = SelectReference(references, stamp);
        int64_t unix_ms = 0;
        const obc_time::STATUS status = obc_time::Reconstruct(stamp,
            reference ? &reference->value : nullptr, unix_ms);
        std::cout << "{\"file_offset\":" << offset << ",\"version\":" << static_cast<unsigned>(version)
            << ",\"type\":";
        WriteString(type);
        std::cout << ",\"boot_id\":" << stamp.boot_id
            << ",\"monotonic_ms\":" << stamp.monotonic_ms << ",\"unix_ms\":";
        if (status == obc_time::STATUS::OK) std::cout << unix_ms;
        else std::cout << "null";
        std::cout << ",\"time_status\":";
        WriteString(status == obc_time::STATUS::OK ? "estimated"
            : status == obc_time::STATUS::NO_REFERENCE ? "no_reference" : "out_of_range");
        std::cout << ",\"reference_offset\":";
        if (reference) std::cout << reference->file_offset;
        else std::cout << "null";
        std::cout << ",\"data\":";
    }

    // Conserva o envelope e o corpo v1, sem ampliar sua união na RAM.
    void WriteRecord(
        const RECORD& record,
        uint64_t offset,
        const std::vector<REFERENCE>& references
    ) {
        const char* types[] = {"mission", "telemetry", "event", "fault", "statistics", "boot", "time_reference"};
        if (record.header.version == obc_record::telemetry_version) {
            WriteEnvelope(record.header.version, "telemetry", record.header.stamp, offset, references);
            WriteTelemetry(record.body.telemetry, true);
            std::cout << "}\n";
            return;
        }
        WriteEnvelope(record.header.version, types[static_cast<uint8_t>(record.header.type) - 1],
            record.header.stamp, offset, references);
        WriteBody(record);
        std::cout << "}\n";
    }

    // Duas passagens no mesmo arquivo seekable. Memória extra cresce apenas
    // com as referências; busca temporal O(log K) para K referências.
    // Exporta o diário aditivo, mantendo indisponibilidade e identificação u64.
    void WriteDelivery(
        const obc_delivery::RECORD& record,
        uint64_t offset
    ) {
        const auto& delivery = record.delivery;
        const auto& aircraft = delivery.aircraft;
        std::cout << "{\"version\":1,\"type\":\"" << (record.confirmation ? "delivery_confirm" : "delivery_data")
            << "\",\"file_offset\":" << offset << ",\"boot\":\"" << delivery.id.boot
            << "\",\"sequence\":\"" << delivery.id.sequence << '"';
        if (!record.confirmation) {
            std::cout << ",\"monotonic_ms\":" << delivery.monotonic_ms << ",\"icao\":" << aircraft.icao << ",\"lat\":";
            if (aircraft.available_fields & obc_data::SNAPSHOT_LATITUDE) std::cout << std::setprecision(17) << aircraft.latitude;
            else std::cout << "null";
            std::cout << ",\"lon\":";
            if (aircraft.available_fields & obc_data::SNAPSHOT_LONGITUDE) std::cout << std::setprecision(17) << aircraft.longitude;
            else std::cout << "null";
            std::cout << ",\"alt\":";
            if (aircraft.available_fields & obc_data::SNAPSHOT_ALTITUDE) std::cout << std::setprecision(17) << aircraft.altitude;
            else std::cout << "null";
            std::cout << ",\"vel\":null,\"hdg\":null,\"ts\":null,\"count\":null";
        }
        std::cout << "}\n";
    }

    int ReadFile(
        const char* path
    ) {
        std::ifstream input(path, std::ios::binary);
        input.seekg(0, std::ios::end);
        const std::streamoff size = input.tellg();
        if (!input || size < 0) {
            std::cerr << "Não foi possível abrir/medir o arquivo de entrada.\n";
            return 2;
        }
        std::vector<REFERENCE> references;
        RECORD record = {};
        obc_data::PAYLOAD_SNAPSHOT snapshot = {};
        bool is_snapshot = false;
        bool is_delivery = false;
        obc_delivery::RECORD delivery = {};
        uint64_t offset = 0;
        uint64_t record_offset = 0;
        SCAN_SUMMARY first_pass = {};
        while (ReadNext(input, static_cast<uint64_t>(size), offset, record, snapshot,
                is_snapshot, delivery, is_delivery, record_offset, first_pass, false)) {
            if (!is_snapshot && !is_delivery && record.header.type == obc_data::RECORD_TYPE::TIME_REFERENCE) {
                references.push_back({record.body.time_reference, record_offset});
            }
        }
        if (first_pass.io_error) {
            std::cerr << "Falha de leitura na primeira passagem.\n";
            return 2;
        }
        std::stable_sort(references.begin(), references.end(), ReferenceLess);

        offset = 0;
        SCAN_SUMMARY summary = {};
        while (ReadNext(input, static_cast<uint64_t>(size), offset, record, snapshot,
                is_snapshot, delivery, is_delivery, record_offset, summary, true)) {
            if (is_delivery) WriteDelivery(delivery, record_offset);
            else if (is_snapshot) {
                WriteEnvelope(obc_record::snapshot_version, "payload_snapshot", snapshot.stamp, record_offset, references);
                WriteSnapshotBody(snapshot);
                std::cout << "}\n";
            } else {
                WriteRecord(record, record_offset, references);
            }
        }
        std::cout.flush();
        if (summary.io_error || !std::cout) {
            std::cerr << "Falha de leitura ou escrita na saída.\n";
            return 2;
        }
        std::cerr << "Registros válidos: " << summary.records << "; trechos descartados: "
            << summary.discarded_ranges << "; bytes descartados: " << summary.discarded_bytes << ".\n";
        return summary.discarded_ranges != 0 ? 1 : 0;
    }

} // namespace

int main(
    int argc,
    char** argv
) {
    std::cout.imbue(std::locale::classic());
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        std::cout << "Uso: read-records entrada.bin > registros.jsonl\n";
        return 0;
    }
    if (argc != 2) {
        std::cerr << "Uso: read-records entrada.bin > registros.jsonl\n";
        return 2;
    }
    try {
        return ReadFile(argv[1]);
    } catch (const std::exception& error) {
        std::cerr << "Falha na interpretação: " << error.what() << '\n';
        return 2;
    }
}
