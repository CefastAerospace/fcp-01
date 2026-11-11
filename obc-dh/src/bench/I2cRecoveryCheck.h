/**
 * @file I2cRecoveryCheck.h
 * @brief Cenários COM-003 do núcleo portátil e do executor concorrente real.
 */
#pragma once
#ifdef ARDUINO
#include <Arduino.h> // Serial e entrada da bancada ESP32.
#endif
#include "I2cBus.h" // APIs e primitivas usadas pelo executor de produção.
#include <cstdio> // Relatório nativo.
#include <cstdlib> // abort em falha nativa.
#include <atomic> // Observação segura das invocações entre tarefas.
#include <initializer_list> // Casos de classificação usados pelo mesmo roteiro.

#ifndef OBC_I2C_RECOVERY_CHECK
#error "Usar o ambiente i2c-recovery-check ou o comando nativo documentado"
#endif
