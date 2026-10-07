#include "LoraRadio.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "LoraRadio.h".
 */

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

static constexpr uint8_t  sx1276_reg_fifo            = 0x00;
static constexpr uint8_t  sx1276_reg_fifo_addr_ptr   = 0x0D;
static constexpr uint8_t  sx1276_reg_fifo_rx_current = 0x10;
static constexpr uint8_t  sx1276_reg_irq_flags       = 0x12;
static constexpr uint8_t  sx1276_reg_rx_nb_bytes     = 0x13;
static constexpr uint8_t  sx1276_irq_rx_done         = 0x40;
static constexpr uint8_t  sx1276_irq_crc_error       = 0x20;
static constexpr uint8_t  sx1276_irq_tx_done         = 0x08;
static constexpr uint8_t  sx1276_write_bit           = 0x80;
static constexpr uint32_t sx1276_spi_hz              = 8E6;

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── CONSTRUTORES ─────────────────────────────────────────────────

// Público:
Sx1276Radio::Sx1276Radio(
    const RADIO_PINS& pins
) : pins(pins) {
}

// ─── GETTERS ──────────────────────────────────────────────────────

// Público:
int16_t Sx1276Radio::GetRssi(
) {
    return (int16_t)LoRa.packetRssi();
}

// Público:
float Sx1276Radio::GetSnr(
) {
    return LoRa.packetSnr();
}

// Público:
bool Sx1276Radio::IsReady(
) {
    return ready;
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
bool Sx1276Radio::Init(
) {
    SPI.begin(pins.sck, pins.miso, pins.mosi, pins.cs);
    LoRa.setPins(pins.cs, pins.rst, pins.dio0);

    if (!LoRa.begin(ttc::radio_frequency_hz)) {
        Serial.println("[TT&C] ERRO: SX1276 nao responde.");
        ready = false;
        return false;
    }

    LoRa.setSpreadingFactor(ttc::radio_spreading);
    LoRa.setSignalBandwidth(ttc::radio_bandwidth_hz);
    LoRa.setCodingRate4(ttc::radio_coding_rate);
    LoRa.setTxPower(ttc::radio_tx_power_dbm);
    LoRa.setSyncWord(ttc::radio_sync_word);
    LoRa.setPreambleLength(ttc::radio_preamble_len);
    LoRa.enableCrc();

    LoRa.receive();
    ready = true;
    Serial.println("[TT&C] SX1276 inicializado.");
    return true;
}

// Público:
bool Sx1276Radio::Send(
    const uint8_t* data,
    size_t len
) {
    if (!ready || data == nullptr || len == 0 || len > ttc::max_frame_len) return false;

    LoRa.idle();
    LoRa.beginPacket();
    LoRa.write(data, len);
    LoRa.endPacket(true);

    if (!WaitTxDone()) {
        Serial.println("[TT&C] ERRO: timeout no TX, reinicializando o SX1276.");
        Init();
        return false;
    }

    LoRa.receive();
    return true;
}

// Privado:
bool Sx1276Radio::WaitTxDone(
) {
    uint32_t start_ms = millis();
    while ((TransferRegister(sx1276_reg_irq_flags, 0x00) & sx1276_irq_tx_done) == 0) {
        if (millis() - start_ms > ttc::radio_tx_timeout_ms) return false;
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // SX1276 limpa a flag escrevendo 1 no bit.
    TransferRegister(sx1276_reg_irq_flags | sx1276_write_bit, sx1276_irq_tx_done);
    return true;
}

// Privado:
uint8_t Sx1276Radio::TransferRegister(
    uint8_t address,
    uint8_t value
) {
    SPI.beginTransaction(SPISettings(sx1276_spi_hz, MSBFIRST, SPI_MODE0));
    digitalWrite(pins.cs, LOW);
    SPI.transfer(address);
    uint8_t response = SPI.transfer(value);
    digitalWrite(pins.cs, HIGH);
    SPI.endTransaction();
    return response;
}

// Público:
int Sx1276Radio::Receive(
    uint8_t* buffer,
    size_t max_len
) {
    if (!ready || buffer == nullptr || max_len == 0) return 0;

    // Leitura direta do FIFO: parsePacket() trocaria o RX contínuo por
    // RX single, deixando o rádio surdo entre chamadas.
    uint8_t irq_flags = TransferRegister(sx1276_reg_irq_flags, 0x00);
    if ((irq_flags & sx1276_irq_rx_done) == 0) return 0;
    TransferRegister(sx1276_reg_irq_flags | sx1276_write_bit, irq_flags);
    if (irq_flags & sx1276_irq_crc_error) return -1;

    uint8_t packet_len = TransferRegister(sx1276_reg_rx_nb_bytes, 0x00);
    uint8_t fifo_start = TransferRegister(sx1276_reg_fifo_rx_current, 0x00);
    TransferRegister(sx1276_reg_fifo_addr_ptr | sx1276_write_bit, fifo_start);

    size_t read = min((size_t)packet_len, max_len);
    for (size_t i = 0; i < read; i++) {
        buffer[i] = TransferRegister(sx1276_reg_fifo, 0x00);
    }
    return (int)read;
}
