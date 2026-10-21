#include "LoraRadio.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "LoraRadio.h".
 */

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────

static constexpr uint8_t  sx1276_reg_fifo            = 0x00;
static constexpr uint8_t  sx1276_reg_op_mode         = 0x01;
static constexpr uint8_t  sx1276_reg_version         = 0x42;
static constexpr uint8_t  sx1276_version             = 0x12;
static constexpr uint8_t  sx1276_mode_lora_rx        = 0x85; // LoRa + RX contínuo.
static constexpr uint8_t  sx1276_mode_mask           = 0x07;
static constexpr uint8_t  sx1276_mode_tx             = 0x03;
static constexpr uint8_t  sx1276_reg_fifo_addr_ptr   = 0x0D;
static constexpr uint8_t  sx1276_reg_fifo_rx_current = 0x10;
static constexpr uint8_t  sx1276_reg_irq_flags       = 0x12;
static constexpr uint8_t  sx1276_reg_rx_nb_bytes     = 0x13;
static constexpr uint8_t  sx1276_reg_dio_mapping_1   = 0x40;
static constexpr uint8_t  sx1276_dio0_tx_done        = 0x40; // Em RX, 0x00 = RX Done.
static constexpr uint8_t  sx1276_irq_rx_done         = 0x40;
static constexpr uint8_t  sx1276_irq_crc_error       = 0x20;
static constexpr uint8_t  sx1276_irq_tx_done         = 0x08;
static constexpr uint8_t  sx1276_write_bit           = 0x80;
static constexpr uint32_t sx1276_spi_hz              = 8E6;
static constexpr TickType_t  tx_done_poll         = pdMS_TO_TICKS(5);
static constexpr uint32_t rx_entry_timeout_us        = 2000;
static constexpr uint32_t rx_entry_settle_us         = 100;  // Entre pedir RX e reler o RegOpMode.

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

Sx1276Radio* Sx1276Radio::irq_owner = nullptr;

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

// Público:
bool Sx1276Radio::CheckHealth(
) {
    if (!ready) return false;
    uint8_t version = TransferRegister(sx1276_reg_version, 0x00);
    uint8_t op_mode = TransferRegister(sx1276_reg_op_mode, 0x00);
    if (version == sx1276_version && op_mode == sx1276_mode_lora_rx) return true;

    Serial.printf("[TT&C] SX1276: RegVersion=0x%02X RegOpMode=0x%02X.\n", version, op_mode);
    return false;
}

// Público:
void Sx1276Radio::SetNotifyTask(
    TaskHandle_t task
) {
    notify_task = task;
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

    // Interrupção própria no DIO0, sem o onReceive() da biblioteca, cuja
    // ISR lê o FIFO pelo SPI.
    irq_owner = this;
    pinMode(pins.dio0, INPUT);
    attachInterrupt(digitalPinToInterrupt(pins.dio0), OnDio0, RISING);

    // receive(), chamado em EnterReceive(), mapeia o DIO0 para RX Done.
    if (!EnterReceive()) {
        Serial.println("[TT&C] ERRO: SX1276 nao entrou em RX.");
        ready = false;
        return false;
    }
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
    TransferRegister(sx1276_reg_dio_mapping_1 | sx1276_write_bit, sx1276_dio0_tx_done);
    LoRa.endPacket(true);

    if (!WaitTxDone()) {
        Serial.println("[TT&C] ERRO: timeout no TX, reinicializando o SX1276.");
        Init();
        return false;
    }

    // O TX já terminou: falha aqui fica para a verificação de saúde.
    if (!EnterReceive()) Serial.println("[TT&C] ERRO: SX1276 nao voltou ao RX apos o TX.");
    return true;
}

// Privado:
bool Sx1276Radio::EnterReceive(
) {
    uint32_t start_us = micros();
    while (micros() - start_us < rx_entry_timeout_us) {
        if ((TransferRegister(sx1276_reg_op_mode, 0x00) & sx1276_mode_mask) == sx1276_mode_tx) continue;

        LoRa.receive();
        delayMicroseconds(rx_entry_settle_us);
        if (TransferRegister(sx1276_reg_op_mode, 0x00) == sx1276_mode_lora_rx) return true;
    }
    return false;
}

// Privado:
bool Sx1276Radio::WaitTxDone(
) {
    uint32_t start_ms = millis();
    while ((TransferRegister(sx1276_reg_irq_flags, 0x00) & sx1276_irq_tx_done) == 0) {
        if (millis() - start_ms > ttc::radio_tx_timeout_ms) return false;
        // Acorda no TX Done (DIO0) ou a cada tx_done_poll, o que vier antes.
        ulTaskNotifyTake(pdTRUE, tx_done_poll);
    }

    // SX1276 limpa a flag escrevendo 1 no bit.
    TransferRegister(sx1276_reg_irq_flags | sx1276_write_bit, sx1276_irq_tx_done);
    return true;
}

// Privado:
void IRAM_ATTR Sx1276Radio::OnDio0(
) {
    if (irq_owner == nullptr || irq_owner->notify_task == nullptr) return;

    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(irq_owner->notify_task, &woken);
    if (woken == pdTRUE) portYIELD_FROM_ISR();
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
