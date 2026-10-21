#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

// ============================================================================
//  Módulo de Controle do Motor — SimpleFOC + AS5600
//  Encapsula toda a lógica de controle FOC da roda de reação
// ============================================================================

// Modos de controle disponíveis
enum ControlMode {
    MODE_VELOCITY,        // Controle de velocidade em malha fechada
    MODE_ANGLE,           // Controle de posição em malha fechada
    MODE_TORQUE,          // Controle de torque (tensão)
    MODE_VELOCITY_OPEN,   // Velocidade em malha aberta (sem encoder)
    MODE_ANGLE_OPEN       // Posição em malha aberta (sem encoder)
};

// Inicializa motor, driver e encoder (chamar após Wire.begin())
void motor_init();

// Loop FOC — DEVE rodar na máxima frequência possível!
// Nunca coloque delay() ou operações bloqueantes antes desta chamada
void motor_loopFOC();

// Define velocidade alvo (rad/s)
void motor_setVelocity(float rad_s);

// Define posição alvo (rad)
void motor_setAngle(float rad);

// Para o motor suavemente (target = 0)
void motor_stop();

// Parada de emergência — desabilita o driver (corta corrente)
void motor_emergencyStop();

// Habilita o driver
void motor_enable();

// Desabilita o driver
void motor_disable();

// Altera o modo de controle
void motor_setMode(ControlMode mode);

// Leitura do estado atual
float motor_getVelocity();    // rad/s
float motor_getAngle();       // rad
float motor_getAngleDeg();    // deg
float motor_getTarget();      // target atual
ControlMode motor_getMode();  // modo atual
bool motor_isEnabled();       // driver habilitado?

// Retorna string descritiva do modo
const char* motor_getModeString();

#endif // MOTOR_CONTROL_H
