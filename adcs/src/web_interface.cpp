#include "web_interface.h"
#include "config.h"
#include "commands.h"
#include "sensors.h"
#include "motor_control.h"
#include "missions.h"
#include <WiFi.h>
#include <WebServer.h>

// ============================================================================
//  Implementação da Interface Web
//  Dashboard HTML com telemetria + botões de controle/debug
// ============================================================================

static WebServer sv(80);

// ---- Gera a página HTML do dashboard ----
static String buildDashboardHTML() {
    const SensorData& s = sensors_getData();

    String p = "<!DOCTYPE html>\n<html lang='pt-br'>\n<head>\n";
    p += "<meta charset='UTF-8'>\n";
    p += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n";
    p += "<meta http-equiv='refresh' content='2'>\n";
    p += "<title>ADCS — CubeSat</title>\n";
    p += "<style>\n";
    p += "* { box-sizing: border-box; margin: 0; padding: 0; }\n";
    p += "body { font-family: 'Segoe UI', Arial, sans-serif; background: #0a0e17; color: #e0e0e0; padding: 10px; }\n";
    p += "h1 { text-align: center; color: #4fc3f7; margin: 15px 0; font-size: 1.4em; }\n";
    p += "h2 { color: #81d4fa; font-size: 1.1em; margin: 10px 0 5px; border-bottom: 1px solid #1e3a5f; padding-bottom: 3px; }\n";
    p += ".card { background: #111827; border: 1px solid #1e3a5f; border-radius: 8px; padding: 10px; margin-bottom: 10px; }\n";
    p += "table { width: 100%; border-collapse: collapse; }\n";
    p += "th, td { padding: 4px 8px; text-align: center; border: 1px solid #1e3a5f; font-size: 0.9em; }\n";
    p += "th { background: #1a2744; color: #90caf9; }\n";
    p += "td { background: #0d1b2a; }\n";
    p += ".val { color: #4fc3f7; font-weight: bold; }\n";
    p += ".ok { color: #66bb6a; } .err { color: #ef5350; }\n";
    p += ".btn-row { display: flex; flex-wrap: wrap; gap: 6px; justify-content: center; margin: 6px 0; }\n";
    p += ".btn { padding: 10px 14px; font-size: 0.85em; border: none; border-radius: 6px; cursor: pointer; text-decoration: none; color: #fff; display: inline-block; }\n";
    p += ".btn-blue { background: #1565c0; } .btn-blue:hover { background: #1976d2; }\n";
    p += ".btn-green { background: #2e7d32; } .btn-green:hover { background: #388e3c; }\n";
    p += ".btn-orange { background: #e65100; } .btn-orange:hover { background: #f57c00; }\n";
    p += ".btn-red { background: #b71c1c; } .btn-red:hover { background: #c62828; }\n";
    p += ".btn-gray { background: #37474f; } .btn-gray:hover { background: #455a64; }\n";
    p += ".status-bar { display: flex; justify-content: space-between; background: #1a2744; padding: 6px 12px; border-radius: 6px; margin-bottom: 10px; font-size: 0.85em; }\n";
    p += ".status-item { text-align: center; }\n";
    p += ".status-label { color: #78909c; font-size: 0.75em; }\n";
    p += "</style>\n</head>\n<body>\n";

    // ---- Título ----
    p += "<h1>&#128752; ADCS — Controle de Atitude</h1>\n";

    // ---- Barra de Status ----
    p += "<div class='status-bar'>\n";
    p += "  <div class='status-item'><div class='status-label'>MISSAO</div><div class='val'>" + String(mission_getStateString()) + "</div></div>\n";
    p += "  <div class='status-item'><div class='status-label'>MOTOR</div><div class='" + String(motor_isEnabled() ? "ok" : "err") + "'>" + String(motor_isEnabled() ? "ON" : "OFF") + "</div></div>\n";
    p += "  <div class='status-item'><div class='status-label'>MODO</div><div class='val'>" + String(motor_getModeString()) + "</div></div>\n";
    p += "  <div class='status-item'><div class='status-label'>TEMPO</div><div class='val'>" + String(mission_getElapsedTime() / 1000) + "s</div></div>\n";
    p += "</div>\n";

    // ---- Motor ----
    p += "<div class='card'>\n<h2>&#9881; Motor — Roda de Reacao</h2>\n";
    p += "<table><tr><th>Vel. Atual (rad/s)</th><th>Vel. Alvo (rad/s)</th><th>Angulo (deg)</th><th>RPM</th></tr>\n";
    p += "<tr><td class='val'>" + String(motor_getVelocity(), 2) + "</td>";
    p += "<td class='val'>" + String(motor_getTarget(), 2) + "</td>";
    p += "<td class='val'>" + String(motor_getAngleDeg(), 2) + "</td>";
    p += "<td class='val'>" + String(motor_getVelocity() * 9.5493f, 1) + "</td></tr>\n";
    p += "</table>\n</div>\n";

    // ---- IMU ----
    p += "<div class='card'>\n<h2>&#128225; IMU — MPU9250";
    p += " <span class='" + String(s.mpu_ok ? "ok" : "err") + "'>[" + String(s.mpu_ok ? "OK" : "OFFLINE") + "]</span>";
    p += "</h2>\n";
    p += "<table><tr><th></th><th>X</th><th>Y</th><th>Z</th></tr>\n";
    p += "<tr><th>Accel (g)</th><td class='val'>" + String(s.accel_x, 3) + "</td><td class='val'>" + String(s.accel_y, 3) + "</td><td class='val'>" + String(s.accel_z, 3) + "</td></tr>\n";
    p += "<tr><th>Gyro (d/s)</th><td class='val'>" + String(s.gyro_x, 2) + "</td><td class='val'>" + String(s.gyro_y, 2) + "</td><td class='val'>" + String(s.gyro_z, 2) + "</td></tr>\n";
    p += "<tr><th>Angulo (d)</th><td class='val'>" + String(s.angle_x, 1) + "</td><td class='val'>" + String(s.angle_y, 1) + "</td><td class='val'>" + String(s.angle_z, 1) + "</td></tr>\n";
    p += "</table>\n";
    p += "<p style='text-align:right; font-size:0.8em; color:#78909c; margin-top:4px;'>Temp IMU: " + String(s.imu_temp, 1) + " &deg;C</p>\n";
    p += "</div>\n";

    // ---- Sensores de Luz ----
    p += "<div class='card'>\n<h2>&#9728; Sensores de Luz — BH1750</h2>\n";
    p += "<table><tr><th>Sensor 1 (lux)</th><th>Sensor 2 (lux)</th><th>Diferenca (lux)</th></tr>\n";
    p += "<tr><td class='val'>" + String(s.lux_sensor1, 1);
    p += " <span class='" + String(s.bh1750_1_ok ? "ok" : "err") + "'>[" + String(s.bh1750_1_ok ? "OK" : "!") + "]</span></td>";
    p += "<td class='val'>" + String(s.lux_sensor2, 1);
    p += " <span class='" + String(s.bh1750_2_ok ? "ok" : "err") + "'>[" + String(s.bh1750_2_ok ? "OK" : "!") + "]</span></td>";
    p += "<td class='val'>" + String(s.lux_diff, 1) + "</td></tr>\n";
    p += "</table>\n</div>\n";

    // ---- Botões de Missão ----
    p += "<div class='card'>\n<h2>&#127919; Missoes</h2>\n";
    p += "<div class='btn-row'>\n";
    p += "  <a class='btn btn-green' href='/cmd/detumble'>&#9211; Detumble</a>\n";
    p += "  <a class='btn btn-green' href='/cmd/pointing'>&#9728; Sun Pointing</a>\n";
    p += "  <a class='btn btn-orange' href='/cmd/mission_stop'>&#9632; Parar Missao</a>\n";
    p += "</div>\n</div>\n";

    // ---- Botões de Debug/Teste ----
    p += "<div class='card'>\n<h2>&#128295; Testes &amp; Debug</h2>\n";
    p += "<div class='btn-row'>\n";
    p += "  <a class='btn btn-blue' href='/cmd/vel_up'>&#9650; +" + String((int)DEBUG_VELOCITY_STEP) + " rad/s</a>\n";
    p += "  <a class='btn btn-blue' href='/cmd/vel_down'>&#9660; -" + String((int)DEBUG_VELOCITY_STEP) + " rad/s</a>\n";
    p += "  <a class='btn btn-gray' href='/cmd/stop'>&#9724; Parar</a>\n";
    p += "</div>\n";
    p += "<div class='btn-row'>\n";
    p += "  <a class='btn btn-blue' href='/cmd/spin'>&#8635; Spin Test</a>\n";
    p += "  <a class='btn btn-blue' href='/cmd/enable'>&#9889; Habilitar</a>\n";
    p += "  <a class='btn btn-red' href='/cmd/estop'>&#9888; EMERGENCIA</a>\n";
    p += "</div>\n</div>\n";

    p += "</body>\n</html>\n";
    return p;
}

