#include "MQTTCommands.h"
#include "config.h"
#include "web_server.h"     // Define AsyncWebServerRequest usado por ota_update.h
#include "MQTTManager.h"
#include "BME_Sensor.h"     // extern Bsec iaqSensor
#include "VoltageMonitor.h" // getMeasuredVoltage()
#include "ota_update.h"     // checkForUpdates() / downloadAndUpdate()
#include <ArduinoJson.h>

// ─────────────────────────────────────────────────────────────────────────────
// Identidad y tópicos
// ─────────────────────────────────────────────────────────────────────────────

// Normaliza la ubicación para usarla como tag en tópicos MQTT:
// minúsculas y espacios a guion bajo (p.ej. "Jardín Norte" -> "jardin_norte").
String getLocationTag() {
    String tag = config.location;
    tag.trim();
    tag.toLowerCase();
    tag.replace(' ', '_');
    if (tag.length() == 0) tag = "desconocida";
    return tag;
}

const char* COMMAND_TOPIC_BROADCAST = "estacion/all/comando";

String getCommandTopic()       { return "estacion/" + getLocationTag() + "/comando"; }
String getReportTopic()        { return "estacion/reporte/" + getLocationTag(); }

// ─────────────────────────────────────────────────────────────────────────────
// Estado interno para acciones no bloqueantes (reiniciar / OTA)
// ─────────────────────────────────────────────────────────────────────────────

static bool pendingRestart = false;
static unsigned long restartAt = 0;

static bool pendingOTA = false;
static unsigned long otaAt = 0;
static bool otaIsBroadcast = false;

// ─────────────────────────────────────────────────────────────────────────────
// LWT (Last Will)
// ─────────────────────────────────────────────────────────────────────────────

void setupMQTTWill(AsyncMqttClient& client) {
    // Buffers estáticos: AsyncMqttClient guarda los punteros, así que deben persistir.
    static char willTopic[96];
    static char willPayload[128];

    String topic = getReportTopic();
    topic.toCharArray(willTopic, sizeof(willTopic));

    JsonDocument doc;
    doc["ubicacion"] = getLocationTag();
    doc["status"] = "offline";
    size_t n = serializeJson(doc, willPayload, sizeof(willPayload));

    // Retain para que, al reconectar el broker, cualquier suscriptor conozca el estado.
    client.setWill(willTopic, 1, true, willPayload, n);
}

void subscribeMQTTCommands(AsyncMqttClient& client) {
    String cmdTopic = getCommandTopic();
    client.subscribe(cmdTopic.c_str(), 1);
    client.subscribe(COMMAND_TOPIC_BROADCAST, 1);
    Serial.printf("📥 Suscrito a comandos: %s | %s\n", cmdTopic.c_str(), COMMAND_TOPIC_BROADCAST);
}

// ─────────────────────────────────────────────────────────────────────────────
// Publicación de respuestas
// ─────────────────────────────────────────────────────────────────────────────

