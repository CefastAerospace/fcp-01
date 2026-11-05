/**
 * @file PayloadSnapshotCheck.cpp
 * @brief Verificação nativa dos modelos de snapshot, sem parser ou hardware.
 * @details Executar em obc-dh/:
 * g++ -std=c++11 -Wall -Wextra -Werror -Iheaders src/ObcData.cpp tests/PayloadSnapshotCheck.cpp -o /tmp/fcp01-snapshot-check
 * /tmp/fcp01-snapshot-check
 */

#include "models/ObcData.h" // Modelos, disponibilidade e validadores reutilizados.
#include <assert.h> // assert.
#include <cstdio> // std::printf e std::snprintf para limites e relatório de RAM.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "models/ObcData.h".
 */

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

// O snapshot não cabe na pilha padrão de uma tarefa FreeRTOS.
static obc_data::PAYLOAD_SNAPSHOT snapshot;

// ─── HELPERS ──────────────────────────────────────────────────────

// Representa manualmente estados observados em payload.json. Não comprova
// extração de strings, conversão de unidades nem interpretação de JSON.
static void CheckExamples(
) {
    using namespace obc_data;
    snapshot.id = 480;
    snapshot.stamp = {1, 5000};
    snapshot.aircraft_count = 2;
    AIRCRAFT_STATE& decoded = snapshot.aircraft[0];
    decoded.icao = 0xE49A48;
    decoded.available_fields = SNAPSHOT_LATITUDE | SNAPSHOT_LONGITUDE
        | SNAPSHOT_ALTITUDE | SNAPSHOT_IDENTIFICATION | SNAPSHOT_TICK;
    decoded.latitude = -20.109775;
    decoded.longitude = -44.370883;
    decoded.altitude = 38000;
    std::strcpy(decoded.identification, "GLO1669");
    std::strcpy(decoded.tick, "2026-11-03T17:50:21.280810+00:00");
    decoded.extra_count = 2;
    std::strcpy(decoded.extras[0].key, "EW velocity");
    std::strcpy(decoded.extras[0].value, "-282");
    std::strcpy(decoded.extras[1].key, "Capability");
    std::strcpy(decoded.extras[1].value,
        "5 (Level 2+3+4 (DF0,4,5,11,20,21,24,code7 - is airborne))");

    AIRCRAFT_STATE& undecoded = snapshot.aircraft[1];
    undecoded.icao = 0xE495B2;
    undecoded.available_fields = SNAPSHOT_ALTITUDE | SNAPSHOT_TICK;
    undecoded.altitude = 13050;
    std::strcpy(undecoded.tick, "2026-11-03T17:47:45.778932+00:00");
    undecoded.extra_count = 2;
    std::strcpy(undecoded.extras[0].key, "Latitude");
    std::strcpy(undecoded.extras[0].value, "96298 (not decoded)");
    std::strcpy(undecoded.extras[1].key, "Longitude");
    std::strcpy(undecoded.extras[1].value, "26514 (not decoded)");
    assert(Validate(snapshot) == STATUS::OK);
    assert((undecoded.available_fields & (SNAPSHOT_LATITUDE | SNAPSHOT_LONGITUDE)) == 0);
    assert(std::strcmp(undecoded.extras[0].value, "96298 (not decoded)") == 0);
    assert(std::strcmp(decoded.tick, "2026-11-03T17:50:21.280810+00:00") == 0);
}

// Zero, valores negativos de altitude e identidade sem dados de voo são
// distintos de ausência; a máscara determina quais valores serão verificados.
static void CheckAvailability(
) {
    using namespace obc_data;
    AIRCRAFT_STATE data = {};
    data.icao = 0xFFFFFF;
    assert(Validate(data) == STATUS::OK);
    data.available_fields = SNAPSHOT_IDENTIFICATION;
    std::strcpy(data.identification, "GLO1669");
    assert(Validate(data) == STATUS::OK);
    data.available_fields = SNAPSHOT_LATITUDE | SNAPSHOT_LONGITUDE | SNAPSHOT_ALTITUDE;
    assert(Validate(data) == STATUS::OK);
    data.latitude = -90;
    data.longitude = 180;
    data.altitude = -100;
    assert(Validate(data) == STATUS::OK);
    data.latitude = 90;
    data.longitude = -180;
    assert(Validate(data) == STATUS::OK);
    data.latitude = 91;
    assert(Validate(data) == STATUS::INVALID_VALUE);
    data.latitude = 0;
    data.longitude = -181;
    assert(Validate(data) == STATUS::INVALID_VALUE);
    data.longitude = 0;
    data.altitude = std::numeric_limits<double>::infinity();
    assert(Validate(data) == STATUS::INVALID_VALUE);
    data.altitude = 0;
    data.latitude = std::numeric_limits<double>::quiet_NaN();
    assert(Validate(data) == STATUS::INVALID_VALUE);
    data.available_fields = 0;
    assert(Validate(data) == STATUS::OK);
    data.available_fields = SNAPSHOT_LATITUDE;
    assert(Validate(data) == STATUS::INVALID_VALUE);
    data.available_fields = SNAPSHOT_LONGITUDE;
    assert(Validate(data) == STATUS::INVALID_VALUE);
    data.available_fields = 0x20;
    assert(Validate(data) == STATUS::INVALID_VALUE);
    data.available_fields = 0;
    data.icao = 0x1000000;
    assert(Validate(data) == STATUS::INVALID_VALUE);
}

