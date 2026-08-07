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
void publishSensorData(); // Esta es la función clave

// Variables externas
extern AsyncMqttClient mqttClient;

#endif