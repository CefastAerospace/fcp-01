/**
 * @file PayloadReceiverEsp32.h
 * @brief Adaptador sequencial UART2 para o receptor portátil do Payload.
 */
#pragma once

#include "PayloadReceiver.h" // Receiver, RESULT e STATUS portáteis.
#include "Pinout.h" // GPIO16/RX e GPIO17/TX existentes.
#include "ObcTimeEsp32.h" // GetMonotonicMs do boot corrente.
#include <Arduino.h> // Serial2 e HardwareSerial do framework.

namespace obc_payload {
namespace esp32 {

    // ─── HELPERS ──────────────────────────────────────────────────────

    /**
     * @brief Inicia UART2 em 115200 8N1 com RX 4 KiB e TX 256 bytes.
     * @details Reserva de boot pertence ao chamador. Receiver deve ter vida
     * estática ou equivalente, fora da pilha. Não inicializar concorrentemente
     * nem acessar Serial2 por outro caminho. Rejeita segunda inicialização.
     * Não inicia tarefa, missão, NVS ou qualquer consumidor automaticamente.
     * @param receiver Armazenamento estável, fornecido pela integração.
     * @param boot_id Boot confirmado e não nulo.
     * @return true quando UART e receptor estão prontos.
     */
    bool Init(
        Receiver& receiver,
        uint64_t boot_id
    );

    /**
     * @brief Atende até 256 bytes e um evento por chamada, sem esperar quadro.
     * @details Chamar ao menos a cada 10 ms e consumir READY antes da próxima
     * chamada. Resposta parcial fica retida; não descarta bytes já lidos nem
     * espera espaço de TX. O parse de um quadro completo ocorre na chamada;
     * não executar no atendimento FOC ou ISR. Mesma tarefa acessa o snapshot.
     * @return Resultado do núcleo ou WAITING durante envio da resposta.
     */
    STATUS Poll(
    );

} // namespace esp32
} // namespace obc_payload
