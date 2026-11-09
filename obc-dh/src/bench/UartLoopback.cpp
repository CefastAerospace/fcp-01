/**
 * @file UartLoopback.cpp
 * @brief Entrada isolada para exercitar o beta UART2 em loopback.
 * @details Compilada apenas no ambiente uart-loopback. ESP32-WROOM-32:
 * ligar GPIO17 a GPIO16 com a placa desligada e depois conectar o USB.
 * Os 60 envios preservam os vetores e o salto de sequência do beta.
 */

#include <Arduino.h> // Serial, millis e delay do framework.
#include "PayloadUart.h" // iniciarUart, enviarTeste e receberDados do beta.
#include <stdint.h> // uint8_t e uint32_t para contagem e tempo modular.

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "PayloadUart.h".
 */

namespace {

    // ─── ELEMENTOS STATIC ─────────────────────────────────────────────

    constexpr uint8_t message_count = 60;
    constexpr uint32_t send_interval_ms = 1000;
    uint32_t last_send_ms = 0;
    uint8_t sent_count = 0;
    bool completion_reported = false;

} // namespace

/** @brief Inicializa USB e UART2 sem iniciar outros serviços do OBC. */
void setup(
) {
    Serial.begin(115200);
    iniciarUart();
    Serial.println("[UART-TEST] 60 envios, um por segundo; esperado: 1, 2, 4, 5...61");
    Serial.println("[UART-TEST] Conferir recepcao e uma perda simulada da sequencia 3");
    last_send_ms = static_cast<uint32_t>(millis());
}

/** @brief Atende RX continuamente e transmite os vetores originais do beta. */
void loop(
) {
    receberDados();
    const uint32_t now_ms = static_cast<uint32_t>(millis());

    // A subtração modular mantém o intervalo mesmo no rollover de millis.
    if (static_cast<uint32_t>(now_ms - last_send_ms) >= send_interval_ms) {
        if (sent_count < message_count) {
            enviarTeste();
            ++sent_count;
            last_send_ms = now_ms;
        } else if (!completion_reported) {
            // O segundo final permite receber o último quadro. Encerramento
            // conta envios; não declara que houve recepção ou aprovação.
            Serial.println("[UART-TEST] 60 envios encerrados; conferir o log de recepcao");
            completion_reported = true;
        }
    }

    delay(1);
}
