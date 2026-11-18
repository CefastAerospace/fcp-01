/** @file ObcHousekeepingCheck.cpp
 * @brief Composição real, disponibilidade dos proprietários e ida/volta pelo codec v3.
 */
#include "ObcHousekeeping.h" // Composição compartilhada pela bancada ESP32.
#include "ObcEps.h" // Validade existente, sem reimplementar o prazo.
#include "ObcThermal.h" // Validade e geração I2C existentes.
#include "ObcRecordCodec.h" // Codec real de TM v3.
#include <cassert>
#include <cstdio>

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcHousekeeping.h".
 */

namespace {

    // Vetor independente dos cenários USB, com temperaturas exatas em float.
    obc_housekeeping::READINGS Nominal(
    ) {
        obc_housekeeping::READINGS readings = {};
        readings.battery = {true, 7.8f, 0.42f};
        readings.solar = {true, 9.1f, 0.31f};
        for (size_t index = 0; index < obc_data::thermal_sensors; ++index) {
            readings.temperatures[index] = {true, 20.0f + static_cast<float>(index)};
        }
        return readings;
    }

    // Valores sentinela verificam que somente os campos da integração mudam.
    void CheckComposition(
    ) {
        obc_data::TELEMETRY_DATA telemetry = {};
        telemetry.available_fields = (1UL << 19) - 1;
        telemetry.mode = obc_data::MODE::MISSION;
        telemetry.timestamp = 123;
        telemetry.rate_dps = 1.5f;
        telemetry.azimuth_deg = 13;
        telemetry.aircraft_count = 7;
        telemetry.message_count = 999;
        telemetry.rssi = -81;
        telemetry.snr = 9.5f;
        telemetry.rx_count = 21;
        telemetry.error_count = 2;
        telemetry.radio_resets = 3;
        const auto before = telemetry;
        auto readings = Nominal();
        obc_housekeeping::ApplyTelemetry(readings, telemetry);
        assert(telemetry.vbat == 7.8f && std::fabs(telemetry.ibat - 420) < 0.001f);
        assert(telemetry.vsolar == 9.1f && std::fabs(telemetry.isolar - 310) < 0.001f);
        assert(obc_housekeeping::AllTemperaturesAvailable(telemetry));
        readings.battery = {true, 0, 0};
        readings.solar.current_a = -0.31f;
        readings.temperatures[0].temperature_c = -12.5f;
        obc_housekeeping::ApplyTelemetry(readings, telemetry);
        assert(telemetry.vbat == 0 && telemetry.ibat == 0);
        assert((telemetry.available_fields & (obc_data::TM_VBAT | obc_data::TM_IBAT))
            == (obc_data::TM_VBAT | obc_data::TM_IBAT));
        assert(std::fabs(telemetry.isolar + 310) < 0.001f && telemetry.temps[0] == -12.5f);
        assert((telemetry.available_fields & ~obc_housekeeping::telemetry_fields)
            == (before.available_fields & ~obc_housekeeping::telemetry_fields));
        assert(telemetry.mode == before.mode && telemetry.timestamp == before.timestamp);
        assert(telemetry.rate_dps == before.rate_dps && telemetry.azimuth_deg == before.azimuth_deg);
        assert(telemetry.aircraft_count == before.aircraft_count && telemetry.message_count == before.message_count);
        assert(telemetry.rssi == before.rssi && telemetry.snr == before.snr);
        assert(telemetry.rx_count == before.rx_count && telemetry.error_count == before.error_count
            && telemetry.radio_resets == before.radio_resets);

        // Uma entrada ausente limpa também os valores, evitando NaN no serializador externo.
        readings = {};
        obc_housekeeping::ApplyTelemetry(readings, telemetry);
        assert(!(telemetry.available_fields & obc_housekeeping::telemetry_fields));
        assert(telemetry.vbat == 0 && telemetry.ibat == 0 && telemetry.vsolar == 0 && telemetry.isolar == 0);
        for (float temperature : telemetry.temps) assert(temperature == 0);
    }

