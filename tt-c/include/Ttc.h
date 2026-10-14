/**
 * @file Ttc.h
 * @brief Enlace de rádio entre o OBC e a estação terrena.
 *
 * @details Subida: valida o envelope JSON dos TCs e os entrega ao OBC, que
 * executa e responde com SendAck(). Descida: transmite ACKs, telemetria e
 * demais pacotes JSON sob demanda, assim que o rádio estiver livre. O
 * contador "n" de cada pacote de descida é inserido no momento do TX, em
 * ordem estrita no ar. Start() cria a task do enlace (DP §7.2: Core 0,
 * prioridade alta), única a acessar o rádio; os demais métodos podem ser
 * chamados de qualquer task.
 */

// Include guard
#pragma once

#include <Arduino.h>           // para Serial, millis
#include <freertos/FreeRTOS.h> // para portMUX_TYPE, TickType_t
#include <freertos/queue.h>    // para QueueHandle_t, xQueueSend, xQueueReceive
#include <freertos/semphr.h>   // para SemaphoreHandle_t, xSemaphoreTake/Give
#include <freertos/task.h>     // para TaskHandle_t, xTaskCreatePinnedToCore, xTaskNotifyGive
#include "LoraRadio.h"         // para LoraRadio
#include "TtcProtocol.h"       // para o contrato JSON e constantes do protocolo

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
    uint32_t duplicates_count; // TCs repetidos não reentregues ao OBC.
};

/**
 * @brief TC entregue ao OBC.
 *
 * @details O OBC interpreta os argumentos ("args") a partir de json.
 */
struct TC_FRAME {
    uint16_t        seq;
    ttc::COMMAND_ID cmd;
    uint32_t        rx_ms; // millis() da recepção.
    uint8_t         len;
    char            json[ttc::max_frame_len + 1];
};

/**
 * @brief Pacote na fila de descida, ainda sem o contador "n".
 */
struct TX_FRAME {
    uint8_t len;
    char    json[ttc::max_frame_len + 1];
};

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

constexpr BaseType_t  ttc_task_core     = 0;  // DP §7.1: TT&C isolado no Core 0.
constexpr UBaseType_t ttc_task_priority = 10; // Acima das tasks de aplicação.
constexpr size_t      ttc_dedup_slots   = 8;  // TCs lembrados para deduplicação.

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
     * @details A task dorme até um evento do rádio (DIO0), um pacote novo
     * na fila de descida ou o fim do intervalo de verificação.
     *
     * @return true se a task foi criada.
     */
    bool Start(
        BaseType_t core = ttc_task_core,
        UBaseType_t priority = ttc_task_priority
    );

    /**
     * @brief Recebe um quadro e transmite no máximo um pacote de descida.
     *
     * @details Um pacote por chamada mantém a recepção atendida entre
     * transmissões. ACKs têm prioridade sobre os demais pacotes. Só deve
     * ser chamada diretamente quando Start() não for usada.
     *
     * @return true se ainda há pacotes esperando transmissão.
     */
    bool Update();

    /**
     * @brief Retira o próximo TC válido e registra o instante da entrega.
     *
     * @param wait Ticks de espera (0 = não bloqueia).
     *
     * @return true se um TC foi entregue.
     */
    bool ReceiveTelecommand(
        TC_FRAME& out,
        TickType_t wait
    );

    /**
     * @brief Responde a um TC entregue por ReceiveTelecommand().
     *
     * @details Inclui no ACK o atraso entre a recepção e a entrega ao OBC
     * (campo "q", REQ-TTC-004) e guarda o ACK para reenviá-lo se o mesmo
     * TC chegar de novo.
     *
     * @return true se o ACK foi enfileirado.
     */
    bool SendAck(
        uint16_t seq,
        ttc::COMMAND_ID cmd,
        ttc::ACK_STATUS status
    );

    /**
     * @brief Enfileira um pacote JSON de descida (TM, aeronave, evento).
     *
     * @param json Objeto JSON de até ttc::max_body_len bytes, sem "n".
     *
     * @return true se enfileirado.
     */
    bool Send(
        const char* json,
        size_t len
    );

    /**
     * @brief Completa os campos de enlace, serializa e enfileira a TM.
     *
     * @return true se enfileirada.
     */
    bool SendTelemetry(
        const ttc::TELEMETRY_DATA& tm
    );

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
    LINK_STATS GetLinkStats();
    bool IsHealthy();

private:
    /**
     * @brief TC recente, para deduplicação e para o campo "q" do ACK.
     */
    struct DEDUP_SLOT {
        bool     used;
        bool     acked;
        uint16_t seq;
        uint32_t rx_ms;
        uint32_t delivered_ms;
        uint8_t  ack_len;
        char     ack[ttc::max_ack_len];
    };

    LoraRadio& radio;
    QueueHandle_t rx_queue = nullptr;
    QueueHandle_t ack_queue = nullptr; // Descida prioritária.
    QueueHandle_t tx_queue = nullptr;
    TaskHandle_t task_handle = nullptr;
    SemaphoreHandle_t dedup_mutex = nullptr;
    DEDUP_SLOT dedup[ttc_dedup_slots] = {};
    size_t dedup_next = 0;
    LINK_STATS stats = {};
    portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;
    uint16_t downlink_counter = 0; // Só a task do enlace usa.
    volatile bool hold_tx = false;
    volatile bool transmitting = false;

    /**
     * @brief Laço da task do enlace.
     */
    static void TaskEntry(
        void* parameters
    );

    void PollReceive();

    /**
     * @brief Trata um TC com envelope válido: reenvia o ACK de um repetido
     * ou entrega ao OBC.
     */
    void HandleTelecommand(
        const ttc::TC_INFO& info,
        const char* json,
        size_t len
    );

    /**
     * @brief Procura um TC recente. Exige dedup_mutex.
     *
     * @return Índice do slot; -1 se não existe ou expirou.
     */
    int FindDedupSlot(
        uint16_t seq
    );

    /**
     * @brief Transmite o próximo pacote, ACKs primeiro, com o contador "n".
     */
    void TransmitNext();

    bool HasPendingTransmit();

    /**
     * @brief Monta e enfileira um ACK/NACK na fila prioritária.
     */
    bool EnqueueAck(
        uint16_t seq,
        const char* cmd_name,
        ttc::ACK_STATUS status,
        int32_t queue_ms
    );

    /**
     * @brief Copia o pacote para a fila indicada e acorda a task.
     */
    bool EnqueueFrame(
        QueueHandle_t queue,
        const char* json,
        size_t len
    );

    /**
     * @brief Acorda a task do enlace, se existir.
     */
    void Notify();

    void CountRxError();
};
