/**
 * @file LoraRadio.h
 * @brief Abstração do rádio LoRa e driver do SX1276.
 */

// Include guard
#pragma once

#include <Arduino.h>           // para millis, digitalWrite, attachInterrupt, Serial
#include <SPI.h>               // para SPIClass, SPISettings
#include <LoRa.h>              // para LoRa (LoRaClass do sandeepmistry/LoRa)
#include <freertos/FreeRTOS.h> // para TickType_t
#include <freertos/task.h>     // para TaskHandle_t, vTaskNotifyGiveFromISR, ulTaskNotifyTake
#include "LinkTransport.h"     // para LinkTransport
#include "TtcProtocol.h"       // para parâmetros de rádio e max_frame_len

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
 * @brief Interface de rádio: meio do enlace com medidas de sinal.
 */
class LoraRadio : public LinkTransport {
public:
    virtual int16_t GetRssi() = 0;
    virtual float GetSnr() = 0;

    /**
     * @brief Confere se o rádio responde e está em recepção.
     *
     * @details Chamado entre transmissões; false leva o Ttc a reinicializar
     * o rádio.
     */
    virtual bool CheckHealth() {
        return IsReady();
    }
};

/**
 * @brief Driver do SX1276 sobre sandeepmistry/LoRa.
 *
 * @details Send() não usa o endPacket() síncrono, que espera TX_DONE sem
 * prazo; em timeout, o módulo é reinicializado. O DIO0 gera interrupção em
 * RX Done e TX Done e acorda a task definida em SetNotifyTask(); a ISR não
 * acessa o SPI. Só uma instância pode existir, pois a ISR é estática.
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
    void SetNotifyTask(
        TaskHandle_t task
    ) override;

    /**
     * @brief Lê RegVersion (0x12 no SX1276) e RegOpMode (LoRa em RX contínuo).
     */
    bool CheckHealth() override;

private:
    RADIO_PINS pins;
    bool ready = false;
    TaskHandle_t notify_task = nullptr;

    static Sx1276Radio* irq_owner; // Instância atendida pela ISR do DIO0.

    /**
     * @brief ISR do DIO0: só notifica a task, sem acessar o SPI.
     */
    static void IRAM_ATTR OnDio0();

    /**
     * @brief Espera TX_DONE em IRQ_FLAGS até ttc::radio_tx_timeout_ms.
     *
     * @details Dorme na notificação do DIO0 entre leituras; a leitura de
     * IRQ_FLAGS cobre o caso de a task chamadora não ser a notificada.
     */
    bool WaitTxDone();

    /**
     * @brief Coloca o rádio em RX contínuo e confirma pelo RegOpMode.
     *
     * @details Depois do TX Done, o SX1276 ainda volta sozinho a STANDBY;
     * um RX pedido antes disso é sobrescrito e o rádio fica surdo. Espera
     * o fim do TX, pede RX e relê o modo até rx_entry_timeout_us.
     *
     * @return true se o rádio ficou em RX contínuo.
     */
    bool EnterReceive();

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
