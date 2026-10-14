/**
 * @file TtcProtocol.h
 * @brief Contrato do enlace entre satélite e estação terrena (JSON, DP §7.1).
 *
 * @details Cada quadro LoRa ou linha umbilical carrega um objeto JSON de até
 * max_frame_len bytes (FIFO do SX1276). A integridade fica a cargo do CRC de
 * hardware do LoRa. O esquema está documentado em
 * common/protocol/ttc-json.md. Este módulo não depende do Arduino nem do
 * FreeRTOS, para rodar nos testes nativos.
 */

// Include guard
#pragma once

#include <math.h>        // para round
#include <stddef.h>      // para size_t
#include <stdint.h>      // para uint8_t, uint16_t, uint32_t, int16_t
#include <stdio.h>       // para snprintf
#include <string.h>      // para strcmp, strncpy, memmove, memcpy
#include <ArduinoJson.h> // para JsonDocument, serializeJson, deserializeJson, measureJson

namespace ttc {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr long     radio_frequency_hz  = 921E6;
    constexpr int      radio_spreading     = 7; // DP §5.3: SF7 confirmado.
    constexpr long     radio_bandwidth_hz  = 125E3;
    constexpr int      radio_coding_rate   = 6; // 4/6.
    constexpr int      radio_tx_power_dbm  = 17;
    constexpr int      radio_sync_word     = 0x12;
    constexpr long     radio_preamble_len  = 32;
    constexpr uint32_t radio_tx_timeout_ms = 1000; // Airtime de 255 bytes ~0,45 s em SF7.

    constexpr size_t max_frame_len    = 255; // FIFO do SX1276.
    constexpr size_t max_counter_len  = 10;  // "n":65535, inserido na descida.
    constexpr size_t max_body_len     = max_frame_len - max_counter_len; // Limite dos Build*.
    constexpr size_t max_ack_len      = 96;
    constexpr size_t cmd_name_len     = 24;
    constexpr size_t thermal_sensors  = 5; // LM75A do controle térmico.

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /**
     * @brief Opcodes de telecomando. A execução é responsabilidade do OBC.
     *
     * @details No ar, o comando trafega pelo nome (CommandName()).
     */
    enum COMMAND_ID : uint8_t {
        CMD_UNKNOWN           = 0x00,
        CMD_PING              = 0x01,
        CMD_SET_MODE          = 0x02,
        CMD_START_MISSION     = 0x03,
        CMD_STOP_MISSION      = 0x04,
        CMD_GET_STATUS        = 0x05,
        CMD_ENTER_SAFE        = 0x0A,
        CMD_EXIT_SAFE         = 0x0B,
        CMD_REQUEST_TELEMETRY = 0x0C,
        CMD_SET_TIME          = 0x10,
        CMD_ADCS_START        = 0x20,
        CMD_ADCS_STOP         = 0x21,
        CMD_DEPLOY_ANTENNA    = 0x30,
        CMD_REQUEST_LOG       = 0x40,
        CMD_TEST_TM           = 0xE0, // Só bancada: rajada de TM (REQ-TTC-003).
        CMD_RESET_OBC         = 0xFF
    };

    /**
     * @brief Status transportado no ACK.
     */
    enum ACK_STATUS : uint8_t {
        ACK_OK             = 0x00,
        ERR_BAD_JSON       = 0x01,
        ERR_INVALID_CMD    = 0x02,
        ERR_INVALID_PARAM  = 0x03,
        ERR_STATE_REJECTED = 0x04,
        ERR_UNKNOWN_CMD    = 0xFF
    };

    /**
     * @brief Modos operacionais do DP §2.5, como aparecem na TM.
     */
    enum OP_MODE : uint8_t {
        MODE_BENCH,
        MODE_SAFE,
        MODE_DETUMBLING,
        MODE_SUN_POINTING,
        MODE_MISSION,
        MODE_LOW_POWER
    };

    /**
     * @brief Envelope de um TC recebido.
     *
     * @details Os argumentos ("args") ficam no JSON original e são
     * interpretados pelo OBC.
     */
    struct TC_INFO {
        bool       has_seq; // seq legível: permite responder com NACK.
        uint16_t   seq;
        COMMAND_ID cmd;
        ACK_STATUS status;  // ACK_OK se o envelope é válido.
        char       cmd_name[cmd_name_len];
    };

