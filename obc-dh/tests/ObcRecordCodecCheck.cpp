/**
 * @file ObcRecordCodecCheck.cpp
 * @brief Teste nativo do codec contra vetores independentes de struct/zlib.
 */

#include "ObcRecordCodec.h" // Codec e modelos reais usados pelo firmware.
#include <assert.h> // assert.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcRecordCodec.h".
 */

namespace codec = obc_record;
namespace data = obc_data;

// ─── HELPERS ──────────────────────────────────────────────────────

// Reproduz os exemplos lógicos, sem usar o decoder para criar as entradas.
static codec::RECORD Example(
    data::RECORD_TYPE type
) {
    codec::RECORD record = {};
    record.header = {1, type, data::GetRecordSize(type), {1, 5000}};
    switch (type) {
        case data::RECORD_TYPE::MISSION:
            record.body.mission = {42, 12, 0xE4953A, 3, 0x27, -19.95438, -43.6116, 39000, 0, 0, {}};
            std::strcpy(record.body.mission.timestamp, "2026-09-08T00:21:44Z");
            break;
        case data::RECORD_TYPE::TELEMETRY:
            record.body.telemetry = {0x3FFF, 1760000123, data::MODE::MISSION, 7.81f, 420, 9.1f, 310,
                {21.5f, 22, 23.1f, 20.9f, 24.3f}, 12.4f, 37, 3, 1200, -81, 9.5f, 10, 0, 0};
            break;
        case data::RECORD_TYPE::EVENT:
        case data::RECORD_TYPE::FAULT:
            record.body.event = {};
            record.body.event.source = type == data::RECORD_TYPE::EVENT ? data::SOURCE::OBC : data::SOURCE::SD;
            record.body.event.code = 1;
            record.body.event.severity = type == data::RECORD_TYPE::EVENT ? data::SEVERITY::INFO : data::SEVERITY::ERROR;
            std::strcpy(record.body.event.description, type == data::RECORD_TYPE::EVENT ? "startup" : "sd unavailable");
            break;
        case data::RECORD_TYPE::STATISTICS:
            record.body.statistics = {100, 2, 3, 1, 96, 1, 1};
            break;
        case data::RECORD_TYPE::BOOT:
            record.header.stamp.monotonic_ms = 0;
            record.body.boot = {0, 0};
            break;
        case data::RECORD_TYPE::TIME_REFERENCE:
            record.body.time_reference = {{1, 5000}, 1760000000, 162};
            break;
    }
    return record;
}

// Converte somente vetores hexadecimais literais, gerados por Python struct/zlib.
static size_t ReadHex(
    const char* hex,
    uint8_t* bytes
) {
    const size_t count = std::strlen(hex);
    assert(count % 2 == 0 && count / 2 <= data::max_record_bytes);
    for (size_t index = 0; index < count; index += 2) {
        unsigned value = 0;
        for (size_t next = 0; next < 2; ++next) {
            const char digit = hex[index + next];
            const unsigned nibble = digit <= '9' ? digit - '0' : digit - 'a' + 10;
            assert(nibble < 16);
            value = value * 16 + nibble;
        }
        bytes[index / 2] = static_cast<uint8_t>(value);
    }
    return count / 2;
}

// Confere a preservação lógica de cabeçalho e corpo ativo em qualquer falha.
static void CheckError(
    const uint8_t* bytes,
    size_t size,
    codec::STATUS expected
) {
    codec::RECORD output = Example(data::RECORD_TYPE::EVENT);
    output.header.stamp = {999, 777};
    output.body.event.code = 55;
    output.body.event.severity = data::SEVERITY::WARNING;
    std::strcpy(output.body.event.description, "sentinel");
    size_t consumed = 999;
    assert(codec::Decode(bytes, size, output, consumed) == expected);
    assert(consumed == 0 && output.header.version == 1);
    assert(output.header.type == data::RECORD_TYPE::EVENT && output.header.total_bytes == 96);
    assert(output.header.stamp.boot_id == 999 && output.header.stamp.monotonic_ms == 777);
    assert(output.body.event.source == data::SOURCE::OBC && output.body.event.code == 55);
    assert(output.body.event.severity == data::SEVERITY::WARNING);
    assert(std::strcmp(output.body.event.description, "sentinel") == 0);
}

