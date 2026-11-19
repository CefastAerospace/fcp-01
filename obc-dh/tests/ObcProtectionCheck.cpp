/** @file ObcProtectionCheck.cpp
 * @brief Fronteiras temporais e de medição da política EPS/Térmico.
 */
#include "ObcProtection.h" // Avaliador portátil utilizado no perfil ESP32.
#include "ObcModes.h" // Pausa enquanto se aguarda parada ADCS.
#include <cassert>
#include <cstdio>
#include <limits>

namespace {
    using namespace obc_protection;
    constexpr TEMPERATURE_LIMITS thermal = {10, 40, 15, 35};
    constexpr POLICY policy = {6, 7, {thermal, thermal, thermal, thermal, {5, 45, 14, 36}}, 3000, 3000, 2000};

    // Gera uma rodada saudável e atual nas unidades dos respectivos canais.
    READINGS Healthy(
        uint64_t at_ms
    ) {
        READINGS readings = {};
        readings.battery = {true, 7, at_ms};
        for (auto& temperature : readings.temperatures) temperature = {true, 25, at_ms};
        return readings;
    }

    // A carência vale só para falta da primeira rodada e termina no limite exato.
    void CheckStartup() {
        Evaluator missing(policy, 10);
        auto result = missing.Evaluate(3009, {});
        assert(!result.starts_allowed && !result.protection_active && result.unavailable == 0x3f);
        result = missing.Evaluate(3010, {});
        assert(result.protection_active && !result.recovery_allowed);

        Evaluator early(policy, 10);
        READINGS partial = {};
        partial.battery = {true, 6, 10};
        result = early.Evaluate(10, partial);
        assert(result.protection_active && result.below_limit == 1 && result.unavailable == 0x3e);

        Evaluator after_first(policy, 10);
        assert(after_first.Evaluate(30, Healthy(30)).starts_allowed);
        auto readings = Healthy(31);
        readings.temperatures[2].available = false;
        result = after_first.Evaluate(31, readings);
        assert(result.protection_active && result.unavailable == 0x08);
    }

    // Confirma extremos inclusivos, mapa por canal e a agregação de causas.
    void CheckLimits() {
        Evaluator evaluator(policy, 0);
        auto readings = Healthy(3000);
        readings.battery.value = 6;
        readings.temperatures[0].value = 10;
        readings.temperatures[1].value = 40;
        readings.temperatures[4].value = 40;
        readings.temperatures[2].value = std::numeric_limits<float>::quiet_NaN();
        readings.temperatures[3].at_ms = 3001;
        auto result = evaluator.Evaluate(3000, readings);
        assert(result.protection_active && result.below_limit == 0x03 && result.above_limit == 0x04);
        assert(result.unavailable == 0x18 && !result.starts_allowed && !result.recovery_allowed);

        Evaluator stale(policy, 0);
        assert(stale.Evaluate(3000, Healthy(0)).starts_allowed);
        result = stale.Evaluate(3001, Healthy(0));
        assert(result.protection_active && result.unavailable == 0x3f);
    }

    // A recuperação depende da faixa mais estreita e da renovação de todos os canais.
    void CheckRecovery() {
        Evaluator evaluator(policy, 0);
        auto readings = Healthy(1000);
        readings.temperatures[0].value = 40;
        assert(evaluator.Evaluate(1000, readings).protection_active);
        readings = Healthy(2000);
        readings.battery.value = 6.5f;
        assert(evaluator.Evaluate(2000, readings).protection_active);
        assert(evaluator.Evaluate(3000, Healthy(3000)).protection_active);
        readings = Healthy(4000);
        readings.temperatures[0].value = 36;
        assert(evaluator.Evaluate(4000, readings).protection_active);

        assert(evaluator.Evaluate(5000, Healthy(5000)).protection_active);
        readings = Healthy(7000);
        readings.temperatures[4].at_ms = 6999;
        assert(!evaluator.Evaluate(7000, readings).recovery_allowed);
        readings = Healthy(7001);
        readings.temperatures[0].value = 15;
        readings.temperatures[1].value = 35;
        const auto recovered = evaluator.Evaluate(7001, readings);
        assert(!recovered.protection_active && recovered.recovery_allowed && recovered.starts_allowed);
        readings.temperatures[0].value = 39;
        assert(!evaluator.Evaluate(7002, readings).recovery_allowed);
    }

    // A pausa deve sobreviver à normalização das leituras enquanto o ADCS ainda não parou.
    void CheckPendingStop() {
        using namespace obc_modes;
        Coordinator modes(true);
        SUBSYSTEM_STATUS status = {true, true, false, false, false, false};
        const MISSION_STATUS mission = {};
        const ACTIONS actions = {[] { return true; }, [] { return false; }, [](const EVENT&) { return true; }};
        assert(modes.Execute(COMMAND::START_ADCS, 0, status, mission, actions));
        status.protection_active = true;
        modes.Service(1, status, mission, actions);
        assert(modes.GetMode() == obc_data::MODE::DETUMBLING && modes.IsProtectionInEffect());
        status.protection_active = false;
        status.recovery_allowed = true;
        modes.Service(2001, status, mission, actions);
        assert(modes.IsProtectionInEffect() && !modes.Execute(COMMAND::EXIT_SAFE, 2001, status, mission, actions));
        status.adcs_stopped = true;
        modes.Service(2002, status, mission, actions);
        assert(modes.GetMode() == obc_data::MODE::LOW_POWER && modes.IsProtectionInEffect());
        assert(modes.Execute(COMMAND::EXIT_SAFE, 2002, status, mission, actions));
        assert(modes.GetMode() == obc_data::MODE::SAFE && !modes.IsProtectionInEffect());
    }
} // namespace

int main() {
    CheckStartup();
    CheckLimits();
    CheckRecovery();
    CheckPendingStop();
    std::puts("ObcProtectionCheck passed");
}
