#include "missions.h"
#include "config.h"
#include "sensors.h"
#include "motor_control.h"

// ============================================================================
//  Implementação do Módulo de Missões
// ============================================================================

// Estado interno
static MissionState currentMission = MISSION_IDLE;
static unsigned long missionStartTime = 0;
static unsigned long lastMissionUpdate = 0;

// ---- Inicialização ----
void mission_init() {
    currentMission = MISSION_IDLE;
    missionStartTime = 0;
    Serial.println("[MISSION] Modulo de missoes inicializado.");
}

// ---- Detumble Update ----
// Algoritmo simples de frenagem proporcional:
// O motor aplica torque contrário à velocidade angular medida pelo giroscópio.
// Quando a velocidade cai abaixo do threshold, o satélite está "estabilizado".
static void detumble_update() {
    const SensorData& data = sensors_getData();

    // Velocidade angular total (magnitude do vetor gyro)
    // Usamos gyro_z pois a roda de reação gira no eixo Z do satélite
    // Ajuste conforme a orientação de montagem
    float omega = data.gyro_z;  // °/s

    // Verifica se já estabilizou
    float absOmega = abs(omega);
    if (absOmega < DETUMBLE_THRESHOLD) {
        Serial.println("[MISSION] Detumble concluido — satelite estabilizado!");
        motor_stop();
        // Não para a missão automaticamente, continua monitorando
        return;
    }

    // Controle proporcional: torque = -Kd * omega
    // Converte °/s para rad/s para o SimpleFOC
    float omega_rad = omega * 0.01745f;  // °/s → rad/s
    float commandVelocity = -DETUMBLE_GAIN_KD * omega_rad;

    // Limita a velocidade comandada
    if (commandVelocity > MOTOR_VELOCITY_LIMIT) commandVelocity = MOTOR_VELOCITY_LIMIT;
    if (commandVelocity < -MOTOR_VELOCITY_LIMIT) commandVelocity = -MOTOR_VELOCITY_LIMIT;

    motor_setVelocity(commandVelocity);
}

// ---- Sun Pointing Update ----
// Algoritmo de apontamento solar por sensor diferencial:
// Dois sensores BH1750 montados em faces opostas (ou em ângulo).
// A diferença de luminosidade indica a direção do sol.
// O motor gira até equalizar as leituras.
static void pointing_update() {
    const SensorData& data = sensors_getData();

    // Verifica se há luz suficiente para apontar
    float maxLux = max(data.lux_sensor1, data.lux_sensor2);
    if (maxLux < POINTING_MIN_LUX) {
        motor_stop();
        return;  // Muito escuro, não faz nada
    }

    // Erro = diferença entre os sensores
    float error = data.lux_diff;  // sensor1 - sensor2

    // Dead-band para evitar oscilação
    if (abs(error) < POINTING_DEADBAND) {
        motor_stop();
        return;  // Já está apontado (dentro da tolerância)
    }

    // Controle proporcional
    float commandVelocity = POINTING_GAIN_KP * error;

    // Limita a velocidade
    if (commandVelocity > POINTING_VEL_LIMIT) commandVelocity = POINTING_VEL_LIMIT;
    if (commandVelocity < -POINTING_VEL_LIMIT) commandVelocity = -POINTING_VEL_LIMIT;

    motor_setVelocity(commandVelocity);
}

// ---- Dispatcher ----
void mission_update() {
    unsigned long now = millis();
    if (now - lastMissionUpdate < MISSION_UPDATE_INTERVAL_MS) {
        return;  // Ainda não é hora de atualizar
    }
    lastMissionUpdate = now;

    switch (currentMission) {
        case MISSION_DETUMBLE:
            detumble_update();
            break;
        case MISSION_SUN_POINTING:
            pointing_update();
            break;
        case MISSION_MANUAL_TEST:
            // No manual test, o motor é controlado diretamente via comandos
            break;
        case MISSION_IDLE:
        default:
            break;
    }
}

// ---- Controle de Missão ----
void mission_startDetumble() {
    if (currentMission != MISSION_IDLE && currentMission != MISSION_MANUAL_TEST) {
        Serial.println("[MISSION] Parando missao anterior...");
        motor_stop();
    }
    currentMission = MISSION_DETUMBLE;
    missionStartTime = millis();
    motor_enable();
    Serial.println("[MISSION] >>> DETUMBLE iniciado <<<");
}

void mission_startPointing() {
    if (currentMission != MISSION_IDLE && currentMission != MISSION_MANUAL_TEST) {
        Serial.println("[MISSION] Parando missao anterior...");
        motor_stop();
    }
    currentMission = MISSION_SUN_POINTING;
    missionStartTime = millis();
    motor_enable();
    Serial.println("[MISSION] >>> SUN POINTING iniciado <<<");
}

void mission_startManualTest() {
    currentMission = MISSION_MANUAL_TEST;
    missionStartTime = millis();
    Serial.println("[MISSION] Modo TESTE MANUAL ativado.");
}

void teste_apontamento(){
    currentMission = MISSION_TESTE_APONTAMENTO;
}

void mission_stop() {
    motor_stop();
    currentMission = MISSION_IDLE;
    missionStartTime = 0;
    Serial.println("[MISSION] Missao finalizada — IDLE.");
}

// ---- Estado ----
MissionState mission_getState() {
    return currentMission;
}

const char* mission_getStateString() {
    switch (currentMission) {
        case MISSION_IDLE:          return "Idle";
        case MISSION_DETUMBLE:      return "Detumble";
        case MISSION_SUN_POINTING:  return "Sun Pointing";
        case MISSION_MANUAL_TEST:   return "Teste Manual";
        default:                    return "Desconhecido";
    }
}

unsigned long mission_getElapsedTime() {
    if (missionStartTime == 0) return 0;
    return millis() - missionStartTime;
}

