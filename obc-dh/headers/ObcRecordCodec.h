/**
 * @file ObcRecordCodec.h
 * @brief Codec portátil de registros v1, snapshots v2 e TM v3, sem I/O ou alocação.
 */

#pragma once

#include "models/ObcData.h" // Corpos, cabeçalho, layout e validadores existentes.

namespace obc_record {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr char snapshot_magic[4] = {'F', 'C', 'P', '2'};
    constexpr uint8_t snapshot_version = 2;
    constexpr uint8_t snapshot_type = 8;
    constexpr char telemetry_magic[4] = {'F', 'C', 'P', '3'};
    constexpr uint8_t telemetry_version = 3;
    constexpr uint8_t telemetry_type = 9;
    constexpr size_t telemetry_record_bytes = 99;
    constexpr size_t min_snapshot_record_bytes = obc_data::header_bytes + 8 + 1 + obc_data::crc_bytes;
    constexpr size_t max_snapshot_record_bytes = min_snapshot_record_bytes
        + obc_data::max_snapshot_aircraft * (3 + 1 + 3 * 8
            + 1 + obc_data::identification_chars + 1 + obc_data::tick_chars + 1
            + obc_data::max_aircraft_extras * (2 + obc_data::extra_key_chars + obc_data::extra_value_chars));
    static_assert(max_snapshot_record_bytes <= UINT16_MAX,
        "Snapshot persistido deve caber no comprimento de 16 bits");

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Corpos v1; a API dedicada v3 também usa o membro telemetry. */
    union RECORD_BODY {
        obc_data::PAYLOAD_DATA mission;
        obc_data::TELEMETRY_DATA telemetry;
        obc_data::EVENT_DATA event;
        obc_data::STATISTICS_DATA statistics;
        obc_data::BOOT_DATA boot;
        obc_time::TIME_REFERENCE time_reference;
    };

    /**
     * @brief Registro em RAM, não serializável por cópia de memória.
     * @details O chamador deve ativar o membro correspondente ao tipo ao
     * construir um registro. Decode publica cabeçalho e corpo juntos.
     */
    struct RECORD {
        obc_data::RECORD_HEADER header;
        RECORD_BODY body;
    };

    /** @brief Falhas distintas; contagem de bytes é zero em qualquer erro. */
    enum class STATUS {
        OK,
        INVALID_ARGUMENT,
        BUFFER_TOO_SMALL,
        INCOMPLETE,
        BAD_MAGIC,
        UNSUPPORTED_VERSION,
        UNSUPPORTED_TYPE,
        INVALID_LENGTH,
        CRC_MISMATCH,
        INVALID_DATA
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Calcula CRC-32/ISO-HDLC, usando os parâmetros de ObcData.h.
     * @param data Bytes; pode ser nullptr somente quando size é zero.
     * @param size Quantidade de bytes cobertos, sem o próprio CRC.
     * @return CRC final; entrada vazia produz zero.
     */
    uint32_t ComputeCrc32(
        const uint8_t* data,
        size_t size
    );

    /** @brief Continua CRC bruto por blocos; iniciar em crc32_initial e aplicar xor final. */
    uint32_t ContinueCrc32(
        uint32_t crc,
        const uint8_t* data,
        size_t size
    );

    /**
     * @brief Valida e codifica um registro completo em buffer do chamador.
     * @details Erros são detectados antes da primeira escrita. Fonte e buffer
     * não devem se sobrepor. Não modifica o registro original; campos ausentes
     * e padding textual saem zerados. Usa memcpy apenas entre reais e seus
     * padrões inteiros de bits, nunca para serializar estruturas.
     * @param record Registro com corpo ativo coerente com o cabeçalho.
     * @param buffer Destino não nulo.
     * @param capacity Capacidade em bytes.
     * @param written Bytes produzidos; zero em falha.
     * @return Estado final; buffer preservado em falha.
     */
    STATUS Encode(
        const RECORD& record,
        uint8_t* buffer,
        size_t capacity,
        size_t& written
    );

    /**
     * @brief Decodifica o primeiro registro, sem consumir bytes posteriores.
     * @details Exige 24 bytes para interpretar o cabeçalho. Valida comprimento
     * canônico, disponibilidade, CRC e conteúdo, antes de publicar a saída.
     * Campos indisponíveis são normalizados para zero, conforme a máscara;
     * terminadores NUL são acrescentados na RAM. Não corrige corrupção nem
     * procura o próximo registro automaticamente.
     * @param buffer Origem não nula.
     * @param available Bytes disponíveis, incluindo possíveis registros seguintes.
     * @param record Saída; preservada em qualquer falha.
     * @param consumed Bytes do registro validado; zero em falha.
     * @return Estado final; INCOMPLETE indica necessidade de mais bytes.
     */
    STATUS Decode(
        const uint8_t* buffer,
        size_t available,
        RECORD& record,
        size_t& consumed
    );

