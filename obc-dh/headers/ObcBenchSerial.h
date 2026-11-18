/**
 * @file ObcBenchSerial.h
 * @brief UART0 compartilhada pela aplicação e pelos logs Arduino da bancada.
 */
#pragma once

#ifdef OBC_BUFFERED_FRAMEWORK_LOG
#include <Arduino.h> // Serial e HardwareSerial.
#include <atomic> // Publicação da prontidão ao outro núcleo.
#include <cstdarg> // Encaminhamento de argumentos do log Arduino.
#include <cstdio> // Formatação completa antes de escrever no driver.
#include <cstdlib> // Buffer de mensagens maiores.

namespace obc_bench_serial {

    /** @brief Abre a UART0 com TX de 8 KiB e habilita a saída comum após sucesso. */
    bool Init(
    );

} // namespace obc_bench_serial
#endif
