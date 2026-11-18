/**
 * @file ObcWatchdogBench.h
 * @brief Injeções finitas e cookie RTC exclusivos das bancadas INT-012.
 */
#pragma once

#include "ObcWatchdog.h" // Recursos e limites de ação reais.
#include "ObcRecordCodec.h" // Observação de eventos no backend simulado.
#include "LoraRadio.h" // Transporte de rádio existente, substituído só na bancada.
#include "Ttc.h" // HoldTransmit e SendAck reais no cenário de saturação.
#include "I2cBus.h" // Execute real preservando o contexto do callback.
#include <atomic> // Coordenação entre loop e proprietários das tarefas.
#include <esp_attr.h> // RTC_NOINIT_ATTR, sem chaves de teste na NVS.
#include <esp_system.h> // Causa de reset usada para validar o cookie.

namespace obc_watchdog_bench {

    // ─── HELPERS ──────────────────────────────────────────────────────

    /** @brief Identifica o boot e impede repetir injeção após o reset esperado. */
    void Boot(
        uint64_t boot_id
    );
    /** @brief Arma o cenário após set_time, sem criar protocolo de controle novo. */
    void Start();
    /** @brief Pausa fora dos locks e na tarefa proprietária do recurso escolhido. */
    void Before(
        obc_watchdog::RESOURCE resource
    );
    /** @brief Pausa finita do próprio supervisor, sem alimentar por outra tarefa. */
    void BeforeLoop();
    /** @brief Enche as quatro vagas reais de ACK somente no cenário correspondente. */
    void BeforeReset(
        Ttc& link
    );
    /** @brief Marca reset esperado; cenário 10 bloqueia o consumidor após agendamento. */
    void AfterReset();
    /** @brief Guarda instante real do callback em RTC, sem serial, fila ou SD. */
    void ResetTimerFired();
    /** @brief Observa o codec real no backend; não comprova SD físico. */
    void ObserveRecord(
        const uint8_t* bytes,
        size_t size
    );

    // ─── CLASSES ──────────────────────────────────────────────────────

    /** @brief Rádio ausente; Receive mantém a tarefa TT&C real e sua injeção. */
    class Radio : public LoraRadio {
    public:
        bool Init() override;
        bool IsReady() override;
        bool Send(
            const uint8_t* data,
            size_t size
        ) override;
        int Receive(
            uint8_t* bytes,
            size_t capacity
        ) override;
        int16_t GetRssi() override;
        float GetSnr() override;
    };

} // namespace obc_watchdog_bench
