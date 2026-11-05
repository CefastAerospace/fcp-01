#include "ObcStartup.h" // Funções de inicialização e relatório portátil.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcStartup.h".
 */

namespace obc_startup {

    // ─── HELPERS ──────────────────────────────────────────────────────

    STARTUP_REPORT Initialize(
        const INITIALIZERS& initializers
    ) {
        STARTUP_REPORT report = {0, obc_time::STATUS::INVALID_ARGUMENT, false,
            {obc_storage::STATUS::INVALID_ARGUMENT, 0, 0, 0}};
        if (initializers.start_boot == nullptr || initializers.init_i2c == nullptr
                || initializers.init_storage == nullptr) return report;

        // Falhas são independentes: não há retorno antecipado entre serviços.
        report.time_status = initializers.start_boot(report.boot_id);
        if (report.time_status != obc_time::STATUS::OK) report.boot_id = 0;
        report.i2c_ready = initializers.init_i2c();
        report.storage_result = initializers.init_storage();
        return report;
    }

} // namespace obc_startup
