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
#include <stdint.h>      // para uint8_t, uint16_t, uint32_t, uint64_t, int16_t
#include <stdio.h>       // para snprintf
#include <string.h>      // para strcmp, strncpy, strlen, strstr, memmove, memcpy
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
    constexpr size_t log_name_len     = 40; // Nome de arquivo do request_log, com '\0'.
    constexpr size_t log_block_max    = 90; // Bytes por resposta "read"; pior caso cabe em max_body_len.

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
     * @brief Campos disponíveis na TM; ausência produz null, nunca omissão.
     * @details TM_TEMPS representa o conjunto das cinco temperaturas.
     * A máscara padrão conserva o comportamento dos consumidores existentes.
     */
    enum TELEMETRY_FIELD : uint32_t {
        TM_TIMESTAMP    = 1UL << 0,
        TM_MODE         = 1UL << 1,
        TM_VBAT         = 1UL << 2,
        TM_IBAT         = 1UL << 3,
        TM_VSOLAR       = 1UL << 4,
        TM_ISOLAR       = 1UL << 5,
        TM_TEMPS        = 1UL << 6,
        TM_RATE         = 1UL << 7,
        TM_AZIMUTH      = 1UL << 8,
        TM_AIRCRAFT     = 1UL << 9,
        TM_MESSAGES     = 1UL << 10,
        TM_RSSI         = 1UL << 11,
        TM_SNR          = 1UL << 12,
        TM_RX_COUNT     = 1UL << 13,
        TM_ERROR_COUNT  = 1UL << 14,
        TM_RADIO_RESETS = 1UL << 15,
        TELEMETRY_ALL   = (1UL << 16) - 1
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

    /**
     * @brief Campos disponíveis no pacote de aeronave; ausência produz null.
     *
     * @details O ICAO é sempre obrigatório. AC_POSITION cobre latitude e
     * longitude juntas. A máscara padrão conserva os consumidores existentes.
     */
    enum AIRCRAFT_FIELD : uint32_t {
        AC_TIMESTAMP = 1UL << 0,
        AC_POSITION  = 1UL << 1,
        AC_ALTITUDE  = 1UL << 2,
        AC_VELOCITY  = 1UL << 3,
        AC_HEADING   = 1UL << 4,
        AC_COUNT     = 1UL << 5,
        AIRCRAFT_ALL = (1UL << 6) - 1
    };

    /**
     * @brief Identificação estável de uma entrega do OBC (INT-006).
     *
     * @details Boot confirmado e contador do OBC, ambos maiores que zero.
     * Trafega como "id":[boot,sequence], independente do contador "n".
     */
    struct DELIVERY_ID {
        uint64_t boot;
        uint64_t sequence;
    };

    /**
     * @brief Resultado da leitura de uma confirmação de entrega do solo.
     */
    enum CONFIRMATION_PARSE : uint8_t {
        CF_NOT_CONFIRMATION, // Outro tipo de quadro (ex.: TC).
        CF_VALID,
        CF_INVALID           // "t":"cf" com ID ausente ou fora do domínio.
    };

    /**
     * @brief Operações do request_log.
     */
    enum LOG_OP : uint8_t {
        LOG_LIST,
        LOG_READ
    };

    /**
     * @brief Destino das respostas do request_log; valores iguais a LINK_ORIGIN.
     */
    enum LOG_DEST : uint8_t {
        LOG_DEST_ORIGIN = 0x00, // Mesmo meio do TC.
        LOG_DEST_RF     = 0x01,
        LOG_DEST_WIRED  = 0x02  // Umbilical USB.
    };

    /**
     * @brief Erros do request_log, transportados na resposta "lg".
     *
     * @details Argumento inválido não chega aqui: recebe NACK invalid_param.
     */
    enum LOG_ERROR : uint8_t {
        LOG_NOT_FOUND, // Arquivo ausente.
        LOG_BUSY,      // Arquivo aberto ou SD ocupado; repetir o mesmo pedido.
        LOG_RANGE,     // Offset maior que o tamanho do arquivo.
        LOG_IO,        // Falha de leitura.
        LOG_NO_MEDIA   // SD indisponível.
    };

    /**
     * @brief Argumentos de request_log já validados.
     *
     * @details LOG_LIST usa name como cursor exclusivo (vazio = início).
     * LOG_READ exige name não vazio e 1 <= length <= log_block_max.
     */
    struct LOG_REQUEST {
        LOG_OP   op;
        char     name[log_name_len];
        uint32_t offset;
        uint16_t length;
        uint8_t  dest; // LOG_DEST.
    };

    /**
     * @brief Item da listagem do request_log.
     */
    struct LOG_ENTRY {
        char     name[log_name_len];
        uint32_t size;
        bool     open; // Arquivo em escrita; ainda não pode ser lido.
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
        size_t capacity,
        uint32_t available = TELEMETRY_ALL
    );

    /**
     * @brief {"t":"ac","id":[boot,seq],"ts":s,"icao":"hex",...}.
     *
     * @param available Máscara de AIRCRAFT_FIELD; campos ausentes ficam null.
     * @param id Entrega identificada; nullptr omite "id" (sem confirmação do solo).
     *
     * @return Bytes escritos (sem '\0'); 0 se excede max_body_len, se o
     * ICAO não tem 6 hexadecimais ou se o ID tem componente zero.
     */
    size_t BuildAircraft(
        const AIRCRAFT_DATA& aircraft,
        char* out,
        size_t capacity,
        uint32_t available = AIRCRAFT_ALL,
        const DELIVERY_ID* id = nullptr
    );

    /**
     * @brief Confirmação de entrega pelo solo: {"t":"cf","id":[boot,seq]}.
     *
     * @return Bytes escritos (sem '\0'); 0 se não coube ou ID inválido.
     */
    size_t BuildConfirmation(
        const DELIVERY_ID& id,
        char* out,
        size_t capacity
    );

    /**
     * @brief Reconhece e valida uma confirmação de entrega recebida do solo.
     *
     * @param json Texto recebido (não precisa terminar em '\0').
     */
    CONFIRMATION_PARSE ParseConfirmation(
        const char* json,
        size_t len,
        DELIVERY_ID& out
    );

    const char* LogErrorName(
        LOG_ERROR error
    );

    /**
     * @brief Valida "args" de um TC request_log.
     *
     * @details list: {"op":"list"[,"after":"nome"][,"to":"rf"|"wired"]}.
     * read: {"op":"read","name":"nome","off":u32,"len":1..log_block_max[,"to":...]}.
     * Nomes usam só [A-Za-z0-9_.-], sem "..", com até log_name_len - 1
     * caracteres. Chaves desconhecidas são rejeitadas.
     *
     * @param json TC completo (não precisa terminar em '\0').
     *
     * @return ACK_OK, ERR_BAD_JSON ou ERR_INVALID_PARAM.
     */
    ACK_STATUS ParseLogRequest(
        const char* json,
        size_t len,
        LOG_REQUEST& out
    );

    /**
     * @brief {"t":"lg","rq":seq,"op":"list","ls":[["nome",bytes,0|1],...],"end":bool}.
     *
     * @details Inclui o maior prefixo de entries que cabe em max_body_len.
     * "end" só é true quando todos couberam e more é false.
     *
     * @param more Há arquivos depois de entries[count - 1].
     * @param used Saída: quantidade de entries incluída.
     *
     * @return Bytes escritos (sem '\0'); 0 se nem o envelope coube.
     */
    size_t BuildLogList(
        uint16_t request_seq,
        const LOG_ENTRY* entries,
        size_t count,
        bool more,
        char* out,
        size_t capacity,
        size_t& used
    );

    /**
     * @brief {"t":"lg","rq":seq,"op":"read","name":"nome","off":o,"tot":t,"d":"base64"}.
     *
     * @details EOF: offset igual a total e zero bytes. O offset seguinte é
     * off + bytes decodificados.
     *
     * @return Bytes escritos (sem '\0'); 0 se count > log_block_max,
     * offset + count > total ou não coube.
     */
    size_t BuildLogData(
        uint16_t request_seq,
        const char* name,
        uint32_t offset,
        uint32_t total,
        const uint8_t* bytes,
        size_t count,
        char* out,
        size_t capacity
    );

    /**
     * @brief {"t":"lg","rq":seq,"op":"list"|"read"[,"name":"nome","off":o],"err":"codigo"}.
     *
     * @param name nullptr omite name e off (listagem).
     *
     * @return Bytes escritos (sem '\0'); 0 se não coube.
     */
    size_t BuildLogError(
        uint16_t request_seq,
        LOG_OP op,
        const char* name,
        uint32_t offset,
        LOG_ERROR error,
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
