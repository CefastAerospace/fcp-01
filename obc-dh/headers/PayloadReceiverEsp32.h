/**
 * @file PayloadReceiverEsp32.h
 * @brief Adaptador sequencial UART2 para o receptor portátil do Payload.
 */
#pragma once

#include "PayloadReceiver.h" // Receiver, RESULT e STATUS portáteis.
#include "Pinout.h" // GPIO16/RX e GPIO17/TX existentes.
#include "ObcTimeEsp32.h" // GetMonotonicMs do boot corrente.
#include <Arduino.h> // Serial2 e HardwareSerial do framework.
#include <freertos/task.h> // Tarefa opcional proprietária do transporte UART2.
#ifdef OBC_FLOW_BENCH
#include <atomic> // Acionamento da fonte de bancada entre tarefas.
#endif

namespace obc_payload {
namespace esp32 {

    // ─── HELPERS ──────────────────────────────────────────────────────

    constexpr uint32_t transport_stack_bytes = 3072;

    /** @brief Observações do atendimento físico, separadas do processamento FCPU. */
    struct TRANSPORT_STATISTICS {
        uint64_t maximum_gap_ms;
        uint64_t full_observations;
        size_t stack_free_bytes;
    };

    /**
     * @brief Inicia UART2 em 115200 8N1 com RX 4 KiB e TX 256 bytes.
     * @details Reserva de boot pertence ao chamador. Receiver deve ter vida
     * estática ou equivalente, fora da pilha. Não inicializar concorrentemente
     * nem acessar Serial2 por outro caminho. Rejeita segunda inicialização.
     * Não inicia tarefa, missão, NVS ou qualquer consumidor automaticamente.
     * @param receiver Armazenamento estável, fornecido pela integração.
     * @param boot_id Boot confirmado e não nulo.
     * @return true quando UART e receptor estão prontos.
     */
    bool Init(
        Receiver& receiver,
        uint64_t boot_id
    );

    /**
     * @brief Transfere a propriedade de Serial2 para tarefa Core 1/prioridade 3.
     * @details Chamar após Init e antes do consumidor. Usa ring fixo de 1 KiB;
     * Poll continua pertencendo somente à tarefa de processamento. Sem esta
     * chamada, Init/Poll preservam o atendimento sequencial das bancadas antigas.
     * Não iniciar concorrente com Poll; falha conserva o modo sequencial.
     */
    bool StartTransportTask(
    );

    /** @brief Copia métricas do transporte, sem modificar receptor/snapshot. */
    TRANSPORT_STATISTICS GetTransportStatistics(
    );

#ifdef OBC_FLOW_BENCH
    /** @brief Fonte simulada somente na bancada; entrega os mesmos bytes ao ring real. */
    size_t ReadBenchInput(
        uint8_t* bytes,
        size_t capacity,
        uint64_t now_ms
    );
    /** @brief Libera uma única execução dos vetores após set_time válido. */
    void StartBenchInput(
    );
#endif

    /**
     * @brief Atende até 256 bytes e um evento por chamada, sem esperar quadro.
     * @details No modo sequencial, chamar ao menos a cada 10 ms. Com tarefa
     * de transporte iniciada, só essa tarefa acessa Serial2 e Poll consome
     * o ring. Consumir READY antes da próxima
     * chamada. Resposta parcial fica retida; não descarta bytes já lidos nem
     * espera espaço de TX. O parse de um quadro completo ocorre na chamada;
     * não executar no atendimento FOC ou ISR. Mesma tarefa acessa o snapshot.
     * @return Resultado do núcleo ou WAITING durante envio da resposta.
     */
    STATUS Poll(
    );

} // namespace esp32
} // namespace obc_payload
