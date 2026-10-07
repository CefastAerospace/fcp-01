/**
 * @file Ttc.h
 * @brief Enlace de rádio entre o OBC e a estação terrena.
 *
 * @details Subida: valida TCs e os entrega ao OBC, que executa e responde
 * com SendAck(). Descida: transmite ACKs, telemetria e mensagens de dados
 * (ex.: JSON do Payload). Update() deve rodar sempre na mesma task; os
 * demais métodos podem ser chamados de qualquer task.
 */

// Include guard
#pragma once

#include <Arduino.h>           // para Serial
#include <freertos/FreeRTOS.h> // para portMUX_TYPE, TickType_t
#include <freertos/queue.h>    // para QueueHandle_t, xQueueSend, xQueueReceive
#include <freertos/semphr.h>   // para SemaphoreHandle_t, xSemaphoreTake/Give
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
     * @brief Recebe um quadro e transmite a fila de descida.
     *
     * @details TC com CRC inválido gera NACK ERR_CRC automaticamente.
     */
    void Update();

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
    QueueHandle_t tx_queue = nullptr;
    SemaphoreHandle_t tx_mutex = nullptr; // Serializa quem enfileira descida.
    LINK_STATS stats = {};
    portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;
    uint16_t downlink_sequence = 0; // Protegido por tx_mutex.
    uint16_t data_message_id = 0;   // Protegido por tx_mutex.

    void PollReceive();
    void FlushTransmit();

    /**
     * @brief Grava o CRC16 little-endian nos 2 últimos bytes do quadro.
     */
    void SealFrame(
        uint8_t* frame,
        size_t len
    );

    /**
     * @brief Copia o quadro para a fila de descida. Exige tx_mutex.
     */
    bool EnqueueFrame(
        const uint8_t* frame,
        size_t len
    );

    bool LockTransmit();
};
