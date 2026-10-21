#include "motor_control.h"
#include "config.h"
#include <Wire.h>
#include <SimpleFOC.h>
#include <math.h>

// ============================================================================
//  Implementação do Módulo de Controle do Motor
// ============================================================================

// Instâncias do SimpleFOC (internas ao módulo)
static BLDCMotor motor = BLDCMotor(MOTOR_POLE_PAIRS);
static BLDCDriver3PWM driver = BLDCDriver3PWM(PIN_IN1, PIN_IN2, PIN_IN3, PIN_EN);
static MagneticSensorI2C encoder = MagneticSensorI2C(AS5600_I2C);

// Estado interno
static ControlMode currentMode = MODE_VELOCITY;
static float currentTarget = 0.0f;
static bool driverEnabled = true;

// ---- Inicialização ----
void motor_init() {
    Serial.println("[MOTOR] Inicializando encoder AS5600...");
    encoder.init(&Wire);
    motor.linkSensor(&encoder);

    Serial.println("[MOTOR] Inicializando driver SimpleFOC Mini...");
    driver.voltage_power_supply = MOTOR_VOLTAGE_SUPPLY;
    driver.voltage_limit = MOTOR_VOLTAGE_SUPPLY;
    driver.init();
    motor.linkDriver(&driver);

    // Configuração do controle
    motor.controller = MotionControlType::velocity;
    motor.torque_controller = TorqueControlType::voltage;
    motor.voltage_limit = MOTOR_VOLTAGE_LIMIT;
    motor.velocity_limit = MOTOR_VELOCITY_LIMIT;
    motor.voltage_sensor_align = VOLTAGE_SENSOR_ALIGN;

    // PID de velocidade
    motor.PID_velocity.P = PID_VEL_P;
    motor.PID_velocity.I = PID_VEL_I;
    motor.PID_velocity.D = PID_VEL_D;
    motor.PID_velocity.output_ramp = PID_VEL_RAMP;
    motor.LPF_velocity.Tf = LPF_VEL_TF;

    // PID de posição
    motor.P_angle.P = PID_ANG_P;

    // Inicialização do motor e alinhamento FOC
    Serial.println("[MOTOR] Inicializando motor...");
    motor.init();
    Serial.println("[MOTOR] Alinhando sensor (o motor vai se mover)...");
    motor.initFOC();
    

    currentTarget = 0.0f;
    driverEnabled = true;

    Serial.println("[MOTOR] Motor pronto!");
}

// ---- Loop FOC (CRÍTICO — máxima frequência!) ----
void motor_loopFOC() {
    motor.loopFOC();
    motor.move(currentTarget);
}

// ---- Comandos de controle ----
void motor_setVelocity(float rad_s) {
    if (currentMode != MODE_VELOCITY) {
        motor_setMode(MODE_VELOCITY);
    }
    // Limita a velocidade
    if (rad_s > MOTOR_VELOCITY_LIMIT) rad_s = MOTOR_VELOCITY_LIMIT;
    if (rad_s < -MOTOR_VELOCITY_LIMIT) rad_s = -MOTOR_VELOCITY_LIMIT;

    currentTarget = rad_s;
    Serial.printf("[MOTOR] Velocidade alvo: %.2f rad/s (%.1f RPM)\n",
                  rad_s, rad_s * 9.5493f);
}

void motor_setAngle(float rad) {
    if (currentMode != MODE_ANGLE) {
        motor_setMode(MODE_ANGLE);
    }
    currentTarget = rad;
    Serial.printf("[MOTOR] Angulo alvo: %.2f rad (%.1f deg)\n",
                  rad, rad * 57.2958f);
}

void motor_stop() {
    currentTarget = 0.0f;
    motor.disable();
    driverEnabled = false;
    Serial.println("[MOTOR] Motor parado (driver desabilitado).");
}

void motor_emergencyStop() {
    currentTarget = 0.0f;
    motor.disable();
    driverEnabled = false;
    Serial.println("[MOTOR] !!! PARADA DE EMERGENCIA — Driver desabilitado !!!");
}

void motor_enable() {
    motor.enable();
    driverEnabled = true;
    currentTarget = 0.0f;
    Serial.println("[MOTOR] Driver habilitado.");
}

void motor_disable() {
    currentTarget = 0.0f;
    motor.disable();
    driverEnabled = false;
    Serial.println("[MOTOR] Driver desabilitado.");
}

void motor_setMode(ControlMode mode) {
    currentTarget = 0.0f;  // Reseta target ao trocar modo

    switch (mode) {
        case MODE_VELOCITY:
            motor.controller = MotionControlType::velocity;
            break;
        case MODE_ANGLE:
            motor.controller = MotionControlType::angle;
            break;
        case MODE_TORQUE:
            motor.controller = MotionControlType::torque;
            break;
        case MODE_VELOCITY_OPEN:
            motor.controller = MotionControlType::velocity_openloop;
            break;
        case MODE_ANGLE_OPEN:
            motor.controller = MotionControlType::angle_openloop;
            break;
    }

    currentMode = mode;
    Serial.printf("[MOTOR] Modo alterado para: %s\n", motor_getModeString());
}

// ---- Leituras ----
float motor_getVelocity() {
    return motor.shaft_velocity;
}

float motor_getAngle() {
    return motor.shaft_angle;
}

float motor_getAngleDeg() {
    float angleDeg = fmod(motor.shaft_angle * 180.0 / PI, 360.0);

    if (angleDeg < 0) {
        angleDeg += 360.0;
    }
    return angleDeg;
}

float motor_getTarget() {
    return currentTarget;
}

ControlMode motor_getMode() {
    return currentMode;
}

bool motor_isEnabled() {
    return driverEnabled;
}

const char* motor_getModeString() {
    switch (currentMode) {
        case MODE_VELOCITY:       return "Velocidade";
        case MODE_ANGLE:          return "Posicao";
        case MODE_TORQUE:         return "Torque";
        case MODE_VELOCITY_OPEN:  return "Vel. Aberta";
        case MODE_ANGLE_OPEN:     return "Pos. Aberta";
        default:                  return "Desconhecido";
    }
}

