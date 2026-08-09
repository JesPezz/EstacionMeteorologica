#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Callback para eventos MQTT (conexión, desconexión)
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);

// Funciones principales
void setupMQTT();
void connectToMqtt();
void resetMQTTClient(); // Reconstruye AsyncMqttClient tras caída WiFi (limpiar estado TCP corrupto)
void publishSensorData(); // Esta es la función clave

// Variables externas
extern AsyncMqttClient mqttClient;
extern unsigned long lastMqttRetry;
extern const unsigned long MQTT_RETRY_INTERVAL_MS;

#endif