// ---- Route Handlers ----
static void handleRoot() {
    sv.send(200, "text/html", buildDashboardHTML());
}

static void handleVelUp() {
    cmd_incrementVelocity(DEBUG_VELOCITY_STEP);
    sv.sendHeader("Location", "/");
    sv.send(303);
}

static void handleVelDown() {
    cmd_incrementVelocity(-DEBUG_VELOCITY_STEP);
    sv.sendHeader("Location", "/");
    sv.send(303);
}

static void handleStop() {
    cmd_stopMotor();
    sv.sendHeader("Location", "/");
    sv.send(303);
}

static void handleEStop() {
    cmd_emergencyStop();
    sv.sendHeader("Location", "/");
    sv.send(303);
}

static void handleEnable() {
    cmd_enableMotor();
    sv.sendHeader("Location", "/");
    sv.send(303);
}

static void handleSpin() {
    cmd_spinTest(DEBUG_SPIN_VELOCITY, DEBUG_SPIN_DURATION);
    sv.sendHeader("Location", "/");
    sv.send(303);
}

static void handleDetumble() {
    cmd_startDetumble();
    sv.sendHeader("Location", "/");
    sv.send(303);
}

static void handlePointing() {
    cmd_startPointing();
    sv.sendHeader("Location", "/");
    sv.send(303);
}

