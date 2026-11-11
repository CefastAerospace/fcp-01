#include "I2cRecovery.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "I2cRecovery.h".
 */

namespace i2c {
namespace {

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Diferença modular válida para o orçamento de 100 ms, inclusive no wrap.
    uint32_t Elapsed(
        const RECOVERY_IO& io,
        uint32_t started_us
    ) { return io.now_us(io.context) - started_us; }

    // Toda espera explícita cabe no orçamento restante.
    bool Pause(
        const RECOVERY_IO& io,
        uint32_t started_us,
        uint32_t duration_us
    ) {
        const uint32_t elapsed = Elapsed(io, started_us);
        if (elapsed >= recovery_budget_us) return false;
        const uint32_t remaining = recovery_budget_us - elapsed;
        io.delay_us(io.context, duration_us < remaining ? duration_us : remaining);
        return Elapsed(io, started_us) < recovery_budget_us;
    }

    // Espera liberação elétrica sem forçar nível alto, limitada por linha e ciclo.
    bool WaitHigh(
        const RECOVERY_IO& io,
        uint32_t started_us,
        bool (*read_line)(void*)
    ) {
        const uint32_t waiting_us = io.now_us(io.context);
        while (!read_line(io.context)) {
            if (io.now_us(io.context) - waiting_us >= line_release_timeout_us
                    || !Pause(io, started_us, recovery_phase_us)) return false;
        }
        return Elapsed(io, started_us) < recovery_budget_us;
    }

    // Controlador desativado: clock manual e STOP pertencem ao mesmo executor.
    bool ClearLines(
        const RECOVERY_IO& io,
        uint32_t started_us,
        RECOVERY_RESULT& result
    ) {
        io.prepare(io.context);
        result.failure = RECOVERY_CAUSE::SCL_LOW;
        if (!WaitHigh(io, started_us, io.read_scl)) return false;
        for (uint8_t pulse = 0; pulse < recovery_clock_pulses && !io.read_sda(io.context); ++pulse) {
            io.set_scl(io.context, false);
            if (!Pause(io, started_us, recovery_phase_us)) return false;
            io.set_scl(io.context, true);
            ++result.pulses;
            if (!WaitHigh(io, started_us, io.read_scl)
                    || !Pause(io, started_us, recovery_phase_us)) return false;
        }
        result.failure = RECOVERY_CAUSE::SDA_LOW;
        if (!WaitHigh(io, started_us, io.read_sda)) return false;

        // STOP: SDA sobe enquanto SCL está liberado e alto.
        io.set_scl(io.context, false);
        io.set_sda(io.context, false);
        if (!Pause(io, started_us, recovery_phase_us)) return false;
        io.set_scl(io.context, true);
        result.failure = RECOVERY_CAUSE::SCL_LOW;
        if (!WaitHigh(io, started_us, io.read_scl)
                || !Pause(io, started_us, recovery_phase_us)) return false;
        io.set_sda(io.context, true);
        result.failure = RECOVERY_CAUSE::SDA_LOW;
        return WaitHigh(io, started_us, io.read_sda);
    }

} // namespace

// ─── HELPERS ──────────────────────────────────────────────────────

RECOVERY_CAUSE GetRecoveryCause(
    OPERATION_RESULT result,
    bool sda_high,
    bool scl_high
) {
    if (!scl_high) return RECOVERY_CAUSE::SCL_LOW;
    if (!sda_high) return RECOVERY_CAUSE::SDA_LOW;
    if (result == OPERATION_RESULT::TRANSFER_TIMEOUT) return RECOVERY_CAUSE::TRANSFER_TIMEOUT;
    if (result == OPERATION_RESULT::BUS_ERROR) return RECOVERY_CAUSE::BUS_ERROR;
    return RECOVERY_CAUSE::NONE;
}

RECOVERY_RESULT RecoverBus(
    const RECOVERY_IO& io
) {
    RECOVERY_RESULT result = {};
    const uint32_t started_us = io.now_us(io.context);
    for (uint8_t attempt = 0; attempt < recovery_attempts; ++attempt) {
        if (Elapsed(io, started_us) >= recovery_budget_us) break;
        if (attempt && !Pause(io, started_us, recovery_interval_us)) break;
        ++result.attempts;
        result.failure = RECOVERY_CAUSE::DEINIT_FAILED;
        if (!io.stop(io.context)) continue;
        // Após stop, GPIOs permanecem liberados mesmo se acabar o orçamento.
        if (Elapsed(io, started_us) >= recovery_budget_us) break;
        const bool cleared = ClearLines(io, started_us, result);
        io.set_sda(io.context, true);
        io.set_scl(io.context, true);
        if (!cleared || Elapsed(io, started_us) >= recovery_budget_us) continue;
        result.failure = RECOVERY_CAUSE::INIT_FAILED;
        if (!io.start(io.context)) continue;
        result.sda_high = io.read_sda(io.context);
        result.scl_high = io.read_scl(io.context);
        if (Elapsed(io, started_us) >= recovery_budget_us) break;
        if (result.sda_high && result.scl_high) {
            result.recovered = true;
            result.failure = RECOVERY_CAUSE::NONE;
            break;
        }
        result.failure = result.scl_high ? RECOVERY_CAUSE::SDA_LOW : RECOVERY_CAUSE::SCL_LOW;
    }
    result.elapsed_us = Elapsed(io, started_us);
    if (result.elapsed_us >= recovery_budget_us) {
        result.recovered = false;
        result.failure = RECOVERY_CAUSE::BUDGET_EXHAUSTED;
    }
    result.sda_high = io.read_sda(io.context);
    result.scl_high = io.read_scl(io.context);
    return result;
}

} // namespace i2c
