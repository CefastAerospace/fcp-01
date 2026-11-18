/**
 * @file ObcResourcesBench.h
 * @brief Rádio falso e injeções finitas exclusivas de int008-resources-check.
 */
#pragma once

#include "ObcResourceHealth.h" // Amostras e registros do supervisor real.
#include "LoraRadio.h" // Interface existente; a tarefa Ttc continua real.
#include <atomic> // Passagem dos cenários entre loop OBC e tarefa TT&C.
#include <cstring> // Cópia do TC RF de bancada.

namespace obc_resources_bench {

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Inicia uma vez após set_time pelo consumidor real. */
    void Start();
    /** @brief Substitui somente valores da cópia diagnóstica, sem consumir heap/pilha. */
    void OverrideMemory(
        obc_resource_health::MEMORY_SAMPLE& sample
    );
    /** @brief Observa registros confirmados pelo backend SD simulado. */
    void ObserveRecord(
        const uint8_t* bytes,
        size_t size
    );

    // ─── CLASSES ──────────────────────────────────────────────────────

    /** @brief Implementa o transporte existente; Init também pode falhar na bancada. */
    class Radio : public LoraRadio {
    public:
        bool Init() override;
        bool CheckHealth() override;
        bool IsReady() override;
        bool Send(
            const uint8_t* data,
            size_t len
        ) override;
        int Receive(
            uint8_t* buffer,
            size_t capacity
        ) override;
        int16_t GetRssi() override;
        float GetSnr() override;
    private:
        bool contact_sent_ = false;
    };

} // namespace obc_resources_bench
