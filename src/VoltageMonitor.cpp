#include "VoltageMonitor.h"
#include "config.h"
#include "FS.h"
#include "SPIFFS.h"

// Estado interno
static unsigned long lastVoltageCheck = 0;
static float lastMeasuredVoltage = 0.0;

enum BatteryState {
    BAT_UNKNOWN = 0,
    BAT_ABSENT,
    BAT_UNDERVOLTAGE,
    BAT_OK
};
static BatteryState currentState = BAT_UNKNOWN;

void initVoltageMonitor() {
    // Asegurarse de que SPIFFS esté inicializado cuando se use writeLog
    SPIFFS.begin(true);

    // Configurar pin ADC si es necesario (en ESP32 no es obligatorio setear pinMode para ADC)
    // analogReadResolution(12); // Por defecto en algunos entornos
}

void checkVoltage() {
    unsigned long now = millis();
    if (now - lastVoltageCheck < config.voltageCheckIntervalMs) return;
    lastVoltageCheck = now;

    // Validar pin
    int pin = config.vbatPin;
    if (pin < 0) return;

    // Leer ADC
    int raw = analogRead(pin);
    const float maxAdc = 4095.0; // 12-bit ADC
    const float adcVref = 3.3;  // Referencia asumida
    float measured = ((float)raw / maxAdc) * adcVref * config.vdivRatio;
    lastMeasuredVoltage = measured;

    BatteryState newState = BAT_UNKNOWN;

    if (measured < config.minDetectVoltage) {
        newState = BAT_ABSENT;
    } else if (measured < config.voltageThreshold) {
        newState = BAT_UNDERVOLTAGE;
    } else {
        newState = BAT_OK;
    }

    if (newState != currentState) {
        // Cambió el estado -> escribir log descriptivo
        if (newState == BAT_ABSENT) {
            String msg = "❌ Batería ausente: lectura " + String(measured, 3) + " V";
            writeLog(msg);
        } else if (newState == BAT_UNDERVOLTAGE) {
            String msg = "⚠️ Fallo de voltaje detectado: " + String(measured, 3) + " V (umbral " + String(config.voltageThreshold, 3) + " V)";
            writeLog(msg);
        } else if (newState == BAT_OK) {
            String msg = "✅ Recuperación de voltaje: " + String(measured, 3) + " V";
            writeLog(msg);
        }
        currentState = newState;
    } else {
        // Si permanece en BAT_UNDERVOLTAGE, escribir un log de persistencia (cada chequeo)
        if (newState == BAT_UNDERVOLTAGE) {
            String msg = "⚠️ Persistencia de fallo de voltaje: " + String(measured, 3) + " V";
            writeLog(msg);
        }
        // No escribir spam para BAT_ABSENT o BAT_OK mientras no cambie
    }
}

float getMeasuredVoltage() {
    return lastMeasuredVoltage;
}

String getBatteryStatus() {
    switch (currentState) {
        case BAT_ABSENT: return "absent";
        case BAT_UNDERVOLTAGE: return "undervoltage";
        case BAT_OK: return "ok";
        default: return "unknown";
    }
}
