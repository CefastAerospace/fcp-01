/**
 * @file ObcDataFlowEsp32.h
 * @brief UART2 e escritor SD isolados do atendimento TT&C no ESP32.
 */
#pragma once

#include "ObcDataFlow.h" // Estado, fila e diário portáteis.
#include "ObcDelivery.h" // Cache e recuperação de entregas com identificação estável.
#include "ObcDownlink.h" // Caixa limitada de request_log e resposta por cópia.
#include "ObcStartup.h" // Relatório já produzido pelo boot central.
#include "ObcTaskHealth.h" // Avaliação portátil e eventos diagnósticos das tarefas.
#include "ObcI2cHealth.h" // Diagnóstico portátil de progresso e disponibilidade I2C.
#include "ObcResourceHealth.h" // Avaliação portátil de TT&C, heap e pilhas.
#include "ObcWatchdog.h" // Códigos de eventos da supervisão com ação de reset.
#include "ObcModes.h" // Prontidão da missão e eventos de transição.
#include "Ttc.h" // Cópia do diagnóstico publicado pelo proprietário do enlace.
#ifdef OBC_RESOURCES_BENCH
#include "ObcResourcesBench.h" // Injeções exclusivas da bancada de recursos.
#endif
#ifdef OBC_HOUSEKEEPING_BENCH
#include "ObcHousekeepingBench.h" // Cópias EPS/Térmico e captura exclusivas da INT-009.
#endif
#include "I2cBus.h" // Cópia coerente dos diagnósticos do executor existente.
#include "PayloadReceiverEsp32.h" // Proprietário único de UART2 e Receiver.
#include "ObcStorageEsp32.h" // Operações existentes sobre SD/HSPI.
#include <esp_system.h> // esp_reset_reason.
#include <freertos/task.h> // Tarefas e notificações, sem segunda fila TT&C.
#include <esp_heap_caps.h> // Métricas de RAM interna com acesso por byte.

namespace obc_flow {
namespace esp32 {

#ifdef OBC_HOUSEKEEPING_BENCH
    /**
     * @brief Reserva os dois buffers do receptor antes dos serviços, sem UART ou tarefas.
     * @details Uso sequencial no setup da bancada, após reservar boot válido e antes de Init.
     * Init reutiliza o receptor e conserva a admissão das pilhas/margem. O estágio
     * SD é reservado pelo tamanho real somente durante ENCODING, fora do boot.
     */
    bool ReserveHousekeepingBuffers(
        uint64_t boot_id
    );
#endif

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

    /** @brief Amostra fluxo, último snapshot, referência UTC e estado do escritor. */
    obc_modes::MISSION_STATUS GetMissionStatus(
        uint64_t max_snapshot_age_ms,
        uint64_t& sampled_ms
    );

    /** @brief Enfileira EVENT/FAULT v1 de origem OBC e retorna admissão local. */
    bool RecordObcEvent(
        uint16_t code,
        bool fault,
        uint64_t at_ms,
        const char* description
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

    /** @brief Enfileira resultado FCP4 no diário atual, sem esperar pelo SD. */
    void RecordAcquisition(
        obc_record::ACQUISITION_KIND kind,
        uint64_t completed_ms,
        const obc_record::ACQUISITION_DATA& data
    );

    /** @brief Avalia saúde a cada 100 ms; conserva diagnósticos periódicos de 1 s. */
    void ReportDiagnostics(
    );

    /** @brief Consome cópia TT&C a cada 100 ms e avalia memória a cada segundo. */
    void ReportResourceHealth(
        const LINK_DIAGNOSTICS& ttc
    );

    /** @brief Copia progresso UART/processamento/escritor sem aninhar locks. */
    void GetTaskProgress(
        obc_health::PROGRESS (&out)[obc_health::task_count]
    );

    /** @brief Tenta enfileirar evento 265/266; sem boot não fabrica identificação. */
    void RecordWatchdogEvent(
        obc_watchdog::CODE code,
        bool fault,
        const char* description
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
#ifdef OBC_I2C_HEALTH_BENCH
    /** @brief Inicia o cliente de cenários I2C apenas na bancada, após set_time. */
    void StartI2cHealthBench(
    );
#endif
#ifdef OBC_TASK_HEALTH_BENCH
    /** @brief Inicia uma vez as injeções da bancada após set_time. */
    void StartHealthBench(
    );
    /** @brief Consulta do proprietário SD, sem executar I/O ou esperar. */
    bool HealthBenchScanRequested(
    );
    /** @brief Varredura simulada longa com progresso por leitura/listagem. */
    void RunHealthBenchScan(
        const obc_delivery::READER_IO& reader
    );
    /** @brief Uma falha de escrita exclusiva para exercitar a espera real de recuperação. */
    bool FailHealthBenchWrite(
    );
    /** @brief Conclui a bancada após o escritor confirmar retomada normal. */
    void ObserveHealthBenchRecovery(
        uint64_t recovered
    );
#endif
    /** @brief Ciclo simulado somente na nova bancada; histórico fornece callbacks nulos. */
    RECOVERY_IO GetBenchRecoveryIO(
    );
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
