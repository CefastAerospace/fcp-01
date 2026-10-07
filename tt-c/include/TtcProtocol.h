/**
 * @file TtcProtocol.h
 * @brief Contrato do enlace LoRa entre satélite e estação terrena.
 *
 * @details Compartilhado com a estação terrena (ground-station/); os dois
 * lados precisam ser gravados com a mesma versão.
 */

// Include guard
#pragma once

#include <stddef.h> // para size_t
#include <stdint.h> // para uint8_t, uint16_t, uint32_t, int16_t

namespace ttc {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr long     radio_frequency_hz  = 921E6;
    constexpr int      radio_spreading     = 9;
    constexpr long     radio_bandwidth_hz  = 125E3;
    constexpr int      radio_coding_rate   = 6; // 4/6.
    constexpr int      radio_tx_power_dbm  = 17;
    constexpr int      radio_sync_word     = 0x12;
    constexpr long     radio_preamble_len  = 32;
    constexpr uint32_t radio_tx_timeout_ms = 2000; // Airtime de 255 bytes ~1,5 s.

    constexpr size_t   max_frame_len   = 255; // FIFO do SX1276.
    constexpr uint16_t downlink_header = 0xAA55;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /**
     * @brief Opcodes de telecomando. A execução é responsabilidade do OBC.
     */
    enum COMMAND_ID : uint8_t {
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
        CMD_RESET_OBC         = 0xFF
    };

    /**
     * @brief Status transportado no ACK.
     */
    enum ACK_STATUS : uint8_t {
        ACK_OK             = 0x00,
        ERR_CRC            = 0x01,
        ERR_INVALID_CMD    = 0x02,
        ERR_INVALID_PARAM  = 0x03,
        ERR_STATE_REJECTED = 0x04,
        ERR_UNKNOWN_CMD    = 0xFF
    };

    /**
     * @brief Valor do campo packet_type dos quadros de descida.
     */
    enum PACKET_TYPE : uint8_t {
        PACKET_TELEMETRY = 0x01,
        PACKET_DATA      = 0x02
    };

    /**
     * @brief Telecomando (subida).
     */
    struct __attribute__((packed)) TELECOMMAND_PACKET {
        uint16_t sequence_id;
        uint8_t  command_id;
        uint8_t  flags;
        uint32_t timestamp;
        uint8_t  arguments[8];
        uint16_t checksum;
    };

    /**
     * @brief ACK/NACK de um telecomando (descida).
     */
    struct __attribute__((packed)) ACK_PACKET {
        uint16_t header;
        uint16_t sequence_id;
        uint8_t  command_id;
        uint8_t  status_code;
        uint16_t checksum;
    };

    /**
     * @brief Telemetria (descida).
     *
     * @details Conteúdo preenchido pelo OBC; enquadramento pelo Ttc.
     */
    struct __attribute__((packed)) TELEMETRY_PACKET {
        uint16_t header;
        uint16_t sequence_id;
        uint8_t  packet_type;
        uint8_t  system_status;
        uint32_t timestamp;     // s.
        float    vbat;          // V.
        float    ibat;          // mA.
        float    temp_obc;      // °C.
        float    rpm;
        float    target_rpm;
        int16_t  last_rssi;     // dBm.
        float    last_snr;      // dB.
        uint32_t rx_packets_count;
        uint32_t tx_packets_count;
        uint32_t rx_errors_count;
        uint16_t checksum;
    };

    /**
     * @brief Cabeçalho do quadro de dados (descida).
     *
     * @details No ar: DATA_HEADER + chunk_len bytes + CRC16. Mensagens
     * maiores que um quadro são fragmentadas sob o mesmo message_id.
     */
    struct __attribute__((packed)) DATA_HEADER {
        uint16_t header;
        uint16_t sequence_id;
        uint8_t  packet_type;
        uint16_t message_id;
        uint8_t  fragment_index;
        uint8_t  fragment_count;
        uint8_t  chunk_len;
    };

    constexpr size_t max_data_chunk = max_frame_len - sizeof(DATA_HEADER) - sizeof(uint16_t);

    static_assert(sizeof(TELECOMMAND_PACKET) == 18, "TC deve ter 18 bytes");
    static_assert(sizeof(ACK_PACKET) == 8, "ACK deve ter 8 bytes");
    static_assert(sizeof(TELEMETRY_PACKET) == 50, "TM deve ter 50 bytes");
    static_assert(sizeof(DATA_HEADER) == 10, "Cabeçalho de dados deve ter 10 bytes");

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief CRC16-CCITT (polinômio 0x1021, semente 0xFFFF).
     */
    uint16_t Crc16(
        const uint8_t* data,
        size_t len
    );

    /**
     * @brief Confere o CRC16 little-endian nos 2 últimos bytes do quadro.
     */
    bool CheckFrameCrc(
        const uint8_t* frame,
        size_t len
    );

} // namespace ttc
