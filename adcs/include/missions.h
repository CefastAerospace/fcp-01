#ifndef MISSIONS_H
#define MISSIONS_H

#include <Arduino.h>

// ============================================================================
//  Módulo de Missões — Detumble + Sun Pointing
//  Algoritmos de controle de atitude do ADCS
// ============================================================================

// Estados possíveis da missão
enum MissionState {
    MISSION_IDLE,           // Sem missão ativa
    MISSION_DETUMBLE,       // Detumbling — freiar rotação
    MISSION_SUN_POINTING,   // Apontamento solar — alinhar com o sol
    MISSION_MANUAL_TEST,     // Teste manual (motor comandado diretamente)
    MISSION_TESTE_APONTAMENTO
};

// Inicializa o módulo de missões
void mission_init();

// Dispatcher — chama o update da missão ativa (non-blocking)
void mission_update();

// Controle de missão
void mission_startDetumble();
void mission_startPointing();
void mission_startManualTest();
void mission_stop();

// Estado atual
MissionState mission_getState();
const char* mission_getStateString();

// Tempo ativo da missão atual (ms)
unsigned long mission_getElapsedTime();

#endif // MISSIONS_H
