/**
 * @file SnapshotCodecCheck.cpp
 * @brief Compara o codec v2 com vetores independentes gerados pelo teste Python.
 * @details Compilar com ObcData.cpp/ObcRecordCodec.cpp e executar pelo script
 * SnapshotRecordsCheck.py, que fornece diretório temporário com quatro vetores.
 */

#include "ObcRecordCodec.h" // Codec real, modelos, limites e CRC.
#include <assert.h> // assert.
#include <fstream> // Vetores binários independentes.
#include <iterator> // istreambuf_iterator.
#include <cstdio> // std::printf.
#include <string>
#include <vector>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcRecordCodec.h".
 */

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

// Modelos e buffers grandes ficam fora da pilha, inclusive neste teste nativo.
static obc_data::PAYLOAD_SNAPSHOT source;
static obc_data::PAYLOAD_SNAPSHOT decoded;
static obc_data::PAYLOAD_SNAPSHOT saved;
static uint8_t output[obc_record::max_snapshot_record_bytes + 16];

// ─── HELPERS ──────────────────────────────────────────────────────

// Os mesmos valores lógicos são serializados independentemente por struct/zlib.
static void BuildFixture(
    unsigned fixture
) {
    using namespace obc_data;
    std::memset(&source, 0, sizeof(source));
    source.stamp = {1, 0};
    if (fixture == 0) return;
    source.id = UINT64_MAX;
    source.stamp = {UINT64_MAX, UINT64_MAX};
    source.aircraft_count = 1;
    source.aircraft[0].icao = 0xFFFFFF;
    if (fixture == 1) {
        source.aircraft[0].latitude = std::numeric_limits<double>::quiet_NaN();
        std::memset(source.aircraft[0].identification, 'X', sizeof(source.aircraft[0].identification));
        return;
    }
    if (fixture == 2) {
        source.id = 480;
        source.stamp = {7, 5000};
        source.aircraft_count = 2;
        AIRCRAFT_STATE& first = source.aircraft[0];
        first.icao = 0xE49A48;
        first.available_fields = 31;
        first.latitude = -20.109775;
        first.longitude = -44.370883;
        first.altitude = 38000;
        std::strcpy(first.identification, "GLO1669");
        std::strcpy(first.tick, "2026-11-03T17:50:21.280810+00:00");
        first.extra_count = 2;
        std::strcpy(first.extras[0].key, "EW velocity");
        std::strcpy(first.extras[0].value, "-282");
        std::strcpy(first.extras[1].key, "note");
        std::strcpy(first.extras[1].value, "line\n\"x\"\\\xC3\xA9");
        AIRCRAFT_STATE& second = source.aircraft[1];
        second.icao = 0xE495B2;
        second.available_fields = SNAPSHOT_ALTITUDE | SNAPSHOT_TICK;
        second.altitude = -0.0;
        second.extra_count = 2;
        std::strcpy(second.extras[0].key, "Latitude");
        std::strcpy(second.extras[0].value, "96298 (not decoded)");
        std::strcpy(second.extras[1].key, "Longitude");
        std::strcpy(second.extras[1].value, "26514 (not decoded)");
        return;
    }
    source.aircraft_count = max_snapshot_aircraft;
    for (size_t index = 0; index < source.aircraft_count; ++index) {
        AIRCRAFT_STATE& data = source.aircraft[index];
        data.icao = static_cast<uint32_t>(index);
        data.available_fields = 31;
        data.latitude = 90;
        data.longitude = -180;
        data.altitude = -0.0;
        std::memset(data.identification, 'I', identification_chars);
        std::memset(data.tick, 'T', tick_chars);
        data.extra_count = max_aircraft_extras;
        for (size_t extra = 0; extra < data.extra_count; ++extra) {
            std::memset(data.extras[extra].key, 'K', extra_key_chars);
            data.extras[extra].key[0] = static_cast<char>('A' + extra);
            std::memset(data.extras[extra].value, 'V', extra_value_chars);
        }
    }
}

