#include "sensors.h"
#include "config.h"
#include <Wire.h>
#include <MPU9250_WE.h>
#include <BH1750.h>

// ============================================================================
//  Implementação do Módulo de Sensores
// ============================================================================

// Instâncias dos sensores (internas ao módulo)
static MPU9250_WE mpu = MPU9250_WE(MPU9250_ADDR);
static BH1750 lightSensor1;
static BH1750 lightSensor2;

// Dados atuais dos sensores
static SensorData sData;

// Timer para leitura periódica
static unsigned long lastSensorRead = 0;

// ---- Inicialização ----
void sensors_init() {
    // --- MPU9250 ---
    if (!mpu.init()) {
        Serial.println("[SENSORS] ERRO: MPU9250 nao responde!");
        sData.mpu_ok = false;
    } else {
        Serial.println("[SENSORS] MPU9250 conectado.");
        sData.mpu_ok = true;

        Serial.println("[SENSORS] Calibrando MPU9250 — mantenha o satelite parado...");
        delay(1000);
        mpu.autoOffsets();
        Serial.println("[SENSORS] Calibracao concluida.");

        // Configuração do giroscópio
        mpu.enableGyrDLPF();
        mpu.setGyrDLPF(MPU9250_DLPF_6);
        mpu.setSampleRateDivider(99);
        mpu.setGyrRange(MPU9250_GYRO_RANGE_250);

        // Configuração do acelerômetro
        mpu.setAccRange(MPU9250_ACC_RANGE_2G);
        mpu.enableAccDLPF(true);
        mpu.setAccDLPF(MPU9250_DLPF_6);
    }

    // --- BH1750 Sensor 1 (ADDR = GND → 0x23) ---
    if (!lightSensor1.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_ADDR_1, &Wire)) {
        Serial.println("[SENSORS] ERRO: BH1750 #1 (0x23) nao encontrado!");
        sData.bh1750_1_ok = false;
    } else {
        Serial.println("[SENSORS] BH1750 #1 (0x23) conectado.");
        sData.bh1750_1_ok = true;
    }

    // --- BH1750 Sensor 2 (ADDR = VCC → 0x5C) ---
    if (!lightSensor2.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_ADDR_2, &Wire)) {
        Serial.println("[SENSORS] ERRO: BH1750 #2 (0x5C) nao encontrado!");
        sData.bh1750_2_ok = false;
    } else {
        Serial.println("[SENSORS] BH1750 #2 (0x5C) conectado.");
        sData.bh1750_2_ok = true;
    }

    // Zera dados numéricos preservando flags de status
    bool mpuOk = sData.mpu_ok;
    bool bh1Ok = sData.bh1750_1_ok;
    bool bh2Ok = sData.bh1750_2_ok;
    memset(&sData, 0, sizeof(SensorData));
    sData.mpu_ok = mpuOk;
    sData.bh1750_1_ok = bh1Ok;
    sData.bh1750_2_ok = bh2Ok;

    Serial.println("[SENSORS] Modulo de sensores inicializado.");
}

// ---- Atualização periódica (non-blocking) ----
void sensors_update() {
    unsigned long now = millis();
    if (now - lastSensorRead < SENSOR_UPDATE_INTERVAL_MS) {
        return;  // Ainda não é hora de ler
    }
    lastSensorRead = now;

    // --- Leitura do MPU9250 ---
    if (sData.mpu_ok) {
        xyzFloat gValue = mpu.getGValues();
        xyzFloat gyr    = mpu.getGyrValues();
        xyzFloat angle  = mpu.getAngles();

        sData.accel_x = gValue.x;
        sData.accel_y = gValue.y;
        sData.accel_z = gValue.z;
        sData.resultant_g = mpu.getResultantG(gValue);

        sData.gyro_x = gyr.x;
        sData.gyro_y = gyr.y;
        sData.gyro_z = gyr.z;

        sData.angle_x = angle.x;
        sData.angle_y = angle.y;
        sData.angle_z = angle.z;

        sData.imu_temp = mpu.getTemperature();
    }

    // --- Leitura dos BH1750 ---
    if (sData.bh1750_1_ok) {
        float lux = lightSensor1.readLightLevel();
        if (lux >= 0) sData.lux_sensor1 = lux;
    }
    if (sData.bh1750_2_ok) {
        float lux = lightSensor2.readLightLevel();
        if (lux >= 0) sData.lux_sensor2 = lux;
    }
    sData.lux_diff = sData.lux_sensor1 - sData.lux_sensor2;

    sData.last_update = now;
}

// ---- Acesso aos dados ----
const SensorData& sensors_getData() {
    return sData;
}

// ---- Debug Serial ----
void sensors_printSerial() {
    Serial.println("=== SENSORES ===");

    if (sData.mpu_ok) {
        Serial.printf("  Accel: x=%.2f  y=%.2f  z=%.2f g  (R=%.2f)\n",
                       sData.accel_x, sData.accel_y, sData.accel_z, sData.resultant_g);
        Serial.printf("  Gyro:  x=%.1f  y=%.1f  z=%.1f deg/s\n",
                       sData.gyro_x, sData.gyro_y, sData.gyro_z);
        Serial.printf("  Angle: x=%.1f  y=%.1f  z=%.1f deg\n",
                       sData.angle_x, sData.angle_y, sData.angle_z);
        Serial.printf("  IMU Temp: %.1f C\n", sData.imu_temp);
    } else {
        Serial.println("  MPU9250: OFFLINE");
    }

    Serial.printf("  Lux1: %.1f  Lux2: %.1f  Diff: %.1f\n",
                   sData.lux_sensor1, sData.lux_sensor2, sData.lux_diff);
    Serial.printf("  BH1750 #1: %s  #2: %s\n",
                   sData.bh1750_1_ok ? "OK" : "OFFLINE",
                   sData.bh1750_2_ok ? "OK" : "OFFLINE");
}

