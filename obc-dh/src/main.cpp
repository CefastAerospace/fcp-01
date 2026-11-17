#include <Arduino.h> // Serial, setup/loop e delay do framework.
#include "ObcStartup.h" // Coordenação e relatório dos serviços de bancada.
#include "ObcTimeEsp32.h" // Reserva de boot em NVS e timer monotônico.
#include "I2cBus.h" // Inicialização do controlador e executor existentes.
#include "ObcStorageEsp32.h" // Montagem SD/HSPI, sem abrir arquivos.
#include "ObcTtc.h" // Enlace TT&C e atendimento dos comandos de bancada.
#ifdef OBC_EPS_BENCH
#include "ObcEpsEsp32.h" // Aquisição isolada, sem publicar energia na TM operacional.
#endif
#ifdef OBC_THERMAL_BENCH
#include "ObcThermalEsp32.h" // Bancada LM75A isolada, sem TM ou proteção térmica.
#endif

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcStartup.h".
 */

namespace {

    // ─── HELPERS ──────────────────────────────────────────────────────

    // Checkpoints ocorrem somente depois do retorno; chamada presa não alimenta.
    obc_time::STATUS StartBoot(
        uint64_t& boot_id
    ) {
        const auto result = obc_time::esp32::StartBoot(boot_id);
        obc_watchdog::esp32::Checkpoint();
        return result;
    }

    bool InitI2c(
    ) {
        const bool result = i2c::Init();
        obc_watchdog::esp32::Checkpoint();
        return result;
    }

    obc_storage::RESULT InitStorage(
    ) {
        const auto result = obc_storage::esp32::Init();
        obc_watchdog::esp32::Checkpoint();
        return result;
    }

    // Emite diagnóstico uma vez, em linhas que o contrato umbilical permite
    // ignorar. Não publica TC/TM ou confunde tempo relativo com horário UTC.
    void LogStartup(
        const obc_startup::STARTUP_REPORT& report
    ) {
        if (report.time_status == obc_time::STATUS::OK) {
            Serial.printf("[OBC] Tempo/NVS: OK; boot_id=%llu\n",
                static_cast<unsigned long long>(report.boot_id));
        } else {
            Serial.printf("[OBC] Tempo/NVS: falha; status=%u; boot_id=indisponivel\n",
                static_cast<unsigned>(report.time_status));
        }
        Serial.printf("[OBC] I2C: %s; estado do controlador/executor, sem verificar sensores\n",
            report.i2c_ready ? "OK" : "falha");
        Serial.printf("[OBC] SD: %s; status=%u; errno=%d; bytes=%lu; tentativas=%u\n",
            report.storage_result.status == obc_storage::STATUS::OK ? "montado" : "indisponivel",
            static_cast<unsigned>(report.storage_result.status), report.storage_result.system_error,
            static_cast<unsigned long>(report.storage_result.transferred),
            static_cast<unsigned>(report.storage_result.attempts));
        Serial.printf("[OBC] Tempo monotonic_ms=%llu; UTC=desconhecido\n",
            static_cast<unsigned long long>(obc_time::esp32::GetMonotonicMs()));
        Serial.println("[OBC] Inicializacao de bancada encerrada; sem repeticao automatica");
    }

} // namespace

/**
 * @brief Inicializa os serviços de bancada e registra seus resultados por USB.
 */
void setup(
) {
    // Não aguarda conexão do computador nem inicia missão ou atuadores.
    Serial.begin(115200);
    obc_watchdog::esp32::Init();
    Serial.println("[OBC] Boot de bancada: tempo/NVS, I2C e SD");
    const obc_startup::INITIALIZERS initializers = {
        StartBoot,
        InitI2c,
        InitStorage
    };
    const obc_startup::STARTUP_REPORT report = obc_startup::Initialize(initializers);
    LogStartup(report);
    obc_ttc::Init();
    obc_watchdog::esp32::Checkpoint();
    obc_flow::esp32::Init(report);
    obc_watchdog::esp32::Checkpoint();
#ifdef OBC_EPS_BENCH
    Serial.printf("[EPS] init=%s; parametros provisorios de bancada\n",
        obc_eps::esp32::Init() ? "iniciada" : "indisponivel");
    obc_watchdog::esp32::Checkpoint();
#endif
#ifdef OBC_THERMAL_BENCH
    Serial.printf("[THERMAL] init=%s; NXP LM75A e parametros provisorios de bancada\n",
        obc_thermal::esp32::Init() ? "iniciada" : "indisponivel");
    obc_watchdog::esp32::Checkpoint();
#endif
    obc_watchdog::esp32::StartRuntime();
}

/**
 * @brief Atende telecomandos de bancada sem iniciar fluxo de missão.
 */
void loop(
) {
    obc_ttc::Service();
    obc_flow::esp32::ReportDiagnostics();
#ifdef OBC_EPS_BENCH
    obc_eps::esp32::ReportDiagnostics();
#endif
#ifdef OBC_THERMAL_BENCH
    obc_thermal::esp32::ReportDiagnostics();
#endif
    obc_watchdog::esp32::Service(obc_ttc::GetDiagnostics());
}
