/**
 * @file Ttc.h
 * @brief Enlace de rádio entre o OBC e a estação terrena.
 *
 * @details Subida: valida o envelope JSON dos TCs recebidos pelo rádio ou
 * pelo umbilical e os entrega ao OBC, que executa e responde com SendAck();
 * o ACK volta pelo meio de origem do TC. Descida: transmite ACKs,
 * telemetria e demais pacotes JSON sob demanda, assim que o rádio estiver
 * livre, também pelo umbilical quando habilitado. O contador "n" de cada
 * pacote de descida é inserido no momento do TX, com uma sequência estrita
 * por meio. Start() cria a task do enlace (DP §7.2: Core 0,
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
#include "LinkTransport.h"     // para LinkTransport
#include "LoraRadio.h"         // para LoraRadio
#include "TtcProtocol.h"       // para o contrato JSON e constantes do protocolo

// ─── STRUCTS ──────────────────────────────────────────────────────

/**
 * @brief Meio do enlace; combinável como máscara de destino.
 */
enum LINK_ORIGIN : uint8_t {
    LINK_RF    = 0x01,
    LINK_WIRED = 0x02
};

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
    uint32_t wired_rx_packets_count; // Parcela de rx_packets_count vinda do umbilical.
    uint32_t wired_tx_packets_count;
    uint32_t radio_resets_count;     // Reinicializações pela verificação de saúde.
    uint32_t last_tc_ms;             // millis() do último TC válido; 0 = nenhum (REQ-OBC-011).
};

/**
 * @brief Gera o JSON do beacon (ex.: com ttc::BuildBeacon).
 *
 * @return Bytes escritos; 0 para pular este período.
 */
typedef size_t (*BEACON_BUILDER)(
    char* out,
    size_t capacity
);

/**
 * @brief TC entregue ao OBC.
 *
 * @details O OBC interpreta os argumentos ("args") a partir de json.
 */
struct TC_FRAME {
    uint16_t        seq;
    ttc::COMMAND_ID cmd;
    uint32_t        rx_ms; // millis() da recepção.
    LINK_ORIGIN     origin;
    uint8_t         len;
    char            json[ttc::max_frame_len + 1];
};

/**
 * @brief Pacote na fila de descida, ainda sem o contador "n".
 */
struct TX_FRAME {
    uint8_t dest; // Máscara de LINK_ORIGIN.
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
    /**
     * @param umbilical Meio cabeado opcional (UmbilicalLink na UART0).
     */
    explicit Ttc(
        LoraRadio& radio,
        LinkTransport* umbilical = nullptr
    );

    /**
     * @brief Cria as filas e inicializa o rádio e o umbilical.
     *
     * @return true se as filas e o rádio estão prontos; falha do umbilical
     * só é registrada no log.
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

    /**
     * @brief Liga ou desliga a cópia da descida no umbilical.
     *
     * @details ACKs de TCs recebidos pelo cabo seguem por ele mesmo
     * desligado.
     */
    void SetUmbilicalEnabled(
        bool enabled
    );

    /**
     * @brief Liga o beacon periódico (DP §7.2), só para safe-mode.
     *
     * @details A política nominal é sob demanda; o beacon fica desligado
     * por padrão.
     *
     * @param period_ms Intervalo; 0 desliga.
     * @param builder Gera o conteúdo a cada período, na task do enlace.
     */
    void EnableBeacon(
        uint32_t period_ms,
        BEACON_BUILDER builder
    );

    bool IsTransmitting();
    LINK_STATS GetLinkStats();

    /**
     * @return true se as filas existem e o rádio passou na última
     * verificação de saúde.
     */
    bool IsHealthy();

private:
    /**
     * @brief TC recente, para deduplicação e para o campo "q" do ACK.
     */
    struct DEDUP_SLOT {
        bool        used;
        bool        acked;
        LINK_ORIGIN origin;
        uint16_t seq;
        uint32_t rx_ms;
        uint32_t delivered_ms;
        uint8_t  ack_len;
        char     ack[ttc::max_ack_len];
    };

    LoraRadio& radio;
    LinkTransport* umbilical;
    volatile bool umbilical_enabled = true;
    QueueHandle_t rx_queue = nullptr;
    QueueHandle_t ack_queue = nullptr; // Descida prioritária.
    QueueHandle_t tx_queue = nullptr;
    TaskHandle_t task_handle = nullptr;
    SemaphoreHandle_t dedup_mutex = nullptr;
    DEDUP_SLOT dedup[ttc_dedup_slots] = {};
    size_t dedup_next = 0;
    LINK_STATS stats = {};
    portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;
    uint16_t rf_counter = 0;    // Só a task do enlace usa.
    uint16_t wired_counter = 0; // Só a task do enlace usa.
    volatile bool hold_tx = false;
    volatile bool transmitting = false;
    volatile bool radio_healthy = false;
    uint32_t last_health_ms = 0;  // Só a task do enlace usa.
    volatile uint32_t beacon_period_ms = 0;
    volatile BEACON_BUILDER beacon_builder = nullptr;
    uint32_t last_beacon_ms = 0;  // Só a task do enlace usa.

    /**
     * @brief Laço da task do enlace.
     */
    static void TaskEntry(
        void* parameters
    );

    /**
     * @brief Verificação periódica do rádio e beacon, entre transmissões.
     */
    void MaintainLink();

    /**
     * @brief Reinicializa o rádio que falhou na verificação de saúde.
     */
    void CheckRadio();

    /**
     * @brief Enfileira o beacon quando o período vence.
     */
    void SendBeaconIfDue();

    /**
     * @brief Lê um quadro do meio e trata o TC, se houver.
     */
    void PollTransport(
        LinkTransport& transport,
        LINK_ORIGIN origin
    );

    /**
     * @brief Trata um TC com envelope válido: reenvia o ACK de um repetido
     * ou entrega ao OBC.
     */
    void HandleTelecommand(
        const ttc::TC_INFO& info,
        const char* json,
        size_t len,
        LINK_ORIGIN origin
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
     * @brief Transmite o próximo pacote, ACKs primeiro, em cada meio de destino.
     */
    void TransmitNext();

    /**
     * @brief Insere o contador do meio e transmite uma cópia do pacote.
     *
     * @return true se transmitido.
     */
    bool TransmitTo(
        LinkTransport& transport,
        uint16_t& counter,
        const TX_FRAME& frame
    );

    /**
     * @return Máscara de destino da descida comum (TM, aeronave, evento).
     */
    uint8_t DownlinkDestinations();

    bool HasPendingTransmit();

    /**
     * @brief Monta e enfileira um ACK/NACK na fila prioritária.
     */
    bool EnqueueAck(
        uint16_t seq,
        const char* cmd_name,
        ttc::ACK_STATUS status,
        int32_t queue_ms,
        uint8_t dest
    );

    /**
     * @brief Copia o pacote para a fila indicada e acorda a task.
     */
    bool EnqueueFrame(
        QueueHandle_t queue,
        const char* json,
        size_t len,
        uint8_t dest
    );

    /**
     * @brief Acorda a task do enlace, se existir.
     */
    void Notify();

    void CountRxError();
};