    /**
     * @brief Valida o modelo e calcula o comprimento exato do snapshot v2.
     * @param snapshot Modelo existente, com campos opcionais e extras textuais.
     * @param total_bytes Saída; zero em falha, inclui cabeçalho/corpo/CRC.
     * @return OK ou estado mapeado dos validadores existentes.
     */
    STATUS GetSnapshotSize(
        const obc_data::PAYLOAD_SNAPSHOT& snapshot,
        size_t& total_bytes
    );

    /**
     * @brief Codifica um snapshot completo no buffer do chamador, sem cópia total.
     * @details Formato variável de 37–64497 bytes nos limites atuais. Campos
     * ausentes não são gravados; strings têm comprimento u8 e bytes sem NUL.
     * Modelo e capacidade são verificados antes de modificar o destino. Fonte
     * estável durante a operação, sem sobreposição com o destino. Não alocar
     * modelo ou buffer máximo na pilha padrão das tarefas FreeRTOS.
     * @param snapshot Modelo original, preservado.
     * @param buffer Destino não nulo e pertencente ao chamador.
     * @param capacity Capacidade do destino.
     * @param written Bytes produzidos; zero em qualquer falha.
     * @return Estado; buffer permanece inalterado em falha.
     */
    STATUS EncodeSnapshot(
        const obc_data::PAYLOAD_SNAPSHOT& snapshot,
        uint8_t* buffer,
        size_t capacity,
        size_t& written
    );

    /**
     * @brief Mesmo registro v2 em dois blocos distintos, sem cópia intermediária.
     * @details Capacidades somadas devem comportar o registro. Blocos e modelo
     * não se sobrepõem. Erro preserva ambos os blocos e zera written. O segundo
     * bloco pode ser nulo quando sua capacidade é zero.
     */
    STATUS EncodeSnapshotParts(
        const obc_data::PAYLOAD_SNAPSHOT& snapshot,
        uint8_t* first,
        size_t first_capacity,
        uint8_t* second,
        size_t second_capacity,
        size_t& written
    );

    /**
     * @brief Decodifica snapshot v2 completo, preservando saída em falha.
     * @details Verifica limites/CRC e faz uma passagem de validação antes de
     * publicar. Espaço temporário limitado a uma aeronave e seus identificadores;
     * não cria outro PAYLOAD_SNAPSHOT. Segunda passagem normaliza ausências e
     * slots inativos para zero. Entrada estável e sem sobreposição com a saída.
     * Strings com NUL dentro do comprimento declarado são rejeitadas, pois o
     * modelo C++ não representa bytes significativos depois desse terminador.
     * @param buffer Origem não nula.
     * @param available Bytes disponíveis, inclusive registros posteriores.
     * @param snapshot Saída do chamador, válida somente em sucesso.
     * @param consumed Comprimento consumido; zero em qualquer falha.
     * @return Estado, incluindo INCOMPLETE para prefixos truncados.
     */
    STATUS DecodeSnapshot(
        const uint8_t* buffer,
        size_t available,
        obc_data::PAYLOAD_SNAPSHOT& snapshot,
        size_t& consumed
    );

    /**
     * @brief Codifica TM v3 com os bits 0–18 de disponibilidade e modo obrigatório.
     * @details Reutiliza os valores de TELEMETRY_DATA, sem ampliar o validador v1.
     * Fonte/destino não se sobrepõem; falha preserva destino e zera written.
     */
    STATUS EncodeTelemetryV3(
        const obc_time::EVENT_STAMP& stamp,
        const obc_data::TELEMETRY_DATA& telemetry,
        uint8_t* buffer,
        size_t capacity,
        size_t& written
    );

    /**
     * @brief Decodifica TM v3 integralmente validada, sem modificar saída em falha.
     * @details RECORD conserva version=3/type=9 do arquivo; seu corpo ativo é
     * telemetry. Encode/Decode v1 continuam recusando esse cabeçalho.
     */
    STATUS DecodeTelemetryV3(
        const uint8_t* buffer,
        size_t available,
        RECORD& record,
        size_t& consumed
    );

} // namespace obc_record
