/**
 * @file ObcWatchdogEsp32.h
 * @brief Watchdog manual do loop e reset independente do consumidor de comandos.
 */
#pragma once

#include "ObcWatchdog.h" // Limites e decisão portátil por recurso.
#include "ObcDataFlowEsp32.h" // Cópias de progresso e fila de eventos do OBC.
#include <esp_task_wdt.h> // API IDF 4.x de inscrição/alimentação da tarefa atual.
#include <esp_timer.h> // Temporizador one-shot no serviço nativo ESP_TIMER_TASK.

namespace obc_watchdog {
namespace esp32 {

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Arma uma vez após Serial.begin, no setup da tarefa loop.
     * @details Conserva IDLE0 e watchdog de interrupção; não habilita LoopWDT
     * automático. Falhas de watchdog/timer são independentes e explícitas.
     */
    void Init();

    /** @brief Alimenta somente no setup, após uma etapa retornar; não espera serviços. */
    void Checkpoint();

    /** @brief Encerra alimentação de inicialização; Service passa a verificar recursos. */
    void StartRuntime();

    /** @brief Avalia cópias a cada 100 ms; chamada bloqueada impede nova alimentação. */
    void Service(
        const LINK_DIAGNOSTICS& ttc
    );

    /**
     * @brief Agenda reset em 1500 ms antes de qualquer tentativa de ACK/registro.
     * @details Só o consumidor OBC chama. Repetição não adia prazo. Retorna
     * false sem declarar pendência se o timer não foi criado ou não iniciou.
     * Sucesso independe do SD/ACK e não confirma recepção pelo solo.
     */
    bool RequestReset();

} // namespace esp32
} // namespace obc_watchdog
