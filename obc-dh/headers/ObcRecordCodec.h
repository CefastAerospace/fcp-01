/**
 * @file ObcRecordCodec.h
 * @brief Codec portátil do contrato binário OBC v1, sem I/O ou alocação dinâmica.
 */

#pragma once

#include "models/ObcData.h" // Corpos, cabeçalho, layout e validadores existentes.

namespace obc_record {

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Corpo ativo determinado exclusivamente por RECORD_HEADER::type. */
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

} // namespace obc_record
