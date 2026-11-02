/**
 * @file ObcDataCheck.cpp
 * @brief Verificação nativa dos modelos e limites do contrato OBC v1 proposto.
 *
 * Executar dentro de obc-dh/:
 * g++ -std=c++11 -Wall -Wextra -Werror -Iheaders src/ObcData.cpp tests/ObcDataCheck.cpp -o /tmp/fcp01-data-check
 * /tmp/fcp01-data-check
 */

#include "models/ObcData.h" // Tipos e validação sem hardware ou ArduinoJson.
#include <assert.h> // assert.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "models/ObcData.h".
 */

// ─── HELPERS ──────────────────────────────────────────────────────

// Cria metadados do mesmo boot/instante usado nos exemplos do contrato.
static obc_data::RECORD_HEADER Header(
    obc_data::RECORD_TYPE type
) {
    return {obc_data::contract_version, type, obc_data::GetRecordSize(type), {1, 5000}};
}

// Exercita os sete layouts canônicos e rejeita comprimentos não canônicos.
static void CheckHeaders(
) {
    using namespace obc_data;
    const uint16_t sizes[] = {105, 99, 96, 96, 84, 40, 36};
    assert(std::memcmp(record_magic, "FCP1", 4) == 0);
    assert(header_bytes == 24 && crc_bytes == 4 && max_record_bytes == 512);
    for (uint8_t index = 0; index < 7; ++index) {
        const RECORD_TYPE type = static_cast<RECORD_TYPE>(index + 1);
        const RECORD_HEADER header = Header(type);
        assert(GetRecordSize(type) == sizes[index]);
        assert(header.total_bytes <= max_record_bytes);
        assert(ValidateHeader(header) == STATUS::OK);
    }
    RECORD_HEADER header = Header(RECORD_TYPE::MISSION);
    header.version = 2;
    assert(ValidateHeader(header) == STATUS::UNSUPPORTED_VERSION);
    header.version = 1;
    header.type = static_cast<RECORD_TYPE>(0);
    assert(GetRecordSize(header.type) == 0);
    assert(ValidateHeader(header) == STATUS::UNSUPPORTED_TYPE);
    header.type = static_cast<RECORD_TYPE>(255);
    assert(ValidateHeader(header) == STATUS::UNSUPPORTED_TYPE);
    header.type = RECORD_TYPE::MISSION;
    const uint16_t bad_sizes[] = {0, 27, 104, 106, 512, 513, UINT16_MAX};
    for (size_t index = 0; index < sizeof(bad_sizes) / sizeof(bad_sizes[0]); ++index) {
        header.total_bytes = bad_sizes[index];
        assert(ValidateHeader(header) == STATUS::INVALID_LENGTH);
    }
    header.total_bytes = 105;
    header.stamp.boot_id = 0;
    assert(ValidateHeader(header) == STATUS::INVALID_VALUE);
    header.stamp = {UINT64_MAX, UINT64_MAX};
    assert(ValidateHeader(header) == STATUS::OK);
}