static void handleMissionStop() {
    cmd_stopMission();
    sv.sendHeader("Location", "/");
    sv.send(303);
}

static void handleSetVelocity() {
    if (sv.hasArg("v")) {
        float vel = sv.arg("v").toFloat();
        cmd_setVelocity(vel);
        sv.send(200, "text/plain", "OK: vel=" + String(vel));
    } else {
        sv.send(400, "text/plain", "Erro: parametro 'v' necessario");
    }
}

static void handleAPIStatus() {
    sv.send(200, "application/json", cmd_getStatusJSON());
}

static void handleNotFound() {
    sv.send(404, "text/plain", "Nao encontrado");
}

// ---- Inicialização ----
void web_init() {
    // Tenta conectar ao WiFi configurado
    Serial.printf("[WEB] Conectando ao WiFi '%s'...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startWiFi = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startWiFi < WIFI_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WEB] WiFi conectado! IP: ");
        Serial.println(WiFi.localIP());
    } else {
        // Fallback: cria Access Point próprio
        Serial.println("[WEB] WiFi falhou. Criando AP...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
        Serial.print("[WEB] AP criado! Conecte-se a '");
        Serial.print(WIFI_AP_SSID);
        Serial.print("' e acesse: http://");
        Serial.println(WiFi.softAPIP());
    }

    // Registra rotas
    sv.on("/", handleRoot);

    // Comandos de debug
    sv.on("/cmd/vel_up", handleVelUp);
    sv.on("/cmd/vel_down", handleVelDown);
    sv.on("/cmd/stop", handleStop);
    sv.on("/cmd/estop", handleEStop);
    sv.on("/cmd/enable", handleEnable);
    sv.on("/cmd/spin", handleSpin);
    sv.on("/cmd/vel", handleSetVelocity);

    // Missões
    sv.on("/cmd/detumble", handleDetumble);
    sv.on("/cmd/pointing", handlePointing);
    sv.on("/cmd/mission_stop", handleMissionStop);

    // API
    sv.on("/api/status", handleAPIStatus);

    sv.onNotFound(handleNotFound);
    sv.begin();

    Serial.println("[WEB] Servidor HTTP iniciado na porta 80.");
}

// ---- Loop ----
void web_loop() {
    sv.handleClient();
}

