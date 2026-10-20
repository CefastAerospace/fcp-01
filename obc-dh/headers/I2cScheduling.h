/**
 * @file I2cScheduling.h
 * @brief Política de atendimento I2C, independente do hardware e do FreeRTOS.
 */

#pragma once

#include <stddef.h> // size_t.
#include <stdint.h> // uint8_t e uint32_t.

namespace i2c {
namespace scheduling {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    /**
     * @brief Teto comum dos cinco níveis de prioridade, de zero a quatro.
     * @details Referências de entrada: baixa = 0, média = 2 e alta = 4.
     */
    constexpr uint8_t max_priority = 4;

    /** @brief Operações concluídas enquanto se aguarda para ganhar um nível de prioridade. */
    constexpr uint8_t completions_per_level = 1;

    static_assert(completions_per_level > 0, "completions_per_level deve ser positivo");

    // ─── STRUCTS ──────────────────────────────────────────────────────

    /** @brief Dados usados para ordenar e expirar uma solicitação. */
    struct REQUEST_ORDER {
        uint8_t base_priority;
        uint32_t queued_at_ms;
        uint32_t max_queue_wait_ms;
        uint32_t completed_while_waiting;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Calcula a prioridade efetiva, saturada no teto comum.
     * @param request Solicitação com prioridade base válida.
     * @return Prioridade base acrescida do envelhecimento, limitada ao teto.
     */
    inline uint8_t GetEffectivePriority(
        const REQUEST_ORDER& request
    ) {
        const uint32_t levels = request.completed_while_waiting / completions_per_level;
        const uint32_t effective_priority = request.base_priority + levels;
        return effective_priority >= max_priority
            ? max_priority
            : static_cast<uint8_t>(effective_priority);
    }

    /**
     * @brief Registra uma operação concluída enquanto a solicitação aguarda.
     * @details Contar apenas callbacks executados, inclusive os que retornam
     * falha. Chegadas, cancelamentos e descartes por timeout não são eventos
     * de envelhecimento. O contador satura quando a prioridade atinge o teto.
     * @param request Solicitação ainda na fila e com timeout não vencido.
     */
    inline void RegisterCompletion(
        REQUEST_ORDER& request
    ) {
        if (GetEffectivePriority(request) < max_priority) {
            ++request.completed_while_waiting;
        }
    }

    /**
     * @brief Verifica se a solicitação esgotou seu tempo de espera na fila.
     * @param request Solicitação cujo timeout é menor que metade do ciclo de uint32_t.
     * @param now_ms Tempo monotônico atual em ms.
     * @return true quando a espera alcança ou ultrapassa o timeout.
     */
    inline bool IsExpired(
        const REQUEST_ORDER& request,
        uint32_t now_ms
    ) {
        return static_cast<uint32_t>(now_ms - request.queued_at_ms)
            >= request.max_queue_wait_ms;
    }

    /**
     * @brief Seleciona uma solicitação, preservando a ordem de chegada em empates.
     * @details Solicitações vencidas são selecionadas primeiro para descarte,
     * sem executar a operação. A lista deve estar em ordem de chegada.
     * @param requests Lista de metadados válidos, ordenada por chegada.
     * @param count Quantidade de solicitações na lista.
     * @param now_ms Tempo monotônico atual em ms.
     * @return Índice selecionado; count quando a lista está vazia.
     */
    inline size_t SelectRequest(
        const REQUEST_ORDER* const* requests,
        size_t count,
        uint32_t now_ms
    ) {
        size_t selected = count;
        uint8_t highest_priority = 0;
        for (size_t index = 0; index < count; ++index) {
            if (IsExpired(*requests[index], now_ms)) {
                return index;
            }

            const uint8_t priority = GetEffectivePriority(*requests[index]);
            if (selected == count || priority > highest_priority) {
                selected = index;
                highest_priority = priority;
            }
        }
        return selected;
    }

} // namespace scheduling
} // namespace i2c