// Qualquer rejeição deve preservar toda a saída, inclusive os slots inativos.
static void ExpectFailure(
    const uint8_t* data,
    size_t size,
    obc_record::STATUS expected
) {
    std::memset(&decoded, 0x5A, sizeof(decoded));
    std::memcpy(&saved, &decoded, sizeof(saved));
    size_t consumed = 99;
    assert(obc_record::DecodeSnapshot(data, size, decoded, consumed) == expected);
    assert(consumed == 0 && std::memcmp(&decoded, &saved, sizeof(decoded)) == 0);
}

// Atualiza CRC após alteração deliberada para exercitar validação semântica.
static void RefreshCrc(
    std::vector<uint8_t>& data
) {
    const uint32_t crc = obc_record::ComputeCrc32(data.data(), data.size() - 4);
    for (unsigned index = 0; index < 4; ++index) {
        data[data.size() - 4 + index] = static_cast<uint8_t>(crc >> (8 * index));
    }
}

// Confere limites de buffer, truncamento, normalização e byte a byte dos vetores.
static void CheckFixture(
    const char* directory,
    unsigned fixture
) {
    using namespace obc_record;
    BuildFixture(fixture);
    const std::string path = std::string(directory) + "/" + static_cast<char>('0' + fixture) + ".bin";
    std::ifstream input(path.c_str(), std::ios::binary);
    assert(input);
    const std::vector<uint8_t> golden((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    size_t size = 123;
    assert(GetSnapshotSize(source, size) == STATUS::OK && size == golden.size());
    if (fixture == 0) assert(size == 37);
    if (fixture == 1) assert(size == 42);
    if (fixture == 3) assert(size == 64497 && size == max_snapshot_record_bytes);

    std::memcpy(&saved, &source, sizeof(source));
    std::memset(output, 0xA5, sizeof(output));
    size_t written = 99;
    assert(EncodeSnapshot(source, output + 1, size, written) == STATUS::OK);
    assert(written == size && std::memcmp(output + 1, golden.data(), size) == 0);
    assert(output[0] == 0xA5 && output[size + 1] == 0xA5);
    assert(std::memcmp(&source, &saved, sizeof(source)) == 0);
    assert(EncodeSnapshot(source, nullptr, size, written) == STATUS::INVALID_ARGUMENT && written == 0);

    // Vetores curtos exercitam todos os prefixos/capacidades; o maior usa
    // fronteiras de cabeçalho/corpo/CRC, evitando varrer gigabytes redundantes.
    const size_t step = fixture == 3 ? size - 1 : 1;
    for (size_t prefix = 0; prefix < size; prefix += step) {
        ExpectFailure(golden.data(), prefix, STATUS::INCOMPLETE);
        std::memset(output, 0xA5, sizeof(output));
        assert(EncodeSnapshot(source, output, prefix, written) == STATUS::BUFFER_TOO_SMALL && written == 0);
        for (size_t index = 0; index < sizeof(output); ++index) assert(output[index] == 0xA5);
    }
    ExpectFailure(golden.data(), 24, STATUS::INCOMPLETE);
    size_t consumed = 0;
    assert(DecodeSnapshot(golden.data(), size, decoded, consumed) == STATUS::OK && consumed == size);
    assert(obc_data::Validate(decoded) == obc_data::STATUS::OK);
    assert(decoded.id == source.id && decoded.aircraft_count == source.aircraft_count);
    assert(decoded.stamp.boot_id == source.stamp.boot_id && decoded.stamp.monotonic_ms == source.stamp.monotonic_ms);
    if (fixture == 1) {
        assert(decoded.aircraft[0].latitude == 0 && decoded.aircraft[0].identification[0] == '\0');
        assert(decoded.aircraft[1].icao == 0);
    }
    if (fixture == 2) assert(std::signbit(decoded.aircraft[1].altitude));
    assert(EncodeSnapshot(decoded, output, sizeof(output), written) == STATUS::OK);
    assert(written == size && std::memcmp(output, golden.data(), size) == 0);

    std::vector<uint8_t> changed = golden;
    changed[0] = 'X'; ExpectFailure(changed.data(), changed.size(), STATUS::BAD_MAGIC);
    changed = golden; changed[4] = 3; ExpectFailure(changed.data(), changed.size(), STATUS::UNSUPPORTED_VERSION);
    changed = golden; changed[5] = 9; ExpectFailure(changed.data(), changed.size(), STATUS::UNSUPPORTED_TYPE);
    changed = golden; changed[6] = 36; changed[7] = 0; ExpectFailure(changed.data(), changed.size(), STATUS::INVALID_LENGTH);
    changed = golden; changed[6] = 255; changed[7] = 255; ExpectFailure(changed.data(), changed.size(), STATUS::INVALID_LENGTH);
    changed = golden; changed.back() ^= 1; ExpectFailure(changed.data(), changed.size(), STATUS::CRC_MISMATCH);
    changed = golden; std::memset(changed.data() + 8, 0, 8); RefreshCrc(changed);
    ExpectFailure(changed.data(), changed.size(), STATUS::INVALID_DATA);
    changed = golden; changed[32] = 21; RefreshCrc(changed);
    ExpectFailure(changed.data(), changed.size(), STATUS::INVALID_LENGTH);

    source.stamp.boot_id = 0;
    size = 99;
    assert(GetSnapshotSize(source, size) == STATUS::INVALID_DATA && size == 0);
    std::memset(output, 0xA5, sizeof(output));
    assert(EncodeSnapshot(source, output, sizeof(output), written) == STATUS::INVALID_DATA && written == 0);
    for (size_t index = 0; index < sizeof(output); ++index) assert(output[index] == 0xA5);
}

// CRC correto não torna válidos comprimentos internos ou domínios impossíveis.
static void CheckMalformedBody(
) {
    using namespace obc_record;
    BuildFixture(2);
    size_t size = 0;
    assert(EncodeSnapshot(source, output, sizeof(output), size) == STATUS::OK);
    const std::vector<uint8_t> valid(output, output + size);
    for (const size_t position : {size_t(61), size_t(69), size_t(103), size_t(115)}) {
        std::vector<uint8_t> changed = valid;
        changed[position] = 255;
        RefreshCrc(changed);
        ExpectFailure(changed.data(), changed.size(), STATUS::INVALID_LENGTH);
    }
    std::vector<uint8_t> changed = valid;
    changed[62] = 0; RefreshCrc(changed);
    ExpectFailure(changed.data(), changed.size(), STATUS::INVALID_DATA);
    changed = valid; changed[36] = 128; RefreshCrc(changed);
    ExpectFailure(changed.data(), changed.size(), STATUS::INVALID_DATA);
    changed = valid; changed[102] = 33; RefreshCrc(changed);
    ExpectFailure(changed.data(), changed.size(), STATUS::INVALID_LENGTH);
    changed = valid;
    const uint64_t nan_bits = UINT64_C(0x7FF8000000000000);
    for (unsigned byte = 0; byte < 8; ++byte) changed[37 + byte] = static_cast<uint8_t>(nan_bits >> (byte * 8));
    RefreshCrc(changed);
    ExpectFailure(changed.data(), changed.size(), STATUS::INVALID_DATA);

    BuildFixture(0);
    source.aircraft_count = 2;
    source.aircraft[0].icao = 1;
    source.aircraft[1].icao = 2;
    assert(EncodeSnapshot(source, output, sizeof(output), size) == STATUS::OK);
    changed.assign(output, output + size);
    changed[38] = 1; RefreshCrc(changed);
    ExpectFailure(changed.data(), changed.size(), STATUS::INVALID_DATA);
    ExpectFailure(nullptr, size, STATUS::INVALID_ARGUMENT);
}

int main(
    int argc,
    char** argv
) {
    assert(argc == 2);
    for (unsigned fixture = 0; fixture < 4; ++fixture) CheckFixture(argv[1], fixture);
    CheckMalformedBody();
    std::printf("Snapshot v2: vetores independentes, limites e saidas preservadas aprovados.\n");
    return 0;
}