// Aceita strings no limite, rejeita falta de NUL e não modifica ou trunca
// entradas. Textos indisponíveis e slots além da contagem são ignorados.
static void CheckTextAndExtras(
) {
    using namespace obc_data;
    AIRCRAFT_STATE data = {};
    data.available_fields = SNAPSHOT_IDENTIFICATION | SNAPSHOT_TICK;
    std::memset(data.identification, 'I', identification_chars);
    std::memset(data.tick, 'T', tick_chars);
    assert(Validate(data) == STATUS::OK);
    data.identification[identification_chars] = 'I';
    assert(Validate(data) == STATUS::INVALID_LENGTH);
    assert(data.identification[identification_chars] == 'I');
    data.identification[identification_chars] = '\0';
    data.tick[tick_chars] = 'T';
    assert(Validate(data) == STATUS::INVALID_LENGTH);
    data.available_fields = 0;
    assert(Validate(data) == STATUS::OK);

    data.extra_count = max_aircraft_extras;
    for (size_t index = 0; index < max_aircraft_extras; ++index) {
        std::memset(data.extras[index].key, 'K', extra_key_chars);
        data.extras[index].key[0] = static_cast<char>('A' + index);
        std::memset(data.extras[index].value, 'V', extra_value_chars);
    }
    assert(Validate(data) == STATUS::OK);
    data.extra_count = max_aircraft_extras + 1;
    assert(Validate(data) == STATUS::INVALID_LENGTH);
    data.extra_count = 1;
    data.extras[0].key[extra_key_chars] = 'K';
    assert(Validate(data) == STATUS::INVALID_LENGTH);
    data.extras[0].key[extra_key_chars] = '\0';
    data.extras[0].value[extra_value_chars] = 'V';
    assert(Validate(data) == STATUS::INVALID_LENGTH);
    data.extras[0].value[extra_value_chars] = '\0';
    data.extras[0].key[0] = '\0';
    assert(Validate(data) == STATUS::INVALID_VALUE);
    std::strcpy(data.extras[0].key, "key");
    data.extras[0].value[0] = '\0';
    assert(Validate(data) == STATUS::OK);
    data.extra_count = 2;
    std::strcpy(data.extras[1].key, "key");
    assert(Validate(data) == STATUS::INVALID_VALUE);
    std::strcpy(data.extras[1].key, "Key");
    assert(Validate(data) == STATUS::OK);
}

// Valida cardinalidade e identidade sem depender de ordem, id crescente ou
// presença de aeronaves. O gerenciamento de substituição pertence ao receptor.
static void CheckSnapshot(
) {
    using namespace obc_data;
    std::memset(&snapshot, 0, sizeof(snapshot));
    assert(Validate(snapshot) == STATUS::INVALID_VALUE);
    snapshot.stamp = {1, 0};
    assert(Validate(snapshot) == STATUS::OK);
    snapshot.id = UINT64_MAX;
    snapshot.stamp = {UINT64_MAX, UINT64_MAX};
    snapshot.aircraft_count = max_snapshot_aircraft;
    for (size_t index = 0; index < max_snapshot_aircraft; ++index) {
        snapshot.aircraft[index].icao = static_cast<uint32_t>(index);
    }
    assert(Validate(snapshot) == STATUS::OK);
    snapshot.aircraft_count = max_snapshot_aircraft + 1;
    assert(Validate(snapshot) == STATUS::INVALID_LENGTH);
    snapshot.aircraft_count = max_snapshot_aircraft;
    snapshot.aircraft[0].icao = snapshot.aircraft[1].icao;
    assert(Validate(snapshot) == STATUS::INVALID_VALUE);
    snapshot.aircraft[0].icao = 0xFFFFFF;
    assert(Validate(snapshot) == STATUS::OK);
    snapshot.aircraft[0].available_fields = 0x80;
    assert(Validate(snapshot) == STATUS::INVALID_VALUE);
    snapshot.aircraft_count = 0;
    assert(Validate(snapshot) == STATUS::OK);
}

int main(
) {
    CheckExamples();
    CheckAvailability();
    CheckTextAndExtras();
    CheckSnapshot();
    std::printf("sizeof(EXTRA_FIELD)=%zu sizeof(AIRCRAFT_STATE)=%zu sizeof(PAYLOAD_SNAPSHOT)=%zu\n",
        sizeof(obc_data::EXTRA_FIELD), sizeof(obc_data::AIRCRAFT_STATE),
        sizeof(obc_data::PAYLOAD_SNAPSHOT));
    return 0;
}
