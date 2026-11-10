/**
 * @file ObcData.h
 * @brief Modelos portáteis de registros v1 e snapshots do Payload.
 * @details Ordem e tamanhos persistidos estão em common/protocol/obc-data.md.
 * Não serializar memória destas estruturas. Aceite externo ainda pendente.
 */

#pragma once

#include "../ObcTime.h" // EVENT_STAMP e TIME_REFERENCE, sem duplicar tempo/boots.
#include <stddef.h> // size_t para comprimentos e índices portáteis.
#include <cmath> // std::isfinite nos campos disponíveis.
#include <cstring> // std::memchr para descrição limitada.
#include <limits> // Representação IEEE 754 verificada na compilação.

namespace obc_data {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr char record_magic[4] = {'F', 'C', 'P', '1'};
    constexpr uint8_t contract_version = 1;
    constexpr uint16_t header_bytes = 24;
    constexpr uint16_t crc_bytes = 4;
    constexpr uint16_t max_record_bytes = 512;
    constexpr uint32_t crc32_polynomial = 0xEDB88320UL;
    constexpr uint32_t crc32_initial = 0xFFFFFFFFUL;
    constexpr uint32_t crc32_final_xor = 0xFFFFFFFFUL;
    constexpr size_t timestamp_chars = 20;
    constexpr size_t description_chars = 64;
    constexpr size_t thermal_sensors = 5;
    constexpr size_t max_snapshot_aircraft = 20;
    constexpr size_t max_aircraft_extras = 32;
    constexpr size_t extra_key_chars = 32;
    constexpr size_t extra_value_chars = 64;
    constexpr size_t identification_chars = 16;
    constexpr size_t tick_chars = 40;

    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
        "Contrato v1 exige float IEEE 754 de 32 bits");
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
        "Contrato v1 exige double IEEE 754 de 64 bits");

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Discriminantes persistidos; novos layouts exigem nova versão. */
    enum class RECORD_TYPE : uint8_t {
        MISSION = 1,
        TELEMETRY = 2,
        EVENT = 3,
        FAULT = 4,
        STATISTICS = 5,
        BOOT = 6,
        TIME_REFERENCE = 7
    };

    /** @brief Resultado da validação do modelo, não do CRC ou do transporte. */
    enum class STATUS {
        OK,
        UNSUPPORTED_VERSION,
        UNSUPPORTED_TYPE,
        INVALID_LENGTH,
        INVALID_VALUE
    };

    /** @brief Bits de disponibilidade; não correspondem ao val_flags legado. */
    enum PAYLOAD_FIELD : uint8_t {
        PAYLOAD_LATITUDE = 1U << 0,
        PAYLOAD_LONGITUDE = 1U << 1,
        PAYLOAD_ALTITUDE = 1U << 2,
        PAYLOAD_VELOCITY = 1U << 3,
        PAYLOAD_HEADING = 1U << 4,
        PAYLOAD_TIMESTAMP = 1U << 5
    };

    /** @brief Disponibilidade do snapshot; não é a máscara de registros v1. */
    enum SNAPSHOT_FIELD : uint8_t {
        SNAPSHOT_LATITUDE = PAYLOAD_LATITUDE,
        SNAPSHOT_LONGITUDE = PAYLOAD_LONGITUDE,
        SNAPSHOT_ALTITUDE = PAYLOAD_ALTITUDE,
        SNAPSHOT_IDENTIFICATION = 1U << 3,
        SNAPSHOT_TICK = 1U << 4
    };

    /** @brief Disponibilidade local da TM; não muda o pacote TT&C. */
    enum TELEMETRY_FIELD : uint32_t {
        TM_TIMESTAMP = 1UL << 0,
        TM_VBAT = 1UL << 1,
        TM_IBAT = 1UL << 2,
        TM_VSOLAR = 1UL << 3,
        TM_ISOLAR = 1UL << 4,
        TM_TEMP_0 = 1UL << 5,
        TM_TEMP_1 = 1UL << 6,
        TM_TEMP_2 = 1UL << 7,
        TM_TEMP_3 = 1UL << 8,
        TM_TEMP_4 = 1UL << 9,
        TM_RATE = 1UL << 10,
        TM_AZIMUTH = 1UL << 11,
        TM_RSSI = 1UL << 12,
        TM_SNR = 1UL << 13
    };

    /** @brief Disponibilidade exclusiva da TM v3; v1 continua rejeitando esses bits. */
    enum TELEMETRY_V3_FIELD : uint32_t {
        TM_AIRCRAFT = 1UL << 14,
        TM_MESSAGES = 1UL << 15,
        TM_RX_COUNT = 1UL << 16,
        TM_ERROR_COUNT = 1UL << 17,
        TM_RADIO_RESETS = 1UL << 18
    };

    /** @brief Valores correspondentes aos modos atuais de ttc::OP_MODE. */
    enum class MODE : uint8_t {
        BENCH = 0,
        SAFE = 1,
        DETUMBLING = 2,
        SUN_POINTING = 3,
        MISSION = 4,
        LOW_POWER = 5
    };

    /** @brief Origem do evento; o significado de code pertence a essa origem. */
    enum class SOURCE : uint8_t {
        OBC = 1,
        PAYLOAD = 2,
        TTC = 3,
        ADCS = 4,
        EPS_SUBSYSTEM = 5,
        THERMAL = 6,
        SD = 7,
        I2C = 8
    };

    /** @brief FAULT aceita ERROR/CRITICAL; EVENT aceita INFO/WARNING. */
    enum class SEVERITY : uint8_t {
        INFO = 0,
        WARNING = 1,
        ERROR = 2,
        CRITICAL = 3
    };

    /**
     * @brief Metadados internos comuns; a assinatura FCP1 é implícita no modelo.
     * @details total_bytes inclui cabeçalho, corpo e CRC. stamp conserva o
     * instante de recepção validada/ocorrência no OBC, nunca o horário do Pi.
     */
    struct RECORD_HEADER {
        uint8_t version;
        RECORD_TYPE type;
        uint16_t total_bytes;
        obc_time::EVENT_STAMP stamp;
    };

    /**
     * @brief Somente campos atualizados nesta mensagem; sem cache de aeronaves.
     * @details ICAO usa apenas os 24 bits inferiores. Latitude/longitude são
     * um par disponível ou indisponível. Altitude em ft, velocidade em kt e
     * rumo em graus. Timestamp opcional do Pi: YYYY-MM-DDTHH:MM:SSZ, separado
     * do tempo OBC. Campos ausentes são ignorados pelo validador e deverão
     * ser escritos como zero pelo futuro codificador. Não inventar validade
     * para os valores zero do script legado. Sessão zero não é válida.
     */
    struct PAYLOAD_DATA {
        uint64_t session_id;
        uint32_t seq_id;
        uint32_t icao;
        uint8_t msg_type;
        uint8_t available_fields;
        double latitude;
        double longitude;
        double altitude;
        double velocity;
        double heading;
        char timestamp[timestamp_chars + 1];
    };

    /**
     * @brief Campo textual não padronizado, com chave e valor terminados em NUL.
     * @details Limites em bytes, sem contar NUL. Chave não vazia; valor vazio
     * permitido. Preserva também textos de campos conhecidos não decodificados.
     * Não truncar, nem converter outros tipos JSON silenciosamente em strings.
     */
    struct EXTRA_FIELD {
        char key[extra_key_chars + 1];
        char value[extra_value_chars + 1];
    };

    /**
     * @brief Estado acumulado de uma aeronave no snapshot, sem sessão/sequência.
     * @details ICAO de 24 bits obrigatório; posição em graus, altitude em ft.
     * Latitude/longitude são um par disponível ou indisponível. Não exige
     * dado de voo: uma aeronave pode ter somente ICAO, identificação ou extras.
     * Campos ausentes são ignorados; zero é válido quando disponível.
     * tick é texto original do produtor, não UTC validado nem tempo OBC.
     * Valida apenas limite/terminação dos textos, não UTF-8 ou calendário.
     * Extras têm chaves únicas, sensíveis a maiúsculas. Não copiar esta RAM
     * para arquivos ou quadros; o codec v1 não representa este modelo.
     */
    struct AIRCRAFT_STATE {
        uint32_t icao;
        uint8_t available_fields;
        double latitude;
        double longitude;
        double altitude;
        char identification[identification_chars + 1];
        char tick[tick_chars + 1];
        uint8_t extra_count;
        EXTRA_FIELD extras[max_aircraft_extras];
    };

    /**
     * @brief Lista completa recebida do Payload e instante validado no OBC.
     * @details id é opaco, inclusive zero; não implica sequência nem contagem.
     * stamp reutiliza o boot/monotônico do OBC. Lista vazia é válida; ICAOs
     * não podem se repetir. O futuro receptor deve publicar somente snapshots
     * completos e válidos, sem mesclar a lista anterior. Não há parser aqui.
     * Arrays fixos evitam alocação dinâmica; usar armazenamento do chamador
     * fora da pilha de tarefas FreeRTOS. Validar contagens antes de converter
     * ou preencher arrays; não truncar dados para fazê-los caber.
     */
    struct PAYLOAD_SNAPSHOT {
        uint64_t id;
        obc_time::EVENT_STAMP stamp;
        uint8_t aircraft_count;
        AIRCRAFT_STATE aircraft[max_snapshot_aircraft];
    };

    /**
     * @brief Modelo local da TM; nomes/tipos/unidades seguem TELEMETRY_DATA.
     * @details A máscara local representa indisponibilidade sem depender de
     * ArduinoJson ou alterar TT&C. Não inclui n, que pertence ao enlace.
     * Correntes são mA, não A da biblioteca EPS. A conversão pertence à
     * integração. Temperaturas são cinco posições, sem presumir montagem.
     * Contadores de enlace preservam os 16 bits com volta do contrato TT&C.
     */
    struct TELEMETRY_DATA {
        uint32_t available_fields;
        uint32_t timestamp;
        MODE mode;
        float vbat;
        float ibat;
        float vsolar;
        float isolar;
        float temps[thermal_sensors];
        float rate_dps;
        float azimuth_deg;
        uint16_t aircraft_count;
        uint32_t message_count;
        int16_t rssi;
        float snr;
        uint16_t rx_count;
        uint16_t error_count;
        uint16_t radio_resets;
    };

    /**
     * @brief Evento/falha com descrição de até 64 bytes, terminada em NUL na RAM.
     * @details code é opaco de 16 bits, definido pelo produtor com a origem.
     * A validação verifica o limite, não a codificação UTF-8 do texto.
     */
    struct EVENT_DATA {
        SOURCE source;
        uint16_t code;
        SEVERITY severity;
        char description[description_chars + 1];
    };

    /**
     * @brief Contagens cumulativas do boot OBC, amostradas sem zerar contadores.
     * @details Sem implementação de coleta. Contadores não devem voltar:
     * na integração, saturar em UINT64_MAX. Definições estão no contrato.
     */
    struct STATISTICS_DATA {
        uint64_t received;
        uint64_t invalid;
        uint64_t gaps;
        uint64_t duplicates;
        uint64_t records_written;
        uint64_t storage_failures;
        uint64_t records_dropped;
    };

    /**
     * @brief Reinicializações registradas e motivo bruto do reset.
     * @details restart_count = stamp.boot_id - 1, com as limitações da NVS
     * documentadas em ObcTime.h. reset_reason usa esp_reset_reason_t numérico;
     * zero é desconhecido. Este modelo não consulta o hardware.
     */
    struct BOOT_DATA {
        uint64_t restart_count;
        uint32_t reset_reason;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Tamanho canônico persistido, sem usar sizeof de estruturas.
     * @param type Tipo do registro.
     * @return Bytes totais; zero se o tipo não for suportado.
     */
    uint16_t GetRecordSize(
        RECORD_TYPE type
    );

    /**
     * @brief Valida versão, tipo, comprimento e identificação do boot.
     * @param header Metadados a verificar.
     * @return Resultado; não verifica bytes da assinatura nem CRC.
     */
    STATUS ValidateHeader(
        const RECORD_HEADER& header
    );

    /**
     * @brief Valida uma atualização MISSION, inclusive campos disponíveis.
     * @param header Metadados de MISSION.
     * @param data Dados da atualização.
     * @return Resultado sem modificar entradas.
     */
    STATUS Validate(
        const RECORD_HEADER& header,
        const PAYLOAD_DATA& data
    );

    /**
     * @brief Valida disponibilidade, domínios, textos e extras de uma aeronave.
     * @param data Estado acumulado, sem interpretar o JSON de origem.
     * @return OK, INVALID_LENGTH ou INVALID_VALUE; não modifica a entrada.
     */
    STATUS Validate(
        const AIRCRAFT_STATE& data
    );

    /**
     * @brief Valida boot, limite, aeronaves e unicidade dos ICAOs do snapshot.
     * @param data Snapshot completo; não representa um registro v1.
     * @return OK, INVALID_LENGTH ou INVALID_VALUE; não publica estado global.
     */
    STATUS Validate(
        const PAYLOAD_SNAPSHOT& data
    );

    /**
     * @brief Valida TM local; valores ausentes não recebem validade implícita.
     * @param header Metadados de TELEMETRY.
     * @param data TM em unidades do contrato TT&C.
     * @return Resultado sem alterar contrato ou biblioteca TT&C.
     */
    STATUS Validate(
        const RECORD_HEADER& header,
        const TELEMETRY_DATA& data
    );

    /**
     * @brief Valida origem, severidade e limite do texto de EVENT/FAULT.
     * @param header Metadados de EVENT ou FAULT.
     * @param data Descrição tipada do evento/falha.
     * @return Resultado da validação.
     */
    STATUS Validate(
        const RECORD_HEADER& header,
        const EVENT_DATA& data
    );

    /**
     * @brief Valida os metadados das estatísticas; todos os uint64_t são válidos.
     * @param header Metadados de STATISTICS.
     * @param data Amostra cumulativa dos contadores.
     * @return Resultado da validação.
     */
    STATUS Validate(
        const RECORD_HEADER& header,
        const STATISTICS_DATA& data
    );

    /**
     * @brief Valida a relação entre ID do boot e contagem registrada.
     * @param header Metadados de BOOT.
     * @param data Contagem e motivo de reset.
     * @return Resultado da validação.
     */
    STATUS Validate(
        const RECORD_HEADER& header,
        const BOOT_DATA& data
    );

    /**
     * @brief Valida referência sem duplicar ou reconstruir o horário recebido.
     * @param header Metadados de TIME_REFERENCE, no instante da recepção.
     * @param data Referência existente de ObcTime.
     * @return Resultado; stamp deve coincidir com data.reception.
     */
    STATUS Validate(
        const RECORD_HEADER& header,
        const obc_time::TIME_REFERENCE& data
    );

} // namespace obc_data
