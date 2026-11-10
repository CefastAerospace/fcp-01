/**
 * @file PayloadReceiver.h
 * @brief Recepção portátil FCPU v1 e publicação de snapshots completos.
 * @details Contrato em common/protocol/payload-uart.md. Não faz I/O, não
 * depende de Arduino/FreeRTOS e não altera o JSON ou os modelos existentes.
 */
#pragma once

#include "models/ObcData.h" // PAYLOAD_SNAPSHOT, AIRCRAFT_STATE e Validate.
#include "ObcRecordCodec.h" // ComputeCrc32 existente.
#include <ArduinoJson.h> // JsonDocument, JsonVariantConst e Allocator.
#include <cstddef> // size_t e max_align_t.
#include <cstdlib> // malloc, realloc, free e strtod.
#include <cerrno> // errno e ERANGE.
#include <cstring> // memcpy, memmove, memcmp e strlen.
#include <limits> // Saturação dos contadores.
#include <initializer_list> // Lista fixa de literais da gramática JSON.
#include <memory> // unique_ptr para duas reservas fixas com liberação automática.
#include <new> // std::nothrow na inicialização verificada do armazenamento.

namespace obc_payload {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr size_t header_bytes = 22;
    constexpr size_t crc_bytes = 4;
    constexpr size_t max_json_bytes = 65536;
    constexpr size_t max_frame_bytes = header_bytes + max_json_bytes + crc_bytes;
    constexpr size_t parser_memory_bytes = 96 * 1024;
    constexpr uint64_t inter_byte_timeout_ms = 250;
    constexpr uint64_t frame_timeout_ms = 8000;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Discriminantes do transporte, independentes do JSON. */
    enum class FRAME_TYPE : uint8_t { DATA = 1, ACK = 2, NACK = 3 };

    /** @brief READY é o único resultado que publica novo snapshot. */
    enum class STATUS : uint8_t {
        WAITING, READY, DUPLICATE, INVALID_FRAME, INVALID_JSON,
        INVALID_VALUE, LIMIT_EXCEEDED, NO_MEMORY, SEQUENCE_ERROR,
        TIMEOUT, NOT_INITIALIZED, INVALID_ARGUMENT
    };

    /** @brief Motivos codificados no primeiro byte do corpo de NACK. */
    enum class NACK_REASON : uint8_t {
        INVALID_JSON = 1, INVALID_VALUE = 2, LIMIT_EXCEEDED = 3,
        NO_MEMORY = 4, SEQUENCE_ERROR = 5
    };

    /** @brief Contadores saturados por Init; pico do alocador inclui overhead. */
    struct STATISTICS {
        uint64_t accepted;
        uint64_t duplicates;
        uint64_t rejected;
        uint64_t timeouts;
        uint64_t gaps;
        uint64_t session_changes;
        size_t parser_peak_bytes;
        uint64_t complete_candidates; // Inclui CRC/conteúdo inválido e repetição.
        uint64_t missing_positions; // Posições distintas; gaps conserva ocorrências.
    };

    /**
     * @brief Resultado e eventual resposta binária, sem alocação dinâmica.
     * @details Feed para no primeiro evento; chamar novamente com os bytes
     * restantes, inclusive size zero para processar candidatos retidos.
     * reply_size zero significa silêncio. ACK confirma RAM, nunca SD/solo.
     */
    struct RESULT {
        STATUS status;
        size_t consumed;
        size_t reply_size;
        uint8_t reply[header_bytes + 5 + crc_bytes];
    };

    // ─── CLASSES ──────────────────────────────────────────────────────

    /**
     * @brief Contexto estável, proprietário de um quadro e um snapshot no heap.
     * @details Uso sequencial em contexto de tarefa. Fonte não pode sobrepor
     * este objeto. Feed/CheckTimeout recebem ms não decrescentes do mesmo
     * boot. GetSnapshot permanece válido até o próximo READY ou Init.
     * Não há deduplicação durável após reset do OBC, nem sincronização entre
     * consumidores concorrentes. Reservar pilha suficiente para o parser.
     * Init reserva uma vez dois blocos separados: max_frame_bytes e um
     * PAYLOAD_SNAPSHOT. Não exige um bloco contíguo de cerca de 130 KiB.
     * Reinicializações reutilizam o armazenamento, sem realocação. O contexto
     * deve sobreviver aos consumidores; não pode ser copiado ou movido.
     */
    class Receiver {
    public:
        /** @brief Constrói contexto sem reservar memória de quadro/snapshot. */
        Receiver(
        ) = default;

        /** @brief Impede cópia/movimento que invalidaria referências do adaptador. */
        Receiver(
            const Receiver& other
        ) = delete;

        /** @brief Impede compartilhamento implícito do armazenamento exclusivo. */
        Receiver& operator=(
            const Receiver& other
        ) = delete;

        /**
         * @brief Inicia nova observação e reserva memória somente no primeiro uso.
         * @return false para boot zero ou falta de memória; não publica estado.
         */
        bool Init(
            uint64_t boot_id
        );

        /** @brief Consome bytes sem I/O; nullptr somente quando size é zero. */
        RESULT Feed(
            const uint8_t* data,
            size_t size,
            uint64_t now_ms
        );

        /** @brief Descarta quadro expirado sem modificar a lista publicada. */
        STATUS CheckTimeout(
            uint64_t now_ms
        );

        /** @brief Retorna nullptr antes do primeiro READY deste Init. */
        const obc_data::PAYLOAD_SNAPSHOT* GetSnapshot(
        ) const;

        /** @brief Retorna observações locais; não são registros duráveis. */
        const STATISTICS& GetStatistics(
        ) const;

    private:
        std::unique_ptr<uint8_t[]> frame_;
        std::unique_ptr<obc_data::PAYLOAD_SNAPSHOT> snapshot_;
        STATISTICS statistics_ = {};
        size_t received_ = 0;
        uint64_t boot_id_ = 0;
        uint64_t started_ms_ = 0;
        uint64_t last_byte_ms_ = 0;
        uint64_t last_now_ms_ = 0;
        uint64_t session_ = 0;
        uint32_t sequence_ = 0;
        uint32_t accepted_crc_ = 0;
        uint32_t missing_high_ = 0;
        bool published_ = false;

        // Procura assinatura posterior ou conserva seu prefixo final.
        void Resynchronize(
            uint64_t now_ms
        );

        // Verifica um candidato; WAITING significa quadro ainda incompleto.
        STATUS Examine(
            uint64_t now_ms,
            RESULT& result
        );

        // Valida todas as aeronaves antes de publicar em segunda passagem.
        STATUS ParseSnapshot(
            size_t length,
            uint64_t now_ms
        );

        // Gera resposta somente a partir de um quadro integral com CRC válido.
        void MakeReply(
            RESULT& result,
            uint64_t session,
            uint32_t sequence,
            STATUS status
        );
    };

} // namespace obc_payload