    // Falhas numéricas ficam locais à grandeza, sem apagar os demais canais.
    void CheckNumbers(
    ) {
        const float invalid[] = {std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
        for (float value : invalid) {
            auto readings = Nominal();
            readings.battery.voltage_v = value;
            readings.solar.current_a = value;
            readings.temperatures[2].temperature_c = value;
            obc_data::TELEMETRY_DATA telemetry = {};
            obc_housekeeping::ApplyTelemetry(readings, telemetry);
            assert(!(telemetry.available_fields & (obc_data::TM_VBAT | obc_data::TM_ISOLAR | obc_data::TM_TEMP_2)));
            assert(telemetry.available_fields & obc_data::TM_IBAT);
            assert(telemetry.available_fields & obc_data::TM_VSOLAR);
            assert(telemetry.vbat == 0 && telemetry.isolar == 0 && telemetry.temps[2] == 0);
        }
        const float signs[] = {-1.0f, 1.0f};
        for (float sign : signs) {
            auto readings = Nominal();
            readings.battery.current_a = sign * std::numeric_limits<float>::max();
            obc_data::TELEMETRY_DATA telemetry = {};
            obc_housekeeping::ApplyTelemetry(readings, telemetry);
            assert(!(telemetry.available_fields & obc_data::TM_IBAT) && telemetry.ibat == 0);
            assert(telemetry.available_fields & obc_data::TM_VBAT);
        }
    }

    // Disponibilidade individual atravessa o codec, mesmo quando o conjunto TT&C não está disponível.
    void CheckPartialRecords(
    ) {
        for (size_t missing = 0; missing < obc_data::thermal_sensors; ++missing) {
            auto readings = Nominal();
            readings.temperatures[missing].available = false;
            obc_data::TELEMETRY_DATA telemetry = {};
            obc_housekeeping::ApplyTelemetry(readings, telemetry);
            assert(!obc_housekeeping::AllTemperaturesAvailable(telemetry));
            uint8_t bytes[obc_record::telemetry_record_bytes];
            size_t size = 0;
            const obc_time::EVENT_STAMP stamp = {9, 0};
            assert(obc_record::EncodeTelemetryV3(stamp, telemetry, bytes, sizeof(bytes), size)
                == obc_record::STATUS::OK);
            obc_record::RECORD record = {};
            size_t consumed = 0;
            assert(obc_record::DecodeTelemetryV3(bytes, size, record, consumed) == obc_record::STATUS::OK);
            assert(consumed == 99 && record.header.stamp.boot_id == 9 && record.header.stamp.monotonic_ms == 0);
            assert(record.body.telemetry.available_fields == telemetry.available_fields);
            for (size_t index = 0; index < obc_data::thermal_sensors; ++index) {
                const uint32_t bit = static_cast<uint32_t>(obc_data::TM_TEMP_0) << index;
                assert(((record.body.telemetry.available_fields & bit) != 0) == (index != missing));
                assert(record.body.telemetry.temps[index] == (index == missing ? 0 : 20.0f + index));
            }
        }
    }

    // Fronteiras usam as mesmas funções dos produtores; o mapeador só recebe sua decisão.
    void CheckAvailability(
    ) {
        obc_eps::CHANNEL_STATE eps = {};
        eps.configured = eps.has_valid = eps.latest_valid = true;
        eps.last_valid_ms = 10;
        obc_thermal::CHANNEL_STATE thermal = {};
        thermal.configured = thermal.has_valid = thermal.latest_valid = true;
        thermal.last_valid_ms = 10;
        thermal.recovery_cycles = 7;
        const uint64_t times[] = {9, 10, 3010, 3011};
        const bool expected[] = {false, true, true, false};
        for (size_t index = 0; index < 4; ++index) {
            auto readings = Nominal();
            readings.battery.available = obc_eps::IsAvailable(eps, times[index]);
            readings.temperatures[0].available = obc_thermal::IsAvailable(thermal, times[index], true, 7);
            obc_data::TELEMETRY_DATA telemetry = {};
            obc_housekeeping::ApplyTelemetry(readings, telemetry);
            assert(((telemetry.available_fields & obc_data::TM_VBAT) != 0) == expected[index]);
            assert(((telemetry.available_fields & obc_data::TM_TEMP_0) != 0) == expected[index]);
        }
        assert(!obc_thermal::IsAvailable(thermal, 10, true, 8));
        assert(!obc_thermal::IsAvailable(thermal, 10, false, 7));
        eps.latest_valid = thermal.latest_valid = false;
        assert(!obc_eps::IsAvailable(eps, 10));
        assert(!obc_thermal::IsAvailable(thermal, 10, true, 7));
    }

} // namespace

int main(
) {
    CheckComposition();
    CheckNumbers();
    CheckPartialRecords();
    CheckAvailability();
    std::puts("Housekeeping: composicao, numeros, disponibilidade e TM v3 aprovados.");
}
