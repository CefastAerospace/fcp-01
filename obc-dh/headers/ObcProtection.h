/** @file ObcProtection.h
 * @brief Avaliação portátil de proteção EPS/Térmico no boot atual.
 */
#pragma once

#include "models/ObcData.h" // Quantidade dos canais térmicos do OBC.
#include <cmath> // std::isfinite para rejeitar valores não numéricos.

namespace obc_protection {

    constexpr uint16_t event_code = 268;

    /** @brief Amostra na unidade do canal, com instante monotônico do produtor. */
    struct SAMPLE {
        bool available;
        float value;
        uint64_t at_ms;
    };

    /** @brief Tensão em V e temperaturas em °C, nas posições do mapa térmico. */
    struct READINGS {
        SAMPLE battery;
        SAMPLE temperatures[obc_data::thermal_sensors];
    };

    /** @brief Extremos de disparo e faixa de retorno, com fronteiras inclusivas. */
    struct TEMPERATURE_LIMITS {
        float trip_low;
        float trip_high;
        float resume_low;
        float resume_high;
    };

    /** @brief Política da configuração ativa, sem valores de voo implícitos. */
    struct POLICY {
        float battery_trip_v;
        float battery_resume_v;
        TEMPERATURE_LIMITS temperatures[obc_data::thermal_sensors];
        uint64_t max_age_ms;
        uint64_t startup_grace_ms;
        uint64_t recovery_dwell_ms;
    };

    /** @brief Saída da política e causas atuais. Bit 0 bateria, bits 1–5 T0–T4. */
    struct RESULT {
        bool starts_allowed;
        bool protection_active;
        bool recovery_allowed;
        uint8_t unavailable;
        uint8_t below_limit;
        uint8_t above_limit;
    };

    /** @brief Avaliador sequencial, com carência e estabilização por boot. */
    class Evaluator {
    public:
        /** @brief Recebe política do perfil e instante de referência do boot. */
        Evaluator(
            const POLICY& policy,
            uint64_t boot_ms
        );

        /** @brief Avalia todos os canais na mesma consulta, sem I/O. */
        RESULT Evaluate(
            uint64_t now_ms,
            const READINGS& readings
        );

    private:
        POLICY policy_;
        uint64_t boot_ms_;
        uint64_t recovery_since_ms_ = 0;
        bool has_complete_readings_ = false;
        bool protection_active_ = false;
        bool in_return_band_ = false;
    };
} // namespace obc_protection
