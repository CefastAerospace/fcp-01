/**
 * @file ObcDataFlowEsp32.h
 * @brief UART2 e escritor SD isolados do atendimento TT&C no ESP32.
 */
#pragma once

#include "ObcDataFlow.h" // Estado, fila e diário portáteis.
#include "ObcDelivery.h" // Cache e recuperação de entregas com identificação estável.
#include "ObcDownlink.h" // Caixa limitada de request_log e resposta por cópia.
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

    /** @brief Retira cópia para transporte integrado pelo TT&C; timeout é escolhido pelo chamador. */
    bool BeginDeliveryTransmit(
        uint64_t timeout_ms,
        obc_delivery::DELIVERY& out
    );

    /** @brief Informa conclusão local de transmissão identificada. */
    bool FinishDeliveryTransmit(
        obc_delivery::ID id,
        bool success
    );

    /** @brief Recebe confirmação identificada do solo e notifica o proprietário do SD. */
    bool ConfirmDelivery(
        obc_delivery::ID id
    );

    /**
     * @brief Solicita I/O ao escritor, sem bloquear o atendimento de TC.
     * @return OK se reservado, BUSY se ocupado, NOT_INITIALIZED sem escritor.
     */
    obc_storage::STATUS RequestLog(
        uint16_t sequence,
        const ttc::LOG_REQUEST& request
    );

    /** @brief Retira resposta ou expiração; length zero pede lg busy fora do lock. */
    bool TakeLogResponse(
        obc_downlink::LOG_RESPONSE& out
    );

#ifdef OBC_FLOW_BENCH
#if defined(OBC_CAPACITY_BENCH) || defined(OBC_MEMORY_BENCH)
    /** @brief Observação pelo proprietário do receptor, após publicação e fora do lock. */
    void ObserveLimitsPayload(
        obc_payload::STATUS status,
        const obc_data::PAYLOAD_SNAPSHOT* snapshot,
        const obc_delivery::STATISTICS& statistics
    );
    /** @brief Cópia de diagnóstico; libera a última entrada após esvaziamento do cache. */
    void ObserveLimitsDeliveries(
        const obc_delivery::STATISTICS& statistics
    );
#endif
    /** @brief Escritor lento simulado; não confirma persistência física. */
    JOURNAL_IO GetBenchIO(
    );
#ifdef OBC_DELIVERY_BENCH
    /** @brief Arquivos fechados simulados para o scanner de produção. */
    obc_delivery::READER_IO GetBenchReaderIO(
    );
#endif
#endif

} // namespace esp32
} // namespace obc_flow
