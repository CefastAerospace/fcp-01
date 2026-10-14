/**
 * @file Ttc.h
 * @brief Enlace de rádio entre o OBC e a estação terrena.
 *
 * @details Subida: valida TCs e os entrega ao OBC, que executa e responde
 * com SendAck(). Descida: transmite ACKs, telemetria e mensagens de dados
 * (ex.: JSON do Payload) sob demanda, assim que o rádio estiver livre.
 * Start() cria a task do enlace (DP §7.2: Core 0, prioridade alta), única
 * a acessar o rádio; os demais métodos podem ser chamados de qualquer task.
 */

// Include guard
#pragma once

#include <Arduino.h>           // para Serial
#include <freertos/FreeRTOS.h> // para portMUX_TYPE, TickType_t
#include <freertos/queue.h>    // para QueueHandle_t, xQueueSend, xQueueReceive
#include <freertos/semphr.h>   // para SemaphoreHandle_t, xSemaphoreTake/Give
#include <freertos/task.h>     // para TaskHandle_t, xTaskCreatePinnedToCore, xTaskNotifyGive
#include "LoraRadio.h"         // para LoraRadio
#include "TtcProtocol.h"       // para quadros e constantes do protocolo

// ─── STRUCTS ──────────────────────────────────────────────────────

/**
 * @brief Estatísticas do enlace.
 */
struct LINK_STATS {
    int16_t  last_rssi; // dBm.
    float    last_snr;  // dB.
    uint32_t rx_packets_count;
    uint32_t tx_packets_count;
    uint32_t rx_errors_count;
    uint32_t tx_errors_count;
};

/**
 * @brief Quadro na fila de descida.
 */
struct TX_FRAME {
    uint8_t data[ttc::max_frame_len];
    uint8_t len;
};

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

constexpr BaseType_t  ttc_task_core     = 0;  // DP §7.1: TT&C isolado no Core 0.
constexpr UBaseType_t ttc_task_priority = 10; // Acima das tasks de aplicação.

// ─── CLASSES ──────────────────────────────────────────────────────

class Ttc {
public:
    explicit Ttc(
        LoraRadio& radio
    );

    /**
     * @brief Cria as filas e inicializa o rádio.
     *
     * @return true se tudo está pronto.
     */
    bool Init();

    /**
     * @brief Cria a task do enlace, que executa Update() continuamente.
     *
     * @details A task dorme até um evento do rádio (DIO0), um quadro novo
     * na fila de descida ou o fim do intervalo de verificação.
     *
     * @return true se a task foi criada.
     */
    bool Start(
        BaseType_t core = ttc_task_core,
        UBaseType_t priority = ttc_task_priority
    );

    /**
     * @brief Recebe um quadro e transmite no máximo um quadro de descida.
     *
     * @details Um quadro por chamada mantém a recepção atendida entre
     * transmissões. ACKs têm prioridade sobre telemetria e dados. TC com
     * CRC inválido gera NACK ERR_CRC automaticamente. Só deve ser chamada
     * diretamente quando Start() não for usada.
     *
     * @return true se ainda há quadros esperando transmissão.
     */
    bool Update();

    /**
     * @brief Suspende ou retoma o início de novas transmissões.
     *
     * @details Usado pelo OBC para acionar o burn-wire fora de TX (DP §5.2).
     * A recepção continua ativa durante a suspensão.
     *
     * @param hold true suspende; false retoma.
     * @param wait_ms Espera máxima pelo fim de uma transmissão em curso.
     *
     * @return true se nenhuma transmissão está em curso ao retornar.
     */
    bool HoldTransmit(
        bool hold,
        uint32_t wait_ms
    );

    bool IsTransmitting();

    /**
     * @brief Retira o próximo TC válido.
     *
     * @param wait Ticks de espera (0 = não bloqueia).
     *
     * @return true se um TC foi entregue.
     */
    bool ReceiveTelecommand(
        ttc::TELECOMMAND_PACKET& out,
        TickType_t wait
    );

    /**
     * @return true se o ACK foi enfileirado.
     */
    bool SendAck(
        uint16_t sequence_id,
        uint8_t command_id,
        ttc::ACK_STATUS status
    );

    /**
     * @brief Enfileira a telemetria; header, sequence_id, packet_type e
     * checksum são preenchidos aqui.
     *
     * @return true se enfileirada.
     */
    bool SendTelemetry(
        const ttc::TELEMETRY_PACKET& tm
    );

    /**
     * @brief Enfileira uma mensagem, fragmentada em quadros de dados.
     *
     * @details Tudo ou nada: a mensagem só é aceita se todos os
     * fragmentos couberem na fila.
     *
     * @return true se todos os fragmentos foram enfileirados.
     */
    bool SendData(
        const uint8_t* data,
        size_t len
    );

    LINK_STATS GetLinkStats();
    bool IsHealthy();

private:
    LoraRadio& radio;
    QueueHandle_t rx_queue = nullptr;
    QueueHandle_t ack_queue = nullptr; // Descida prioritária.
    QueueHandle_t tx_queue = nullptr;
    TaskHandle_t task_handle = nullptr;
    volatile bool hold_tx = false;
    volatile bool transmitting = false;
    SemaphoreHandle_t tx_mutex = nullptr; // Serializa quem enfileira descida.
    LINK_STATS stats = {};
    portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;
    uint16_t downlink_sequence = 0; // Protegido por tx_mutex.
    uint16_t data_message_id = 0;   // Protegido por tx_mutex.

    /**
     * @brief Laço da task do enlace.
     */
    static void TaskEntry(
        void* parameters
    );

    void PollReceive();

    /**
     * @brief Transmite o próximo quadro, ACKs primeiro.
     */
    void TransmitNext();

    bool HasPendingTransmit();

    /**
     * @brief Acorda a task do enlace, se existir.
     */
    void Notify();

    /**
     * @brief Grava o CRC16 little-endian nos 2 últimos bytes do quadro.
     */
    void SealFrame(
        uint8_t* frame,
        size_t len
    );

    /**
     * @brief Copia o quadro para a fila indicada. Exige tx_mutex.
     */
    bool EnqueueFrame(
        QueueHandle_t queue,
        const uint8_t* frame,
        size_t len
    );

    bool LockTransmit();
};
