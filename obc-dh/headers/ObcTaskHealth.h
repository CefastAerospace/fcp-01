/**
 * @file ObcTaskHealth.h
 * @brief Progresso e avaliação diagnóstica das três tarefas locais do fluxo OBC.
 */
#pragma once

#include "ObcRecordCodec.h" // RECORD/EVENT/FAULT e identificação temporal existentes.
#include <cstdio> // snprintf para descrição limitada do evento.
#include <limits> // Saturação da contagem de conclusões.

namespace obc_health {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr size_t task_count = 3;
    constexpr uint64_t evaluation_period_ms = 100;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    enum class TASK { UART, PROCESS, WRITER };
    enum class ACTIVITY {
        INACTIVE, STARTING, IDLE, TRANSPORT, PROCESSING, ENCODING,
        OPEN, WRITE, CLOSE, READ, LIST, UNMOUNT, MOUNT, SCAN, RECOVERY_WAIT
    };
    enum class STATE { INACTIVE, OK, SUSPECT };

    /** @brief Publicado pelo proprietário; amostras e alterações exigem proteção externa. */
    struct PROGRESS {
        bool enabled;
        ACTIVITY activity;
        uint64_t since_ms;
        uint64_t last_completed_ms;
        uint64_t completions;
    };

    /** @brief Idade da atividade vigente, sem deduzir saúde pela chegada de aeronaves. */
    struct REPORT {
        TASK task;
        STATE state;
        ACTIVITY activity;
        uint64_t sampled_ms;
        uint64_t age_ms;
        uint64_t limit_ms;
        uint64_t completions;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Registra tarefa antes da criação; falha de criação deve limpar PROGRESS. */
    void Start(
        PROGRESS& progress,
        uint64_t now_ms
    );

    /** @brief Marca avanço real de atividade; completed conta também retornos de erro/sem dados. */
    void Mark(
        PROGRESS& progress,
        ACTIVITY activity,
        uint64_t now_ms,
        bool completed = false
    );

    /** @brief Limites diagnósticos: UART 100 ms, processamento 2 s, escritor 5 s. */
    uint64_t GetLimitMs(
        TASK task
    );
    const char* TaskName(
        TASK task
    );
    const char* StateName(
        STATE state
    );
    const char* ActivityName(
        ACTIVITY activity
    );

    /** @brief Monta evento OBC código 256..258; não grava nem confirma persistência. */
    bool MakeEvent(
        const REPORT& report,
        const obc_time::EVENT_STAMP& stamp,
        obc_record::RECORD& record
    );

    // ─── CLASSES ──────────────────────────────────────────────────────

    /** @brief Avaliação sequencial no loop; só mudanças de saúde geram transição. */
    class Monitor {
    public:
        /**
         * @brief Retorna transição para SUSPECT/OK; primeira amostra OK não gera evento.
         * @details Idade precisa exceder o limite. Tempo de publicação posterior
         * à amostragem resulta em idade zero, sem underflow. Zero é instante válido.
         * Tarefa inválida retorna false sem modificar report ou histórico.
         */
        bool Evaluate(
            TASK task,
            const PROGRESS& progress,
            uint64_t now_ms,
            REPORT& report
        );
    private:
        bool sampled_[task_count] = {};
        STATE previous_[task_count] = {};
    };

#ifdef OBC_TASK_HEALTH_BENCH
namespace bench {
    /** @brief Pausas exclusivas da bancada, chamadas fora dos locks e antes da operação. */
    void Before(
        TASK task,
        ACTIVITY activity
    );
}
#endif

} // namespace obc_health