    /**
     * @brief Conteúdo da TM (HLR-COMM-02): status, energia, atitude, missão
     * e enlace.
     *
     * @details Contadores do enlace em 16 bits, com volta, para caber no
     * quadro no pior caso.
     */
    struct TELEMETRY_DATA {
        uint32_t timestamp;      // s (unix).
        OP_MODE  mode;
        float    vbat;           // V.
        float    ibat;           // mA.
        float    vsolar;         // V.
        float    isolar;         // mA.
        float    temps[thermal_sensors]; // °C.
        float    rate_dps;       // °/s, eixo de controle.
        float    azimuth_deg;    // °, vetor solar.
        uint16_t aircraft_count;
        uint32_t message_count;  // Mensagens ADS-B recebidas.
        int16_t  rssi;           // dBm, último TC.
        float    snr;            // dB, último TC.
        uint16_t rx_count;
        uint16_t error_count;
        uint16_t radio_resets;
    };

    /**
     * @brief Estado agregado de uma aeronave (DP §5.3).
     */
    struct AIRCRAFT_DATA {
        uint32_t timestamp;   // s (unix), última mensagem.
        char     icao[7];     // 6 dígitos hexadecimais.
        double   latitude;    // °.
        double   longitude;   // °.
        float    altitude;    // ft.
        float    velocity;    // kt.
        float    heading;     // °.
        uint16_t message_count;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    const char* CommandName(
        COMMAND_ID cmd
    );

    /**
     * @return CMD_UNKNOWN se o nome não está no catálogo.
     */
    COMMAND_ID CommandFromName(
        const char* name
    );

    const char* StatusName(
        ACK_STATUS status
    );

    const char* ModeName(
        OP_MODE mode
    );

    /**
     * @brief Valida o envelope de um TC: {"t":"tc","seq":N,"cmd":"nome",...}.
     *
     * @param json Texto recebido (não precisa terminar em '\0').
     * @param len Bytes de json.
     * @param out Envelope; out.status indica o motivo da rejeição.
     *
     * @return true se o TC pode ser entregue ao OBC.
     */
    bool ParseTelecommand(
        const char* json,
        size_t len,
        TC_INFO& out
    );

    /**
     * @brief {"t":"ack","seq":N,"cmd":"nome","st":"ok"[,"q":ms]}.
     *
     * @param queue_ms Atraso entre a recepção e a entrega ao OBC; negativo
     * omite o campo (NACK emitido pelo próprio TT&C).
     *
     * @return Bytes escritos (sem '\0'); 0 se não coube.
     */
    size_t BuildAck(
        uint16_t seq,
        const char* cmd_name,
        ACK_STATUS status,
        int32_t queue_ms,
        char* out,
        size_t capacity
    );

    /**
     * @return Bytes escritos (sem '\0'); 0 se excede max_body_len.
     */
    size_t BuildTelemetry(
        const TELEMETRY_DATA& tm,
        char* out,
        size_t capacity
    );

    /**
     * @return Bytes escritos (sem '\0'); 0 se excede max_body_len.
     */
    size_t BuildAircraft(
        const AIRCRAFT_DATA& aircraft,
        char* out,
        size_t capacity
    );

    /**
     * @brief {"t":"ev","ev":"nome"} para eventos operacionais.
     *
     * @return Bytes escritos (sem '\0'); 0 se excede max_body_len.
     */
    size_t BuildEvent(
        const char* event,
        uint32_t timestamp,
        char* out,
        size_t capacity
    );

    /**
     * @brief {"t":"bcn","ts":s,"md":"modo","vb":V} para o beacon de safe-mode.
     *
     * @return Bytes escritos (sem '\0'); 0 se excede max_body_len.
     */
    size_t BuildBeacon(
        uint32_t timestamp,
        OP_MODE mode,
        float vbat,
        char* out,
        size_t capacity
    );

    /**
     * @brief Insere "n":counter como primeiro campo do objeto.
     *
     * @param json Objeto JSON terminado em '\0'.
     * @param len Tamanho atual; atualizado na saída.
     * @param capacity Tamanho do buffer, incluindo o '\0'.
     *
     * @return false se não é um objeto ou se o resultado não cabe em
     * capacity ou em max_frame_len.
     */
    bool InsertCounter(
        char* json,
        size_t& len,
        size_t capacity,
        uint16_t counter
    );

} // namespace ttc
