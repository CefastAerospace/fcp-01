/**
 * @file ObcDelivery.h
 * @brief Entregas INT-006, diário FCD1 e recuperação limitada em memória.
 */
#pragma once

#include "ObcDataFlow.h" // Journal, modelos validados e serviço de arquivos.

namespace obc_delivery {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr size_t cache_slots = 32;
    constexpr size_t data_bytes = 68;
    constexpr size_t confirm_bytes = 36;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Identificação estável, ordenada por boot confirmado e contador u64. */
    struct ID {
        uint64_t boot;
        uint64_t sequence;
    };

    /** @brief Projeção atual; velocidade, rumo, contagem e UTC individual indisponíveis. */
    struct AIRCRAFT {
        uint32_t icao;
        uint8_t available_fields;
        double latitude;
        double longitude;
        double altitude;
    };

    /** @brief Cópia de valor; nenhum consumidor recebe ponteiro para o cache. */
    struct DELIVERY {
        ID id;
        uint64_t monotonic_ms;
        AIRCRAFT aircraft;
    };

    /** @brief DATA ou CONFIRM. CONFIRM conserva apenas ID e monotonic_ms zero. */
    struct RECORD {
        bool confirmation;
        DELIVERY delivery;
    };

    /** @brief Contadores cumulativos saturados, sem redefinir estatísticas anteriores. */
    struct STATISTICS {
        uint64_t accepted;
        uint64_t repeated;
        uint64_t lost;
        uint64_t storage_failures;
        uint64_t attempts;
        uint64_t transmit_failures;
        uint64_t confirmed;
        uint64_t unknown_confirmations;
        uint64_t evicted;
        size_t occupied;
        size_t high_water;
    };

    /** @brief Diagnóstico de leitura; bytes inválidos não equivalem a entregas perdidas. */
    struct RECOVERY {
        uint64_t data;
        uint64_t confirmations;
        uint64_t corrupt_bytes;
        uint64_t incomplete_tails;
        uint64_t orphan_confirmations;
    };

    /** @brief Leitura/listagem pelo proprietário do SD; callbacks síncronos. */
    struct READER_IO {
        obc_storage::RESULT (*list)(
            void* context,
            const char* after,
            obc_storage::FILE_INFO& out
        );
        obc_storage::RESULT (*read)(
            void* context,
            const char* name,
            uint32_t offset,
            uint8_t* bytes,
            size_t capacity,
            uint32_t& total
        );
        void* context;
    };

    typedef bool (*VISITOR)(
        void* context,
        const RECORD& record
    );

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Compara identificação lexicograficamente, sem subtração com overflow. */
    int Compare(
        ID left,
        ID right
    );

    /** @brief Codificação canônica little-endian FCD1, versão 1, CRC existente. */
    bool Encode(
        const RECORD& record,
        uint8_t* bytes,
        size_t capacity,
        size_t& written
    );

    /** @brief Valida versão, comprimento, CRC, máscaras e domínio antes de publicar. */
    bool Decode(
        const uint8_t* bytes,
        size_t size,
        RECORD& out
    );

    /**
     * @brief Percorre arquivos boot_*.bin fechados com buffer de 256 bytes.
     * @details Pula registros FCP íntegros pelo CRC em blocos. Corrupção avança
     * um byte para localizar registros posteriores. Visitor false encerra cedo.
     */
    bool Scan(
        const READER_IO& io,
        VISITOR visitor,
        void* context,
        RECOVERY& diagnostic
    );

    /** @brief Diagnostica também CONFIRM sem DATA legível, com memória constante. */
    bool Audit(
        const READER_IO& io,
        RECOVERY& diagnostic
    );

    /**
     * @brief Encontra DATA pendente de menor ID acima de after, conservando conteúdo.
     * @details Busca confirmações por novas passagens. Custo quadrático no histórico,
     * memória constante. O proprietário deve ceder CPU entre leituras no ESP32.
     */
    bool FindPending(
        const READER_IO& io,
        ID after,
        DELIVERY& out,
        bool& found
    );

    // ─── CLASSES ──────────────────────────────────────────────────────

    /**
     * @brief Cache de 32 cópias, FIFO e uma tentativa/ACK de solo por vez.
     * @details Chamadas sequenciais ou protegidas pelo adaptador. I/O usa cópias
     * retornadas por BeginStorage e BeginTransmit, estáveis fora do lock. Um
     * slot em gravação fica reservado até FinishStorage. ACK pode anteceder SD.
     */
    class Queue {
    public:
        /** @brief Boot zero rejeita recepção. recovery_pending bloqueia TX até consulta SD. */
        void Init(
            uint64_t boot,
            bool recovery_pending
        );
        /** @brief Compara projeções e conta saturação; consulta pendente reserva uma posição. */
        void ObserveValidatedSnapshot(
            const obc_data::PAYLOAD_SNAPSHOT& snapshot
        );
        /** @brief Reserva DATA mais antigo ou CONFIRM cujo DATA terminou de gravar. */
        bool BeginStorage(
            RECORD& out
        );
        /** @brief Conclui reserva, após escrita e fechamento do segmento; falha é explícita. */
        void FinishStorage(
            const RECORD& record,
            bool success
        );
        /** @brief Reserva primeira tentativa ou retenta após timeout escolhido pelo chamador. */
        bool BeginTransmit(
            uint64_t now_ms,
            uint64_t timeout_ms,
            DELIVERY& out
        );
        /** @brief Evento de conclusão local; confirmação de solo tem API própria. */
        bool FinishTransmit(
            ID id,
            bool success
        );
        /** @brief Aceita apenas ID em trânsito ou confirmação já conhecida no boot. */
        bool Confirm(
            ID id
        );
        /** @brief Fronteira confirmada em RAM, para consultar histórico fora do lock. */
        ID GetFrontier(
        ) const;
        /** @brief Indica necessidade de consultar a próxima pendência durável. */
        bool NeedsRecovery(
        ) const;
        /** @brief Aplica consulta feita para a mesma fronteira; found nulo indica EOF. */
        bool Restore(
            ID frontier,
            const DELIVERY* found
        );
        /** @brief Falha de leitura libera oportunidade de envio das cópias em RAM. */
        void ReaderFailed(
        );
        /** @brief Amostra contadores e ocupação, sem zerar. */
        STATISTICS GetStatistics(
        ) const;

    private:
        struct ENTRY {
            DELIVERY delivery;
            bool used;
            bool stored;
            bool data_finished;
            bool confirmed;
            bool busy;
        };
        ENTRY entries_[cache_slots] = {};
        AIRCRAFT previous_[obc_data::max_snapshot_aircraft] = {};
        size_t previous_count_ = 0;
        uint64_t boot_ = 0;
        uint64_t sequence_ = 0;
        ID frontier_ = {};
        ID inflight_ = {};
        bool transmitting_ = false;
        bool waiting_ = false;
        bool disk_checked_ = false;
        uint64_t attempt_ms_ = 0;
        STATISTICS statistics_ = {};
        ENTRY* Find(
            ID id
        );
        ENTRY* Reserve(
            bool for_recovery = false
        );
        void Release(
            ENTRY& entry
        );
    };

} // namespace obc_delivery