static void publishReport(JsonDocument& doc) {
    static char buffer[512];
    size_t n = serializeJson(doc, buffer, sizeof(buffer));
    if (n == 0) {
        Serial.println("❌ MQTT: no se pudo serializar la respuesta del comando.");
        return;
    }
    String reportTopic = getReportTopic();
    mqttClient.publish(reportTopic.c_str(), 1, false, buffer, n);
    Serial.printf("📤 Respuesta MQTT [%s]: %s\n", reportTopic.c_str(), buffer);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Manejo de comandos individuales
// ─────────────────────────────────────────────────────────────────────────────

static void handleCommand(const String& command, bool isBroadcast) {
    Serial.printf("⚙️ Comando recibido: '%s' (broadcast=%d)\n", command.c_str(), isBroadcast);

    JsonDocument doc;
    doc["ubicacion"] = getLocationTag();

    if (command == "ip") {
        doc["ip"] = WiFi.localIP().toString();
        doc["mac"] = WiFi.macAddress();
        doc["rssi"] = WiFi.RSSI();
        doc["ssid"] = WiFi.SSID();
        publishReport(doc);
    }
    else if (command == "estado") {
        doc["status"] = "online";
        doc["firmware"] = String(version);
        doc["uptime_s"] = millis() / 1000;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["bateria_v"] = getMeasuredVoltage();
        // Sin sensor de carga (INA219) en este firmware: se reporta -1 como desconocido
        doc["bateria_soc_porcentaje"] = -1;
        doc["bateria_status"] = getBatteryStatus();
        publishReport(doc);
    }
    else if (command == "clima") {
        doc["temp"] = iaqSensor.temperature;
        doc["hum"] = iaqSensor.humidity;
        doc["presion"] = iaqSensor.pressure / 100.0; // Pa -> hPa
        doc["lluvia"] = nullptr;     // Sensor no disponible en este firmware
        doc["viento_vel"] = nullptr;
        doc["viento_dir"] = nullptr;
        publishReport(doc);
    }
    else if (command == "metricas") {
        // INA219 no implementado en este firmware: campos en null
        doc["v_bus"] = nullptr;
        doc["i_ma"] = nullptr;
        doc["p_mw"] = nullptr;
        doc["balance_mw"] = nullptr;
        publishReport(doc);
    }
    else if (command == "reiniciar") {
        doc["status"] = "reiniciando";
        publishReport(doc);
        pendingRestart = true;
        restartAt = millis() + 1000; // reinicio no bloqueante al cabo de 1s
        Serial.println("🔁 Reinicio programado en 1 segundo.");
    }
    else if (command == "actualizar") {
        // Respuesta inmediata para no dejar la ventana de agregación vacía
        doc["status"] = "actualizando";
        publishReport(doc);

        if (isBroadcast) {
            // Seguridad Broadcast (Jitter): retardo aleatorio entre 1s y 10s
            otaAt = millis() + (unsigned long)random(1000, 10000);
            Serial.printf("📥 Comando actualizar broadcast: OTA programada en %lu ms (jitter)\n", otaAt - millis());
        } else {
            // Comando individual: OTA lo antes posible, sin bloquear el loop
            otaAt = millis() + 1000;
            Serial.println("📥 Comando actualizar individual: OTA programada en 1s.");
        }
        pendingOTA = true;
        otaIsBroadcast = isBroadcast;
    }
    else {
        doc["error"] = "comando_desconocido";
        doc["comando"] = command;
        publishReport(doc);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Despachador de mensajes entrantes
// ─────────────────────────────────────────────────────────────────────────────

// Buffer para reconstruir payloads fragmentados (topic + payload completo)
static char incomingPayload[1024];
static bool payloadInProgress = false;

void handleMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties,
                       size_t len, size_t index, size_t total) {
    if (index == 0) {
        payloadInProgress = true;
    }
    if (payloadInProgress && index + len <= sizeof(incomingPayload)) {
        memcpy(incomingPayload + index, payload, len);
    }

    // Dataset se completa cuando llega el último fragmento o total==len (mensaje en un solo paquete)
    if (index + len >= total) {
        payloadInProgress = false;
        incomingPayload[total] = '\0';

        String received = String(incomingPayload, total);
        received.trim();
        if (received.length() == 0) return;

        // Determinar si es comando broadcast o dirigido
        String commandTopic(topic);
        bool isBroadcast = (commandTopic == COMMAND_TOPIC_BROADCAST);
        handleCommand(received, isBroadcast);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Procesar acciones pendientes desde loop() (no bloqueante)
// ─────────────────────────────────────────────────────────────────────────────

void processPendingMqttActions() {
    if (pendingRestart && millis() >= restartAt) {
        pendingRestart = false;
        Serial.println("🔄 Ejecutando reinicio programado por comando MQTT.");
        delay(500);
        ESP.restart();
    }

    if (pendingOTA && millis() >= otaAt) {
        pendingOTA = false;
        Serial.println("📥 Ejecutando OTA programada por comando MQTT...");
        String fwURL = checkForUpdates();
        if (fwURL.length() > 0) {
            downloadAndUpdate(fwURL);
        } else {
            Serial.println("ℹ️ Comando actualizar: no hay versión nueva disponible.");
        }
    }
}

// Hook del onConnect: suscripción dual
void onMqttConnectedSubscriptions() {
    subscribeMQTTCommands(mqttClient);
}