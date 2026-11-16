/**
 * @file ObcResourceHealth.h
 * @brief Diagnóstico portátil de TT&C, heap e margens históricas de pilha.
 */
#pragma once

#include "ObcRecordCodec.h" // EVENT/FAULT existentes e identificação temporal.
#include <cstdio> // snprintf para descrições limitadas.

namespace obc_resource_health {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint32_t ttc_limit_ms = 2000;
    constexpr uint32_t ttc_period_ms = 100;
    constexpr uint32_t memory_period_ms = 1000;
    constexpr size_t heap_enter_bytes = 8192;
    constexpr size_t heap_leave_bytes = 12288;
    constexpr size_t block_enter_bytes = 4096;
    constexpr size_t block_leave_bytes = 6144;
    constexpr uint32_t stack_limit_bytes = 1024;
    constexpr size_t stack_count = 6;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    enum class STATE { INACTIVE, OK, SUSPECT, UNAVAILABLE };
    enum class STACK { UART, PROCESS, WRITER, I2C, TTC, LOOP };
    enum class CODE : uint16_t { TTC_PROGRESS = 260, RADIO, HEAP, STACK_MARGIN, ALLOCATION };

    /** @brief Somente cópias do proprietário TT&C; sem Arduino ou acesso ao rádio. */
    struct TTC_SAMPLE {
        bool init_attempted;
        bool initialized;
        bool task_started;
        bool radio_ready;
        uint32_t progress_ms;
        uint32_t sampled_ms;
        uint64_t completions;
    };

    struct TTC_REPORT {
        STATE progress;
        STATE radio;
        uint32_t age_ms;
        bool progress_changed;
        bool radio_changed;
    };

    /** @brief active separa pilha ausente de margem zero em tarefa criada. */
    struct STACK_SAMPLE {
        bool active;
        uint32_t free_bytes;
    };

    /** @brief byte_heap/block usam MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT. */
    struct MEMORY_SAMPLE {
        size_t byte_heap;
        size_t byte_largest;
        STACK_SAMPLE stacks[stack_count];
        uint64_t allocation_failures;
    };

    struct MEMORY_REPORT {
        bool heap_low;
        bool block_low;
        bool pressure_changed;
        uint8_t stack_low_mask;
        uint8_t stack_new_mask;
        uint64_t allocation_delta;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    const char* StateName(
        STATE state
    );
    const char* StackName(
        STACK stack
    );

    /** @brief Não grava; boot inválido preserva saída. true marca FAULT/error. */
    bool MakeEvent(
        CODE code,
        bool fault,
        const char* description,
        const obc_time::EVENT_STAMP& stamp,
        obc_record::RECORD& record
    );

    // ─── CLASSES ──────────────────────────────────────────────────────

    class TtcMonitor {
    public:
        /**
         * @brief Avalia atividade e disponibilidade separadamente.
         * @details Idade modular u32 exige detecção antes de um wrap completo.
         * Suspeita detectada persiste até progresso/mudança de atividade.
         * Primeira amostra normal/inativa não gera transição.
         */
        TTC_REPORT Evaluate(
            const TTC_SAMPLE& sample
        );
    private:
        bool sampled_ = false;
        TTC_SAMPLE previous_sample_ = {};
        TTC_REPORT previous_ = {};
    };

    class MemoryMonitor {
    public:
        /**
         * @brief Aplica histerese por métrica e retém alerta histórico de pilha.
         * @details Inatividade termina a vida observada da tarefa. Recusas de
         * alocação são cumulativas/saturadas e não dependem do heap amostrado.
         */
        MEMORY_REPORT Evaluate(
            const MEMORY_SAMPLE& sample
        );
    private:
        MEMORY_REPORT previous_ = {};
        uint64_t allocation_failures_ = 0;
    };

} // namespace obc_resource_health
