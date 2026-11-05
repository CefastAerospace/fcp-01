/**
 * @file ObcStartupCheck.cpp
 * @brief Verifica ordem e independência do boot de bancada, sem hardware.
 * @details Executar em obc-dh/:
 * g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined -Iheaders src/ObcStartup.cpp tests/ObcStartupCheck.cpp -o /tmp/fcp01-startup-check
 * /tmp/fcp01-startup-check
 */

#include "ObcStartup.h" // Coordenador real e resultados dos serviços.
#include <assert.h> // assert.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcStartup.h".
 */

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

static unsigned scenario = 0;
static unsigned next_service = 0;

// ─── HELPERS ──────────────────────────────────────────────────────

// Primeiro serviço; mesmo uma função que altere a saída em falha não deve
// publicar seu ID inválido. O coordenador precisa limpar essa identificação.
static obc_time::STATUS StartBoot(
    uint64_t& boot_id
) {
    assert(next_service++ == 0);
    boot_id = UINT64_MAX;
    return (scenario & 1U) ? obc_time::STATUS::STORAGE_ERROR : obc_time::STATUS::OK;
}

// Segundo serviço; confirmação limitada ao controlador/executor I2C.
static bool InitI2c(
) {
    assert(next_service++ == 1);
    return (scenario & 2U) == 0;
}

// Terceiro serviço; preserva detalhes do resultado mesmo quando há falha.
static obc_storage::RESULT InitStorage(
) {
    assert(next_service++ == 2);
    return (scenario & 4U)
        ? obc_storage::RESULT{obc_storage::STATUS::MOUNT_FAILED, 7, EIO, 2}
        : obc_storage::RESULT{obc_storage::STATUS::OK, 0, 0, 0};
}

// Exercita as oito combinações, exigindo todas as chamadas exatamente uma vez.
static void CheckIndependentResults(
) {
    const obc_startup::INITIALIZERS initializers = {StartBoot, InitI2c, InitStorage};
    for (scenario = 0; scenario < 8; ++scenario) {
        next_service = 0;
        const obc_startup::STARTUP_REPORT report = obc_startup::Initialize(initializers);
        assert(next_service == 3);
        assert(report.time_status == ((scenario & 1U)
            ? obc_time::STATUS::STORAGE_ERROR : obc_time::STATUS::OK));
        assert(report.boot_id == ((scenario & 1U) ? 0 : UINT64_MAX));
        assert(report.i2c_ready == ((scenario & 2U) == 0));
        assert(report.storage_result.status == ((scenario & 4U)
            ? obc_storage::STATUS::MOUNT_FAILED : obc_storage::STATUS::OK));
        assert(report.storage_result.transferred == ((scenario & 4U) ? 7U : 0U));
        assert(report.storage_result.system_error == ((scenario & 4U) ? EIO : 0));
        assert(report.storage_result.attempts == ((scenario & 4U) ? 2 : 0));
    }
}

// Erro de composição da tabela não chama ponteiros nulos ou serviços parciais.
static void CheckMissingInitializers(
) {
    for (unsigned missing = 0; missing < 3; ++missing) {
        obc_startup::INITIALIZERS initializers = {StartBoot, InitI2c, InitStorage};
        if (missing == 0) initializers.start_boot = nullptr;
        if (missing == 1) initializers.init_i2c = nullptr;
        if (missing == 2) initializers.init_storage = nullptr;
        next_service = 0;
        const obc_startup::STARTUP_REPORT report = obc_startup::Initialize(initializers);
        assert(next_service == 0);
        assert(report.boot_id == 0 && !report.i2c_ready);
        assert(report.time_status == obc_time::STATUS::INVALID_ARGUMENT);
        assert(report.storage_result.status == obc_storage::STATUS::INVALID_ARGUMENT);
        assert(report.storage_result.transferred == 0 && report.storage_result.attempts == 0);
        assert(report.storage_result.system_error == 0);
    }
}

int main(
) {
    CheckIndependentResults();
    CheckMissingInitializers();
    std::printf("Boot de bancada: ordem, oito combinacoes e falhas independentes aprovadas.\n");
    return 0;
}