// Confere os exemplos de posição/velocidade e não confunde zero com ausência.
static void CheckPayload(
) {
    using namespace obc_data;
    const RECORD_HEADER header = Header(RECORD_TYPE::MISSION);
    PAYLOAD_DATA position = {};
    position.session_id = 42;
    position.seq_id = 12;
    position.icao = 0xE4953A;
    position.msg_type = 3;
    position.available_fields = PAYLOAD_LATITUDE | PAYLOAD_LONGITUDE
        | PAYLOAD_ALTITUDE | PAYLOAD_TIMESTAMP;
    position.latitude = -19.95438;
    position.longitude = -43.6116;
    position.altitude = 39000;
    std::strcpy(position.timestamp, "2026-09-08T00:21:44Z");
    assert(Validate(header, position) == STATUS::OK);

    PAYLOAD_DATA velocity = {};
    velocity.session_id = 42;
    velocity.seq_id = 13;
    velocity.icao = position.icao;
    velocity.msg_type = 4;
    velocity.available_fields = PAYLOAD_VELOCITY | PAYLOAD_HEADING;
    velocity.velocity = 489;
    velocity.heading = 37;
    assert(Validate(header, velocity) == STATUS::OK);
    assert((velocity.available_fields & (PAYLOAD_LATITUDE | PAYLOAD_LONGITUDE)) == 0);
    assert(position.latitude == -19.95438 && position.seq_id == 12);

    // Valores fora do domínio em posições ausentes não recebem validade.
    velocity.latitude = std::numeric_limits<double>::quiet_NaN();
    velocity.longitude = std::numeric_limits<double>::infinity();
    velocity.velocity = 0;
    velocity.heading = 0;
    assert(Validate(header, velocity) == STATUS::OK);
    velocity.available_fields = 0;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.available_fields = PAYLOAD_TIMESTAMP;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.available_fields = PAYLOAD_VELOCITY | 0x80;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.available_fields = PAYLOAD_LATITUDE;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.available_fields = PAYLOAD_LONGITUDE;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);

    position.latitude = -90;
    position.longitude = -180;
    position.altitude = -100;
    assert(Validate(header, position) == STATUS::OK);
    position.latitude = 90;
    position.longitude = 180;
    assert(Validate(header, position) == STATUS::OK);
    position.latitude = 90.01;
    assert(Validate(header, position) == STATUS::INVALID_VALUE);
    position.latitude = 0;
    position.longitude = -180.01;
    assert(Validate(header, position) == STATUS::INVALID_VALUE);
    position.longitude = 0;
    assert(Validate(header, position) == STATUS::OK);
    position.altitude = std::numeric_limits<double>::quiet_NaN();
    assert(Validate(header, position) == STATUS::INVALID_VALUE);

    velocity.available_fields = PAYLOAD_VELOCITY | PAYLOAD_HEADING;
    velocity.velocity = -1;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.velocity = std::numeric_limits<double>::infinity();
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.velocity = 0;
    velocity.heading = 360;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.heading = -1;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.heading = 359.999;
    velocity.session_id = UINT64_MAX;
    velocity.seq_id = UINT32_MAX;
    velocity.icao = 0xFFFFFF;
    velocity.msg_type = UINT8_MAX;
    assert(Validate(header, velocity) == STATUS::OK);
    velocity.icao = 0x1000000;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.icao = 0;
    velocity.seq_id = 0;
    assert(Validate(header, velocity) == STATUS::OK);
    velocity.session_id = 0;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.session_id = 1;
    velocity.msg_type = 0;
    assert(Validate(header, velocity) == STATUS::INVALID_VALUE);
    velocity.msg_type = 4;
    assert(Validate(Header(RECORD_TYPE::TELEMETRY), velocity) == STATUS::INVALID_VALUE);
}

// Mantém o horário de bancada textual separado da referência Unix de voo.
static void CheckPayloadTimestamp(
) {
    using namespace obc_data;
    const RECORD_HEADER header = Header(RECORD_TYPE::MISSION);
    PAYLOAD_DATA data = {};
    data.session_id = 1;
    data.msg_type = 4;
    data.available_fields = PAYLOAD_VELOCITY | PAYLOAD_TIMESTAMP;
    const char* valid[] = {"2024-02-29T00:00:00Z", "2000-02-29T23:59:59Z",
        "0001-01-01T00:00:00Z", "9999-12-31T23:59:59Z"};
    for (size_t index = 0; index < sizeof(valid) / sizeof(valid[0]); ++index) {
        std::strcpy(data.timestamp, valid[index]);
        assert(Validate(header, data) == STATUS::OK);
    }
    const char* invalid[] = {"2026-02-29T00:00:00Z", "2100-02-29T00:00:00Z",
        "2026-04-31T00:00:00Z", "0000-01-01T00:00:00Z", "2026-00-01T00:00:00Z",
        "2026-13-01T00:00:00Z", "2026-01-00T00:00:00Z", "2026-01-01T24:00:00Z",
        "2026-01-01T00:60:00Z", "2026-01-01T00:00:60Z", "2026/01/01T00:00:00Z",
        "2026-01-01 00:00:00Z", "2026-01-01T00:00:00x", "curto"};
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        std::strcpy(data.timestamp, invalid[index]);
        assert(Validate(header, data) == STATUS::INVALID_VALUE);
    }
    std::memset(data.timestamp, '1', sizeof(data.timestamp));
    assert(Validate(header, data) == STATUS::INVALID_VALUE);
}

// Confere a TM documentada, máscaras independentes e valores indisponíveis.
static void CheckTelemetry(
) {
    using namespace obc_data;
    const RECORD_HEADER header = Header(RECORD_TYPE::TELEMETRY);
    TELEMETRY_DATA data = {};
    data.available_fields = 0x3FFF;
    data.timestamp = 1760000123;
    data.mode = MODE::MISSION;
    data.vbat = 7.81f;
    data.ibat = 420;
    data.vsolar = 9.1f;
    data.isolar = 310;
    const float temperatures[] = {21.5f, 22, 23.1f, 20.9f, 24.3f};
    std::memcpy(data.temps, temperatures, sizeof(temperatures));
    data.rate_dps = 12.4f;
    data.azimuth_deg = 37;
    data.aircraft_count = 3;
    data.message_count = 1200;
    data.rssi = -81;
    data.snr = 9.5f;
    data.rx_count = 10;
    assert(Validate(header, data) == STATUS::OK);
    data.temps[4] = std::numeric_limits<float>::quiet_NaN();
    assert(Validate(header, data) == STATUS::INVALID_VALUE);
    data.available_fields &= ~static_cast<uint32_t>(TM_TEMP_4);
    assert(Validate(header, data) == STATUS::OK);
    data.snr = std::numeric_limits<float>::infinity();
    assert(Validate(header, data) == STATUS::INVALID_VALUE);
    data.available_fields = 0;
    data.timestamp = 0;
    data.aircraft_count = UINT16_MAX;
    data.message_count = UINT32_MAX;
    data.rx_count = data.error_count = data.radio_resets = UINT16_MAX;
    assert(Validate(header, data) == STATUS::OK);
    data.available_fields = TM_TIMESTAMP;
    assert(Validate(header, data) == STATUS::OK);
    data.available_fields = 1UL << 14;
    assert(Validate(header, data) == STATUS::INVALID_VALUE);
    data.available_fields = 0;
    data.mode = static_cast<MODE>(6);
    assert(Validate(header, data) == STATUS::INVALID_VALUE);
}

