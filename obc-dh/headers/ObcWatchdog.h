/**
 * @file ObcWatchdog.h
 * @brief Decisão portátil de alimentação por progresso real, sem acionar hardware.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace obc_watchdog {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr size_t resource_count = 5;
    constexpr uint32_t evaluation_period_ms = 100;
    constexpr uint32_t watchdog_seconds = 5;
    constexpr uint32_t reset_delay_ms = 1500;

    // ─── STRUCTS ──────────────────────────────────────────────────────

    enum class RESOURCE { UART, PROCESS, WRITER, I2C, TTC };
    enum class CODE : uint16_t { PROGRESS = 265, RESET_REQUEST = 266 };

    /** @brief Ausência de tarefa não equivale a progresso vencido. Zero é idade válida. */
    struct SAMPLE {
        bool active;
        uint64_t age_ms;
    };

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Limites provisórios de bancada, separados dos diagnósticos da INT-008. */
    uint32_t GetLimitMs(
        RESOURCE resource
    );
    const char* ResourceName(
        RESOURCE resource
    );

    /** @brief Publicação posterior à amostragem tem idade zero, sem underflow. */
    uint64_t AgeMs(
        uint64_t now_ms,
        uint64_t progress_ms
    );

    /** @brief Diferença modular; detecção deve ocorrer antes de um wrap u32 completo. */
    uint32_t Age32Ms(
        uint32_t now_ms,
        uint32_t progress_ms
    );

    /**
     * @brief Bit por recurso cuja idade excede o limite; zero permite alimentação.
     * @details Não retém suspeita: novo progresso permite alimentar novamente.
     * Disponibilidade do periférico e resultado da operação não integram a decisão.
     */
    uint8_t BlockedMask(
        const SAMPLE (&samples)[resource_count]
    );

} // namespace obc_watchdog
