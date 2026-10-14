/**
 * @file LinkTransport.h
 * @brief Interface comum aos meios do enlace (rádio LoRa e umbilical).
 */

// Include guard
#pragma once

#include <stddef.h>            // para size_t
#include <stdint.h>            // para uint8_t
#include <freertos/FreeRTOS.h> // para TickType_t
#include <freertos/task.h>     // para TaskHandle_t

// ─── CLASSES ──────────────────────────────────────────────────────

/**
 * @brief Meio que transporta quadros JSON, permite substituir o hardware
 * em testes.
 */
class LinkTransport {
public:
    virtual ~LinkTransport() {}

    /**
     * @brief Inicializa o meio em recepção.
     *
     * @return true se o meio está pronto.
     */
    virtual bool Init() = 0;

    /**
     * @brief Transmite um quadro e volta à recepção.
     *
     * @return true se a transmissão terminou.
     */
    virtual bool Send(
        const uint8_t* data,
        size_t len
    ) = 0;

    /**
     * @brief Lê um quadro recebido, sem bloquear.
     *
     * @return Bytes lidos; 0 se nada foi recebido; -1 se o quadro chegou
     * corrompido ou excedeu max_len.
     */
    virtual int Receive(
        uint8_t* buffer,
        size_t max_len
    ) = 0;

    virtual bool IsReady() = 0;

    /**
     * @brief Define a task acordada pelos eventos do meio, se houver.
     *
     * @param task Task notificada; nullptr desliga a notificação.
     */
    virtual void SetNotifyTask(
        TaskHandle_t task
    ) {
        (void)task;
    }
};
