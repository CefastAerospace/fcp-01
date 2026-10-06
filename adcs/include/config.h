#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
//  ADCS Firmware — Arquivo de Configuração Central
//  Edite este arquivo para ajustar ao seu hardware
// ============================================================================

// --- Pinos do SimpleFOC Mini (DRV8313) ---
#define PIN_IN1   27    // PWM fase A
#define PIN_IN2   26    // PWM fase B
#define PIN_IN3   25    // PWM fase C
#define PIN_EN    33    // Enable do driver

// --- Barramento I2C ---
#define PIN_SDA   21
#define PIN_SCL   22
#define I2C_CLOCK 400000  // 400 kHz (Fast Mode)

// --- Endereços I2C ---
#define MPU9250_ADDR    0x68   // AD0 = GND (use 0x69 se AD0 = HIGH)
#define BH1750_ADDR_1   0x23   // ADDR = GND
#define BH1750_ADDR_2   0x5C   // ADDR = VCC
/* Endereço do Encoder AS5600 é fixo no 0x36*/

// --- Parâmetros do Motor BLDC ---
// IMPORTANTE: Ajuste o número de pole pairs do seu motor!
// Conte os ímãs no rotor e divida por 2
#define MOTOR_POLE_PAIRS   7

// Tensão de alimentação (Volts)
#define MOTOR_VOLTAGE_SUPPLY   10.0f

// Limite de tensão aplicada ao motor (proteção contra superaquecimento)
#define MOTOR_VOLTAGE_LIMIT     6.0f

// Limite de velocidade (rad/s) — ~190 RPM
#define MOTOR_VELOCITY_LIMIT   20.0f

// --- PID de Velocidade ---
#define PID_VEL_P    0.2f
#define PID_VEL_I    2.0f
#define PID_VEL_D    0.0f
#define PID_VEL_RAMP 1000.0f   // Rampa de aceleração (V/s)
#define LPF_VEL_TF   0.01f    // Constante de tempo do filtro passa-baixa (s)

// --- PID de Posição (Ângulo) ---
#define PID_ANG_P    20.0f

// --- Tensão de alinhamento do sensor ---
#define VOLTAGE_SENSOR_ALIGN  3.0f

// --- WiFi ---
#define WIFI_SSID      "Aerospace-Net"
#define WIFI_PASSWORD   "4V0bOb391@JF"
#define WIFI_AP_SSID   "Aerospace-Net"
#define WIFI_AP_PASS   "4V0bOb391@JF"
#define WIFI_TIMEOUT_MS 15000  // Tempo limite para conectar (ms)

// --- Intervalos de Atualização (ms) ---
#define SENSOR_UPDATE_INTERVAL_MS   200   // Leitura dos sensores (5 Hz)
#define MISSION_UPDATE_INTERVAL_MS   50   // Atualização da missão (20 Hz)
#define TELEMETRY_PRINT_INTERVAL_MS 1000  // Print no Serial (1 Hz)

// --- Parâmetros do Detumble ---
#define DETUMBLE_GAIN_KD     0.5f    // Ganho proporcional (rad/s → V)
#define DETUMBLE_THRESHOLD   0.5f    // Limiar de "parado" (°/s)

// --- Parâmetros do Sun Pointing ---
#define POINTING_GAIN_KP     0.05f   // Ganho proporcional (lux diff → rad/s)
#define POINTING_DEADBAND    10.0f   // Dead-band (lux) para evitar oscilação
#define POINTING_MIN_LUX     5.0f    // Lux mínimo para ativar (ignora escuro)
#define POINTING_VEL_LIMIT   10.0f   // Velocidade máxima do apontamento (rad/s)

// --- Testes de Debug ---
#define DEBUG_VELOCITY_STEP   5.0f   // Incremento de velocidade nos botões (rad/s)
#define DEBUG_SPIN_VELOCITY  10.0f   // Velocidade do teste de rotação (rad/s)
#define DEBUG_SPIN_DURATION  3000    // Duração do teste de rotação (ms)

#endif // CONFIG_H

