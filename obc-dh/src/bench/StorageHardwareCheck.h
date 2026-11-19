/** @file StorageHardwareCheck.h
 * @brief Bancada serial da TES-006 sobre o adaptador HSPI real.
 */
#pragma once

#include "ObcStorageEsp32.h" // Serviço, resultados e pinos do microSD.
#include <Arduino.h> // Serial, setup e loop da bancada.
#include <esp_timer.h> // Medição monotônica de cada operação.
#include <freertos/task.h> // Disputa entre tarefas em núcleos distintos.
