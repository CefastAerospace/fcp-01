/**
 * @file ObcDataFlowEsp32.h
 * @brief UART2 e escritor SD isolados do atendimento TT&C no ESP32.
 */
#pragma once

#include "ObcDataFlow.h" // Estado, fila e diário portáteis.
#include "ObcStartup.h" // Relatório já produzido pelo boot central.
#include "PayloadReceiverEsp32.h" // Proprietário único de UART2 e Receiver.
#include "ObcStorageEsp32.h" // Operações existentes sobre SD/HSPI.
#include <esp_system.h> // esp_reset_reason.
#include <freertos/task.h> // Tarefas e notificações, sem segunda fila TT&C.
#include <esp_heap_caps.h> // Métricas de RAM interna com acesso por byte.

namespace obc_flow {
namespace esp32 {

    /**
     * @brief Inicia uma vez o fluxo após TT&C, usando o boot já confirmado.
     * @details UART: Core 1/prioridade 3/pilha 3 KiB. Parser: Core 1/
     * prioridade 2/pilha 8 KiB. Escritor: Core 1/
     * prioridade 1/pilha 8 KiB. Falha do SD não impede recepção ou UTC em RAM.
     * @return true se a tarefa de recepção iniciou; estados SD são independentes.
     */
    bool Init(
        const obc_startup::STARTUP_REPORT& report
    );

    /** @brief Aplica UTC em RAM e tenta registrar a referência, sem esperar SD. */
    bool SetTime(
        uint32_t unix_s,
        uint32_t rx_ms,
        uint32_t delay_ms
    );

    /** @brief Copia estado pequeno e instante de composição, sem I/O ou espera. */
    obc_data::TELEMETRY_DATA ComposeTelemetry(
        uint64_t& sampled_ms
    );

    /** @brief Registra os valores usados no envio; fila cheia gera perda explícita. */
    void RecordTelemetry(
        const obc_data::TELEMETRY_DATA& telemetry,
        uint64_t sampled_ms
    );

    /** @brief Amostra estatísticas e emite diagnóstico agrupado até uma vez por segundo. */
    void ReportDiagnostics(
    );

#ifdef OBC_FLOW_BENCH
    /** @brief Escritor lento simulado; não confirma persistência física. */
    JOURNAL_IO GetBenchIO(
    );
#endif

} // namespace esp32
} // namespace obc_flow
