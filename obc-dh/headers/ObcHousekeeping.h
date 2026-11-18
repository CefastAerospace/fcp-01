/**
 * @file ObcHousekeeping.h
 * @brief Composição portátil EPS/Térmico; não realiza aquisição ou decide validade temporal.
 */
#pragma once

#include "models/ObcData.h" // TELEMETRY_DATA e disponibilidade individual existente.
#include <cmath> // std::isfinite antes da publicação de valores numéricos.
#include <limits> // Faixa representável de float após conversão A para mA.

namespace obc_housekeeping {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint32_t temperature_fields = obc_data::TM_TEMP_0 | obc_data::TM_TEMP_1
        | obc_data::TM_TEMP_2 | obc_data::TM_TEMP_3 | obc_data::TM_TEMP_4;
    constexpr uint32_t telemetry_fields = temperature_fields | obc_data::TM_VBAT
        | obc_data::TM_IBAT | obc_data::TM_VSOLAR | obc_data::TM_ISOLAR;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Cópia disponível no instante da composição; corrente conserva o sinal do produtor. */
    struct ENERGY_SAMPLE {
        bool available;
        float voltage_v;
        float current_a;
    };

    /** @brief Temperatura e disponibilidade calculada pelo proprietário. */
    struct TEMPERATURE_SAMPLE {
        bool available;
        float temperature_c;
    };

    /** @brief Somente grandezas já representáveis na TM; Geral conserva seus diagnósticos próprios. */
    struct READINGS {
        ENERGY_SAMPLE battery;
        ENERGY_SAMPLE solar;
        TEMPERATURE_SAMPLE temperatures[obc_data::thermal_sensors];
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Substitui somente campos EPS/Térmico, limpando valores e indicadores anteriores.
     * @details Converte A para mA sem limitar fisicamente a leitura. Valores não finitos
     * ou não representáveis permanecem indisponíveis. Não conserva histórico próprio.
     */
    void ApplyTelemetry(
        const READINGS& readings,
        obc_data::TELEMETRY_DATA& telemetry
    );

    /** @brief O contrato TT&C admite somente disponibilidade conjunta das cinco temperaturas. */
    bool AllTemperaturesAvailable(
        const obc_data::TELEMETRY_DATA& telemetry
    );

} // namespace obc_housekeeping
