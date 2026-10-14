/**
 * @file UmbilicalLink.h
 * @brief Enlace cabeado pela UART0 (USB-C), modo umbilical de bancada.
 *
 * @details Um pacote JSON por linha, terminada em '\n'. A mesma porta
 * transporta os logs de depuração, que nunca começam com '{': linhas sem
 * '{' inicial são ignoradas na recepção e pelo software da estação terrena.
 * Pontuado separadamente do RF na competição (DP §7.1, REQ-TTC-002).
 */

// Include guard
#pragma once

#include <Arduino.h>       // para HardwareSerial
#include "LinkTransport.h" // para LinkTransport
#include "TtcProtocol.h"   // para max_frame_len

// ─── CLASSES ──────────────────────────────────────────────────────

class UmbilicalLink : public LinkTransport {
public:
    UmbilicalLink(
        HardwareSerial& port,
        uint32_t baud
    );

    bool Init() override;

    /**
     * @brief Escreve o quadro seguido de '\n' em uma única escrita, sem
     * intercalar com logs de outras tasks.
     */
    bool Send(
        const uint8_t* data,
        size_t len
    ) override;

    /**
     * @brief Acumula a porta sem bloquear e devolve uma linha JSON completa.
     *
     * @return Bytes da linha (sem '\n'); 0 se não há linha JSON completa;
     * -1 se a linha excedeu max_len.
     */
    int Receive(
        uint8_t* buffer,
        size_t max_len
    ) override;

    bool IsReady() override;

private:
    HardwareSerial& port;
    uint32_t baud;
    bool ready = false;
    char line[ttc::max_frame_len + 1];
    size_t line_len = 0;
    bool overflow = false;

    /**
     * @brief Descarta a linha acumulada.
     */
    void ResetLine();
};