// Vetores completos independentes detectam um encoder e decoder igualmente
// errados; o teste não se limita a round-trip da própria implementação.
static void CheckGoldenRecords(
) {
    const char* golden[] = {
        "4643503101016900010000000000000088130000000000002a000000000000000c0000003a95e40327dcf4673f52f433c0e71da7e848ce45c000000000000be34000000000000000000000000000000000323032362d30392d30385430303a32313a34345ab6609b6d",
        "464350310102630001000000000000008813000000000000ff3f00007b78e7680485ebf9400000d2439a99114100009b430000ac410000b041cdccb8413333a7416666c24166664641000014420300b0040000afff000018410a000000000028a43a75",
        "46435031010360000100000000000000881300000000000001010000737461727475700000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008dd4a741",
        "46435031010460000100000000000000881300000000000007010002736420756e617661696c61626c6500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001d95da5e",
        "464350310105540001000000000000008813000000000000640000000000000002000000000000000300000000000000010000000000000060000000000000000100000000000000010000000000000018a59c0c",
        "464350310106280001000000000000000000000000000000000000000000000000000000b0e8fbc4",
        "4643503101072400010000000000000088130000000000000078e768a20000008c5d0175"
    };
    const uint8_t crc_vector[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    assert(codec::ComputeCrc32(crc_vector, sizeof(crc_vector)) == 0xCBF43926UL);
    assert(codec::ComputeCrc32(nullptr, 0) == 0);
    for (uint8_t index = 0; index < 7; ++index) {
        const codec::RECORD source = Example(static_cast<data::RECORD_TYPE>(index + 1));
        uint8_t expected[data::max_record_bytes];
        const size_t size = ReadHex(golden[index], expected);
        uint8_t encoded[data::max_record_bytes];
        std::memset(encoded, 0xA5, sizeof(encoded));
        size_t written = 999;
        assert(codec::Encode(source, encoded, sizeof(encoded), written) == codec::STATUS::OK);
        assert(written == size && std::memcmp(encoded, expected, size) == 0);
        for (size_t next = size; next < sizeof(encoded); ++next) assert(encoded[next] == 0xA5);

        codec::RECORD decoded = {};
        size_t consumed = 999;
        uint8_t unaligned[data::max_record_bytes + 1];
        std::memcpy(unaligned + 1, expected, size);
        assert(codec::Decode(unaligned + 1, size, decoded, consumed) == codec::STATUS::OK);
        assert(consumed == size && decoded.header.type == source.header.type);
        assert(codec::Encode(decoded, encoded, sizeof(encoded), written) == codec::STATUS::OK);
        assert(written == size && std::memcmp(encoded, expected, size) == 0);
        for (size_t prefix = 0; prefix < size; ++prefix) CheckError(expected, prefix, codec::STATUS::INCOMPLETE);

        // Cada erro de capacidade deve preservar todos os bytes do destino.
        for (size_t capacity = 0; capacity < size; ++capacity) {
            std::memset(encoded, 0xA5, sizeof(encoded));
            assert(codec::Encode(source, encoded, capacity, written) == codec::STATUS::BUFFER_TOO_SMALL);
            assert(written == 0);
            for (size_t next = 0; next < sizeof(encoded); ++next) assert(encoded[next] == 0xA5);
        }
        expected[size - 1] ^= 1;
        CheckError(expected, size, codec::STATUS::CRC_MISMATCH);
    }
}

// Atualiza apenas o CRC de entradas malformadas para alcançar a validação
// semântica. O algoritmo de CRC já é comparado com vetores independentes.
static void RepairCrc(
    uint8_t* bytes,
    size_t size
) {
    const uint32_t crc = codec::ComputeCrc32(bytes, size - 4);
    for (unsigned index = 0; index < 4; ++index) bytes[size - 4 + index] = static_cast<uint8_t>(crc >> (index * 8));
}

// Separa corrupção de cabeçalho, CRC e conteúdo, sem alterar saídas em erro.
static void CheckFailures(
) {
    const codec::RECORD source = Example(data::RECORD_TYPE::MISSION);
    uint8_t original[data::max_record_bytes];
    size_t size = 0;
    assert(codec::Encode(source, original, sizeof(original), size) == codec::STATUS::OK);
    uint8_t bytes[data::max_record_bytes];
    std::memcpy(bytes, original, size);
    bytes[0] ^= 1;
    CheckError(bytes, size, codec::STATUS::BAD_MAGIC);
    std::memcpy(bytes, original, size); bytes[4] = 2;
    CheckError(bytes, size, codec::STATUS::UNSUPPORTED_VERSION);
    std::memcpy(bytes, original, size); bytes[5] = 255;
    CheckError(bytes, size, codec::STATUS::UNSUPPORTED_TYPE);
    std::memcpy(bytes, original, size); bytes[6] = 0; bytes[7] = 2;
    CheckError(bytes, size, codec::STATUS::INVALID_LENGTH);
    std::memcpy(bytes, original, size); bytes[16] ^= 1;
    CheckError(bytes, size, codec::STATUS::CRC_MISMATCH);
    std::memcpy(bytes, original, size); bytes[24] ^= 1;
    CheckError(bytes, size, codec::STATUS::CRC_MISMATCH);
    std::memcpy(bytes, original, size); bytes[40] |= 0x80; RepairCrc(bytes, size);
    CheckError(bytes, size, codec::STATUS::INVALID_DATA);
    std::memcpy(bytes, original, size); std::memset(bytes + 24, 0, 8); RepairCrc(bytes, size);
    CheckError(bytes, size, codec::STATUS::INVALID_DATA);
    std::memcpy(bytes, original, size); bytes[47] = 0xF8; bytes[48] = 0x7F; RepairCrc(bytes, size);
    CheckError(bytes, size, codec::STATUS::INVALID_DATA);
    CheckError(nullptr, 0, codec::STATUS::INVALID_ARGUMENT);

    codec::RECORD invalid = source;
    invalid.body.mission.session_id = 0;
    std::memset(bytes, 0xA5, sizeof(bytes));
    size_t written = 999;
    assert(codec::Encode(invalid, bytes, sizeof(bytes), written) == codec::STATUS::INVALID_DATA);
    assert(written == 0);
    for (size_t index = 0; index < sizeof(bytes); ++index) assert(bytes[index] == 0xA5);
    assert(codec::Encode(source, nullptr, size, written) == codec::STATUS::INVALID_ARGUMENT);
    assert(written == 0);
}

// Garante normalização somente da ausência e preservação exata dos dados
// presentes, inclusive zeros com sinal e limites inteiros.
static void CheckNormalizationAndLimits(
) {
    uint8_t bytes[data::max_record_bytes];
    size_t size = 0;
    size_t consumed = 0;
    codec::RECORD decoded = {};
    codec::RECORD source = Example(data::RECORD_TYPE::MISSION);
    source.body.mission.available_fields = data::PAYLOAD_VELOCITY;
    source.body.mission.velocity = -0.0;
    source.body.mission.latitude = std::numeric_limits<double>::quiet_NaN();
    source.body.mission.heading = -123;
    std::memset(source.body.mission.timestamp, 'X', sizeof(source.body.mission.timestamp));
    source.body.mission.session_id = UINT64_MAX;
    source.body.mission.seq_id = UINT32_MAX;
    source.body.mission.icao = 0xFFFFFF;
    assert(codec::Encode(source, bytes, sizeof(bytes), size) == codec::STATUS::OK);
    assert(std::isnan(source.body.mission.latitude) && source.body.mission.timestamp[0] == 'X');
    assert(codec::Decode(bytes, size, decoded, consumed) == codec::STATUS::OK);
    assert(decoded.body.mission.latitude == 0 && decoded.body.mission.heading == 0);
    assert(decoded.body.mission.timestamp[0] == '\0' && std::signbit(decoded.body.mission.velocity));
    assert(decoded.body.mission.session_id == UINT64_MAX && decoded.body.mission.seq_id == UINT32_MAX);
    assert(decoded.body.mission.icao == 0xFFFFFF);
    for (size_t index = 81; index < 101; ++index) assert(bytes[index] == 0);

    source = Example(data::RECORD_TYPE::TELEMETRY);
    source.body.telemetry.available_fields = data::TM_VBAT | data::TM_RSSI;
    source.body.telemetry.vbat = -0.0f;
    source.body.telemetry.snr = std::numeric_limits<float>::quiet_NaN();
    source.body.telemetry.rssi = INT16_MIN;
    assert(codec::Encode(source, bytes, sizeof(bytes), size) == codec::STATUS::OK);
    assert(codec::Decode(bytes, size, decoded, consumed) == codec::STATUS::OK);
    assert(std::signbit(decoded.body.telemetry.vbat) && decoded.body.telemetry.snr == 0);
    assert(decoded.body.telemetry.timestamp == 0 && decoded.body.telemetry.rssi == INT16_MIN);
    source.body.telemetry.rssi = INT16_MAX;
    assert(codec::Encode(source, bytes, sizeof(bytes), size) == codec::STATUS::OK);
    assert(codec::Decode(bytes, size, decoded, consumed) == codec::STATUS::OK);
    assert(decoded.body.telemetry.rssi == INT16_MAX);

    source = Example(data::RECORD_TYPE::EVENT);
    std::memset(source.body.event.description, 'A', data::description_chars);
    source.body.event.description[data::description_chars] = '\0';
    assert(codec::Encode(source, bytes, sizeof(bytes), size) == codec::STATUS::OK);
    assert(codec::Decode(bytes, size, decoded, consumed) == codec::STATUS::OK);
    assert(std::strlen(decoded.body.event.description) == data::description_chars);
    source.body.event.description[1] = '\0';
    assert(codec::Encode(source, bytes, sizeof(bytes), size) == codec::STATUS::OK);
    for (size_t index = 29; index < 92; ++index) assert(bytes[index] == 0);
}

// Decode informa exatamente o primeiro comprimento, mesmo com outro registro.
static void CheckConsecutive(
) {
    uint8_t bytes[data::max_record_bytes];
    size_t first = 0;
    size_t second = 0;
    assert(codec::Encode(Example(data::RECORD_TYPE::MISSION), bytes, sizeof(bytes), first) == codec::STATUS::OK);
    assert(codec::Encode(Example(data::RECORD_TYPE::TIME_REFERENCE), bytes + first,
        sizeof(bytes) - first, second) == codec::STATUS::OK);
    codec::RECORD output = {};
    size_t consumed = 0;
    assert(codec::Decode(bytes, first + second, output, consumed) == codec::STATUS::OK);
    assert(consumed == first && output.header.type == data::RECORD_TYPE::MISSION);
    assert(codec::Decode(bytes + first, second, output, consumed) == codec::STATUS::OK);
    assert(consumed == second && output.header.type == data::RECORD_TYPE::TIME_REFERENCE);
    assert(output.body.time_reference.reception.boot_id == output.header.stamp.boot_id);
}

int main(
) {
    CheckGoldenRecords();
    CheckFailures();
    CheckNormalizationAndLimits();
    CheckConsecutive();
    return 0;
}
