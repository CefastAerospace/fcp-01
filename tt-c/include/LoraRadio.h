/**
 * @file LoraRadio.h
 * @brief Abstração do rádio LoRa e driver do SX1276.
 */

// Include guard
#pragma once

#include <Arduino.h>     // para millis, digitalWrite, Serial
#include <SPI.h>         // para SPIClass, SPISettings
#include <LoRa.h>        // para LoRa (LoRaClass do sandeepmistry/LoRa)
#include "TtcProtocol.h" // para parâmetros de rádio e max_frame_len

// ─── STRUCTS ──────────────────────────────────────────────────────

/**
 * @brief GPIOs do SX1276, fornecidos pelo projeto integrador.
 */
struct RADIO_PINS {
    int sck;
    int miso;
    int mosi;
    int cs;
    int rst;
    int dio0;
};

// ─── CLASSES ──────────────────────────────────────────────────────

/**
 * @brief Interface de rádio, permite substituir o hardware em testes.
 */
class LoraRadio {
public:
    virtual ~LoraRadio() {}

    /**
     * @brief Inicializa o rádio em recepção.
     *
     * @return true se o rádio respondeu.
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
     * @brief Lê um quadro recebido.
     *
     * @return Bytes lidos; 0 se nada foi recebido; -1 se o quadro chegou
     * corrompido (CRC do rádio).
     */
    virtual int Receive(
        uint8_t* buffer,
        size_t max_len
    ) = 0;

    virtual int16_t GetRssi() = 0;
    virtual float GetSnr() = 0;
    virtual bool IsReady() = 0;
};

/**
 * @brief Driver do SX1276 sobre sandeepmistry/LoRa.
 *
 * @details Send() não usa o endPacket() síncrono, que espera TX_DONE sem
 * prazo; em timeout, o módulo é reinicializado.
 */
class Sx1276Radio : public LoraRadio {
public:
    explicit Sx1276Radio(
        const RADIO_PINS& pins
    );

    bool Init() override;
    bool Send(
        const uint8_t* data,
        size_t len
    ) override;
    int Receive(
        uint8_t* buffer,
        size_t max_len
    ) override;
    int16_t GetRssi() override;
    float GetSnr() override;
    bool IsReady() override;

private:
    RADIO_PINS pins;
    bool ready = false;

    /**
     * @brief Espera TX_DONE em IRQ_FLAGS até ttc::radio_tx_timeout_ms.
     */
    bool WaitTxDone();

    /**
     * @brief Acesso direto a registrador, não exposto por LoRaClass.
     *
     * @param address Endereço (bit 7 = 1 para escrita).
     * @param value Valor a escrever (0x00 em leitura).
     *
     * @return Byte devolvido pelo rádio.
     */
    uint8_t TransferRegister(
        uint8_t address,
        uint8_t value
    );
};
