#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <espMqttClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Callback para eventos MQTT (conexión, desconexión)
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason);

// Funciones principales
void setupMQTT();
void connectToMqtt();
void resetMQTTClient(); // Limpia estado del cliente MQTT tras caída WiFi (reconexión limpia)
void publishSensorData(); // Esta es la función clave

// Variables externas
extern espMqttClient mqttClient;
extern unsigned long lastMqttRetry;
extern const unsigned long MQTT_RETRY_INTERVAL_MS;

#endif