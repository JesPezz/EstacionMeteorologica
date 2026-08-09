#ifndef MQTT_COMMANDS_H
#define MQTT_COMMANDS_H

#include <Arduino.h>
#include <AsyncMqttClient.h>

// Configura el Last Will (LWT) offline en: estacion/reporte/<ubicacion>
void setupMQTTWill(AsyncMqttClient& client);

// Suscribe a los topics de comandos (individual y broadcast)
void subscribeMQTTCommands(AsyncMqttClient& client);

// Despachador de mensajes MQTT entrantes (llamado desde onMessageCallback)
void handleMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties,
                       size_t len, size_t index, size_t total);

// Procesa acciones pendientes no bloqueantes (reiniciar / actualizar) desde loop()
void processPendingMqttActions();

// Tópico de reporte de esta estación: estacion/reporte/<ubicacion>
String getReportTopic();

#endif // MQTT_COMMANDS_H