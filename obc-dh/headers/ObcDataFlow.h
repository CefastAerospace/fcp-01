/**
 * @file ObcDataFlow.h
 * @brief Estado, fila limitada e diário portáteis da primeira etapa INT-006.
 * @details Chamadas sequenciais. O adaptador protege cópias pequenas; I/O e
 * codificação de snapshots nunca ocorrem dentro da proteção compartilhada.
 */
#pragma once

#include "ObcRecordCodec.h" // Modelos e codecs v1/v2/v3 existentes.
#include "ObcStorage.h" // RESULT, STATUS e operações de arquivo.
#include <limits> // Saturação de contadores.

namespace obc_flow {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr size_t small_record_slots = 8;
    constexpr size_t snapshot_first_bytes = 32768;
    constexpr size_t snapshot_second_bytes = obc_record::max_snapshot_record_bytes - snapshot_first_bytes;
    // Maior registro canônico v1: MISSION, 105 bytes. Snapshots usam bloco próprio.
    constexpr size_t small_record_bytes = 105;
    static_assert(obc_record::telemetry_record_bytes <= small_record_bytes, "TM v3 deve caber na fila");

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Bloco pequeno integralmente codificado, sem ponteiros externos. */
    struct RECORD_BLOCK {
        size_t size;
        uint8_t bytes[small_record_bytes];
    };

    /** @brief Operações do serviço existente; contexto permite usar Storage no PC. */
    struct JOURNAL_IO {
        obc_storage::RESULT (*open)(
            void* context,
            const char* name
        );
        obc_storage::RESULT (*append)(
            void* context,
            const uint8_t* bytes,
            size_t size
        );
        obc_storage::RESULT (*close)(
            void* context
        );
        void* context;
    };

    /** @brief Ciclo de vida do armazenamento, separado do diário e das suas filas. */
    struct RECOVERY_IO {
        obc_storage::RESULT (*end)(
            void* context
        );
        obc_storage::RESULT (*init)(
            void* context
        );
        void* context;
    };

    /** @brief VERIFYING exige um registro normal confirmado, além dos metadados. */
    enum class RECOVERY_STATE { IDLE, WAITING, VERIFYING, EXHAUSTED, RETRY };

    /** @brief Diagnóstico cumulativo; attempts pertence somente ao episódio atual. */
    struct STORAGE_RECOVERY_DIAGNOSTICS {
        RECOVERY_STATE state;
        uint8_t attempts;
        uint64_t episodes;
        uint64_t total_attempts;
        uint64_t recovered;
        uint64_t exhausted;
        uint64_t lifecycle_failures;
        obc_storage::RESULT cause;
        obc_storage::RESULT last_end;
        obc_storage::RESULT last_result;
    };

    // ─── CLASSES ──────────────────────────────────────────────────────

    /** @brief Resumo compartilhável; não mantém cópia da lista de aeronaves. */
    class State {
    public:
        /** @brief Inicia estado sem referência UTC ou snapshot; boot zero desabilita o fluxo. */
        void Init(
            uint64_t boot_id
        );
        /** @brief Recebe somente snapshot já validado; verifica boot e contagem. */
        bool ObserveSnapshot(
            const obc_data::PAYLOAD_SNAPSHOT& snapshot
        );
        /** @brief Expande recepção u32 no boot atual, rejeitando idade ambígua >=2^31 ms. */
        bool SetTime(
            uint32_t unix_s,
            uint64_t now_ms,
            uint32_t rx_ms,
            uint32_t delay_ms,
            obc_time::TIME_REFERENCE& reference
        );
        /** @brief Copia referência vigente; false preserva a saída quando UTC é desconhecido. */
        bool GetTimeReference(
            obc_time::TIME_REFERENCE& reference
        ) const;
        /** @brief TM local v3: bench, UTC reconstruído e ac; demais fontes ainda ausentes. */
        obc_data::TELEMETRY_DATA ComposeTelemetry(
            uint64_t now_ms
        ) const;
        /** @brief Atualiza observações cumulativas do receptor sem reinterpretar gaps. */
        void SetPayloadStatistics(
            uint64_t complete_candidates,
            uint64_t rejected,
            uint64_t missing_positions,
            uint64_t duplicates
        );
        /** @brief Copia os contadores cumulativos do escritor. */
        void SetStorageStatistics(
            uint64_t written,
            uint64_t failures
        );
        /** @brief Conta perda explícita sem overflow e sem gerar outro registro. */
        void DropRecords(
            uint64_t count = 1
        );
        /** @brief Retorna uma amostra sem zerar os contadores. */
        obc_data::STATISTICS_DATA GetStatistics(
        ) const;

    private:
        uint64_t boot_id_ = 0;
        bool has_reference_ = false;
        bool has_snapshot_ = false;
        uint16_t aircraft_count_ = 0;
        obc_time::TIME_REFERENCE reference_ = {};
        obc_data::STATISTICS_DATA statistics_ = {};
    };

