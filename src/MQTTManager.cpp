#include "MQTTManager.h"
#include "led_task.h"
#include "SensorManager.h" // Para acceder a los datos del sensor
#include "BME_Sensor.h"    // Para acceder a iaqSensor
#include "led_task.h"
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;

// 🕒 Control de reintentos MQTT no bloqueante: último instante de intento de conexión
unsigned long lastMqttRetry = 0;
// Intervalo mínimo entre intentos de conexión MQTT (15s) para estabilizar la pila TCP/IP
const unsigned long MQTT_RETRY_INTERVAL_MS = 15000;

void connectToMqtt() {
  // 🔍 DEBUG: Imprimir la configuración exacta que se está usando
  Serial.println("------------------------------------------------");
  Serial.printf("📡 Intentando conectar a MQTT...\n");
  /* Serial.printf("🎯 Host: '%s'\n", config.mqttServer.c_str()); // Comillas para ver si está vacío
  Serial.printf("🔌 Puerto: %d\n", config.mqttPort);
  Serial.printf("👤 Usuario: '%s'\n", config.mqttUser.c_str()); */
  Serial.println("------------------------------------------------");

  if (config.mqttServer == "") {
      Serial.println("❌ ERROR CRÍTICO: La IP del servidor MQTT está vacía.");
      Serial.println("👉 Ve a la Web > Configuración y guarda la IP de la Raspberry Pi.");
      return;
  }

  // 🕒 Temporizador de reintento no bloqueante: solo conectar si han pasado >= 15s
  // desde el último intento fallido, evitando churn en la pila de sockets TCP/IP.
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
      if (millis() - lastMqttRetry >= MQTT_RETRY_INTERVAL_MS) {
          lastMqttRetry = millis();
          Serial.println("📡 Intentando conectar a MQTT (Intervalo 15s)...");
          mqttClient.connect();
      }
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("✅ Conectado al Broker MQTT.");
  // Aquí podrías suscribirte a temas si quisieras recibir comandos en el futuro
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("⚠️ Desconectado de MQTT.");
  
  if (WiFi.isConnected()) {
    // Si hay WiFi pero no MQTT, intentamos reconectar en 2 segundos
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void setupMQTT() {
  if (config.mqttServer == "") {
      Serial.println("⚠️ MQTT no configurado (Falta Servidor)");
      return;
  }

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  
  mqttClient.setServer(config.mqttServer.c_str(), config.mqttPort);
  
  // Credenciales opcionales
  if (config.mqttUser.length() > 0) {
      mqttClient.setCredentials(config.mqttUser.c_str(), config.mqttPassword.c_str());
  }

  // Timer para reconexión automática sin bloquear
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(10000), pdFALSE, (void*)0, [](TimerHandle_t xTimer) {
    connectToMqtt();
  });
}

void publishSensorData() {
    if (!mqttClient.connected()) return;

    // 🧠 DIAGNÓSTICO: Heap antes de armar/armar payload
    Serial.printf("🧠 Heap ANTES de armado MQTT: %u bytes\n", ESP.getFreeHeap());

    // 🔄 Usar JsonDocument estático para evitar fragmentación y consumo de Stack
    static JsonDocument doc;
    doc.clear();
    
    populateSensorJson(doc); 

    // Usar un búfer estático serializado en lugar de concatenaciones masivas de String
    static char payloadBuffer[1024];
    size_t n = serializeJson(doc, payloadBuffer, sizeof(payloadBuffer));

    Serial.printf("🧠 Heap DESPUÉS de armado MQTT: %u bytes\n", ESP.getFreeHeap());

    if (n > 0) {
        mqttClient.publish(config.mqttTopic.c_str(), 1, false, payloadBuffer);
        Serial.printf("📤 Publicando MQTT Completo [%s]: %s\n", config.mqttTopic.c_str(), payloadBuffer);
        signalLed(LED_SUCCESS); 
    } else {
        Serial.println("❌ Error al serializar JSON para MQTT");
    }
}
