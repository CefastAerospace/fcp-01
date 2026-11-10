#include <Arduino.h> // Serial, setup/loop e delay do framework.
#include "ObcStartup.h" // Coordenação e relatório dos serviços de bancada.
#include "ObcTimeEsp32.h" // Reserva de boot em NVS e timer monotônico.
#include "I2cBus.h" // Inicialização do controlador e executor existentes.
#include "ObcStorageEsp32.h" // Montagem SD/HSPI, sem abrir arquivos.
#include "ObcTtc.h" // Enlace TT&C e atendimento dos comandos de bancada.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "ObcStartup.h".
 */

namespace {

    // ─── HELPERS ──────────────────────────────────────────────────────

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
    Serial.println("[OBC] Boot de bancada: tempo/NVS, I2C e SD");
    const obc_startup::INITIALIZERS initializers = {
        obc_time::esp32::StartBoot,
        i2c::Init,
        obc_storage::esp32::Init
    };
    const obc_startup::STARTUP_REPORT report = obc_startup::Initialize(initializers);
    LogStartup(report);
    obc_ttc::Init();
    obc_flow::esp32::Init(report);
}

/**
 * @brief Atende telecomandos de bancada sem iniciar fluxo de missão.
 */
void loop(
) {
    obc_ttc::Service();
    obc_flow::esp32::ReportDiagnostics();
}