    /** @brief Fila de oito cópias fixas; proteção pertence ao adaptador. */
    class RecordQueue {
    public:
        /** @brief Copia um registro canônico <=105 bytes; falha não modifica a fila. */
        bool Push(
            const uint8_t* bytes,
            size_t size
        );
        /** @brief Retira a cópia mais antiga; vazia preserva out. */
        bool Pop(
            RECORD_BLOCK& out
        );
    private:
        RECORD_BLOCK blocks_[small_record_slots] = {};
        size_t head_ = 0;
        size_t size_ = 0;
    };

    /**
     * @brief Escritor exclusivo, sem filas, montagem ou recuperação automática.
     * @details Segmentos são exclusivos por boot, até INT32_MAX bytes; cada
     * segmento contém BOOT e a última referência já confirmada. Fonte de Write
     * permanece estável até o retorno. Init deve ocorrer uma vez por instância.
     */
    class Journal {
    public:
        /** @brief Cria segmento zero e persiste BOOT; qualquer falha mantém inativo. */
        bool Init(
            const JOURNAL_IO& io,
            const obc_time::EVENT_STAMP& stamp,
            uint32_t reset_reason
        );
        /**
         * @brief Retoma após End/Init externos, sempre no próximo segmento exclusivo.
         * @details Conserva boot e contadores; não repete o registro incerto. Cada
         * chamada consome um índice, inclusive em colisão/falha de abertura. A
         * referência opcional deve pertencer ao mesmo boot. Exige diário inativo.
         */
        bool Resume(
            const obc_time::TIME_REFERENCE* reference = nullptr
        );
        /** @brief Persiste referência do mesmo boot se diferente da última confirmada. */
        bool WriteReference(
            const obc_time::TIME_REFERENCE& reference
        );
        /** @brief Acrescenta registro integral; FILE_LIMIT rotaciona antes de repetir. */
        bool Write(
            const uint8_t* bytes,
            size_t size
        );
        /**
         * @brief Grava um snapshot em dois blocos sem rotacionar entre eles.
         * @details Conta um registro somente após confirmar ambas as partes.
         * Falha na segunda preserva cauda parcial, desativa o escritor e exige
         * descarte explícito pelo chamador; não confirma o registro incompleto.
         */
        bool WriteParts(
            const uint8_t* first,
            size_t first_size,
            const uint8_t* second,
            size_t second_size
        );
        /** @brief Informa se ainda aceita escrita, sem tentar recuperar falhas. */
        bool IsReady(
        ) const;
        /** @brief Fecha o segmento confirmado e abre outro para permitir leitura do anterior. */
        bool Seal(
        );
        uint64_t GetWritten(
        ) const;
        uint64_t GetFailures(
        ) const;
        obc_storage::RESULT GetLastResult(
        ) const;
    private:
        JOURNAL_IO io_ = {};
        obc_time::EVENT_STAMP boot_stamp_ = {};
        uint32_t segment_ = 0;
        uint32_t confirmed_ = 0;
        uint8_t boot_[40] = {};
        uint8_t reference_[36] = {};
        bool has_reference_ = false;
        bool attempted_ = false;
        bool boot_encoded_ = false;
        bool ready_ = false;
        uint64_t written_ = 0;
        uint64_t failures_ = 0;
        obc_storage::RESULT last_ = {};
        bool OpenSegment(
        );
        bool Append(
            const uint8_t* bytes,
            size_t size,
            bool count_record = true
        );
        bool Rotate(
        );
    };

    /**
     * @brief Três tentativas por episódio, espaçadas por 5 s, sem espera ou reboot.
     * @details Service executa no proprietário do SD. End/Init/Resume continuam
     * síncronos, fora de locks. Callbacks ausentes desabilitam a política nas
     * bancadas históricas. Metadados sozinhos não renovam o orçamento.
     */
    class StorageRecovery {
    public:
        static constexpr uint8_t max_attempts = 3;
        static constexpr uint64_t retry_delay_ms = 5000;
        /** @brief Retorna true somente após remontagem e abertura com metadados confirmados. */
        bool Service(
            const RECOVERY_IO& io,
            Journal& journal,
            uint64_t now_ms,
            const obc_time::TIME_REFERENCE* reference
        );
        /** @brief Sucesso de registro normal conclui episódio VERIFYING; falha não renova limite. */
        void ObserveWrite(
            bool success
        );
        /** @brief Amostra sem zerar contadores; proteção pertence ao adaptador. */
        STORAGE_RECOVERY_DIAGNOSTICS GetDiagnostics(
        ) const;
    private:
        STORAGE_RECOVERY_DIAGNOSTICS diagnostic_ = {};
        uint64_t waiting_since_ms_ = 0;
        void Exhaust(
        );
    };

} // namespace obc_flow
