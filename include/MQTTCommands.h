#ifndef MQTT_COMMANDS_H
#define MQTT_COMMANDS_H

#include <Arduino.h>
#include <espMqttClient.h>

// Configura el Last Will (LWT) offline en: estacion/reporte/<ubicacion>
void setupMQTTWill(espMqttClient& client);

// Suscribe a los topics de comandos (individual y broadcast)
void subscribeMQTTCommands(espMqttClient& client);

// Despachador de mensajes MQTT entrantes (llamado desde onMessageCallback)
void handleMqttMessage(const char* topic, char* payload,
                       const espMqttClientTypes::MessageProperties& properties,
                       size_t len, size_t index, size_t total);

// Procesa acciones pendientes no bloqueantes (reiniciar / actualizar) desde loop()
void processPendingMqttActions();

// Tópico de reporte de esta estación: estacion/reporte/<ubicacion>
String getReportTopic();

#endif // MQTT_COMMANDS_H