// Exercita origem/severidade, texto máximo, estatísticas, boots e referência.
static void CheckOperationalRecords(
) {
    using namespace obc_data;
    EVENT_DATA event = {};
    event.source = SOURCE::OBC;
    event.code = 1;
    event.severity = SEVERITY::INFO;
    std::strcpy(event.description, "startup");
    assert(Validate(Header(RECORD_TYPE::EVENT), event) == STATUS::OK);
    event.severity = SEVERITY::ERROR;
    assert(Validate(Header(RECORD_TYPE::EVENT), event) == STATUS::INVALID_VALUE);
    assert(Validate(Header(RECORD_TYPE::FAULT), event) == STATUS::OK);
    event.source = SOURCE::SD;
    std::strcpy(event.description, "sd unavailable");
    assert(Validate(Header(RECORD_TYPE::FAULT), event) == STATUS::OK);
    event.code = UINT16_MAX;
    event.severity = SEVERITY::CRITICAL;
    std::memset(event.description, 'A', description_chars);
    event.description[description_chars] = '\0';
    assert(Validate(Header(RECORD_TYPE::FAULT), event) == STATUS::OK);
    event.description[description_chars] = 'A';
    assert(Validate(Header(RECORD_TYPE::FAULT), event) == STATUS::INVALID_VALUE);
    event.description[0] = '\0';
    event.source = static_cast<SOURCE>(0);
    assert(Validate(Header(RECORD_TYPE::FAULT), event) == STATUS::INVALID_VALUE);
    event.source = static_cast<SOURCE>(9);
    assert(Validate(Header(RECORD_TYPE::FAULT), event) == STATUS::INVALID_VALUE);
    event.source = SOURCE::OBC;
    event.severity = static_cast<SEVERITY>(4);
    assert(Validate(Header(RECORD_TYPE::FAULT), event) == STATUS::INVALID_VALUE);
    event.severity = SEVERITY::WARNING;
    assert(Validate(Header(RECORD_TYPE::FAULT), event) == STATUS::INVALID_VALUE);
    assert(Validate(Header(RECORD_TYPE::EVENT), event) == STATUS::OK);

    const STATISTICS_DATA example = {100, 2, 3, 1, 96, 1, 1};
    assert(Validate(Header(RECORD_TYPE::STATISTICS), example) == STATUS::OK);
    const STATISTICS_DATA stats = {UINT64_MAX, 0, 0, 0, UINT64_MAX, 0, 0};
    assert(Validate(Header(RECORD_TYPE::STATISTICS), stats) == STATUS::OK);
    assert(Validate(Header(RECORD_TYPE::MISSION), stats) == STATUS::INVALID_VALUE);
    BOOT_DATA boot = {0, 0};
    RECORD_HEADER header = Header(RECORD_TYPE::BOOT);
    header.stamp.monotonic_ms = 0;
    assert(Validate(header, boot) == STATUS::OK);
    boot.restart_count = 1;
    assert(Validate(header, boot) == STATUS::INVALID_VALUE);
    header.stamp.boot_id = UINT64_MAX;
    boot.restart_count = UINT64_MAX - 1;
    boot.reset_reason = UINT32_MAX;
    assert(Validate(header, boot) == STATUS::OK);

    header = Header(RECORD_TYPE::TIME_REFERENCE);
    obc_time::TIME_REFERENCE reference = {{1, 5000}, 1760000000, 162};
    assert(Validate(header, reference) == STATUS::OK);
    reference.reception.monotonic_ms = 5001;
    assert(Validate(header, reference) == STATUS::INVALID_VALUE);
    reference.reception = {2, 5000};
    assert(Validate(header, reference) == STATUS::INVALID_VALUE);
    reference.reception = header.stamp;
    reference.unix_s = reference.delay_ms = UINT32_MAX;
    assert(Validate(header, reference) == STATUS::OK);
}

int main(
) {
    CheckHeaders();
    CheckPayload();
    CheckPayloadTimestamp();
    CheckTelemetry();
    CheckOperationalRecords();
    return 0;
}
