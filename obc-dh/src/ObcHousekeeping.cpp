#include "ObcHousekeeping.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcHousekeeping.h".
 */

namespace obc_housekeeping {
namespace {

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Valida cada grandeza; corrente inválida não torna uma tensão finita indisponível.
    void ApplyEnergy(
        const ENERGY_SAMPLE& sample,
        uint32_t voltage_field,
        uint32_t current_field,
        float& voltage_v,
        float& current_ma,
        uint32_t& available_fields
    ) {
        voltage_v = 0;
        current_ma = 0;
        if (!sample.available) return;
        if (std::isfinite(sample.voltage_v)) {
            voltage_v = sample.voltage_v;
            available_fields |= voltage_field;
        }
        // double evita produzir overflow float durante a própria multiplicação.
        const double milliamps = static_cast<double>(sample.current_a) * 1000.0;
        if (std::isfinite(milliamps) && std::fabs(milliamps) <= std::numeric_limits<float>::max()) {
            current_ma = static_cast<float>(milliamps);
            available_fields |= current_field;
        }
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

void ApplyTelemetry(
    const READINGS& readings,
    obc_data::TELEMETRY_DATA& telemetry
) {
    telemetry.available_fields &= ~telemetry_fields;
    ApplyEnergy(readings.battery, obc_data::TM_VBAT, obc_data::TM_IBAT,
        telemetry.vbat, telemetry.ibat, telemetry.available_fields);
    ApplyEnergy(readings.solar, obc_data::TM_VSOLAR, obc_data::TM_ISOLAR,
        telemetry.vsolar, telemetry.isolar, telemetry.available_fields);
    for (size_t index = 0; index < obc_data::thermal_sensors; ++index) {
        const auto& sample = readings.temperatures[index];
        telemetry.temps[index] = 0;
        if (sample.available && std::isfinite(sample.temperature_c)) {
            telemetry.temps[index] = sample.temperature_c;
            telemetry.available_fields |= static_cast<uint32_t>(obc_data::TM_TEMP_0) << index;
        }
    }
}

bool AllTemperaturesAvailable(
    const obc_data::TELEMETRY_DATA& telemetry
) {
    return (telemetry.available_fields & temperature_fields) == temperature_fields;
}

} // namespace obc_housekeeping
