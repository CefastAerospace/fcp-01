/** @file TelemetryV3Check.cpp
 * @brief Confere vetor independente v3, falhas e rejeição pelo codec v1.
 */
#include "ObcRecordCodec.h" // Codificadores/decodificadores reais.
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcRecordCodec.h".
 */

int main(
    int argc,
    char** argv
) {
    assert(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<uint8_t> expected((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    assert(expected.size() == 99);
    obc_data::TELEMETRY_DATA tm = {};
    tm.available_fields = obc_data::TM_TIMESTAMP | obc_data::TM_AIRCRAFT
        | obc_data::TM_RX_COUNT | obc_data::TM_ERROR_COUNT | obc_data::TM_RADIO_RESETS;
    tm.mode = obc_data::MODE::BENCH;
    tm.message_count = 123; // Indisponível: não pode vazar para o arquivo.
    tm.rssi = -90;
    tm.rx_count = 3;
    tm.error_count = 1;
    tm.radio_resets = 2;
    uint8_t buffer[99];
    size_t count = 0;
    const obc_time::EVENT_STAMP stamp = {7, 5000};
    assert(obc_record::EncodeTelemetryV3(stamp, tm, buffer, sizeof(buffer), count) == obc_record::STATUS::OK);
    assert(count == expected.size() && std::memcmp(buffer, expected.data(), count) == 0);
    obc_record::RECORD record = {};
    assert(obc_record::Decode(buffer, count, record, count) == obc_record::STATUS::BAD_MAGIC);
    assert(count == 0);
    assert(obc_record::DecodeTelemetryV3(buffer, sizeof(buffer), record, count) == obc_record::STATUS::OK);
    assert(count == 99 && record.header.version == 3 && static_cast<unsigned>(record.header.type) == 9);
    assert(record.body.telemetry.message_count == 0 && record.body.telemetry.rssi == 0);
    assert(record.body.telemetry.available_fields == tm.available_fields);
    const obc_record::RECORD prior = record;
    for (size_t length = 0; length < sizeof(buffer); ++length) {
        assert(obc_record::DecodeTelemetryV3(buffer, length, record, count) == obc_record::STATUS::INCOMPLETE);
        assert(count == 0 && std::memcmp(&prior, &record, sizeof(record)) == 0);
    }
    buffer[40] ^= 1;
    assert(obc_record::DecodeTelemetryV3(buffer, sizeof(buffer), record, count) == obc_record::STATUS::CRC_MISMATCH);
    assert(std::memcmp(&prior, &record, sizeof(record)) == 0);
    tm.available_fields |= 1UL << 19;
    uint8_t prior_buffer[99];
    std::memcpy(prior_buffer, buffer, sizeof(buffer));
    assert(obc_record::EncodeTelemetryV3(stamp, tm, buffer, sizeof(buffer), count) == obc_record::STATUS::INVALID_DATA);
    assert(count == 0 && std::memcmp(prior_buffer, buffer, sizeof(buffer)) == 0);
}
