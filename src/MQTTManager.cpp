#include "MQTTManager.h"
#include "led_task.h"
#include "SensorManager.h" // Para acceder a los datos del sensor
#include "BME_Sensor.h"    // Para acceder a iaqSensor
#include "led_task.h"
#include "MQTTCommands.h"  // Comandos MQTT dual (individual + broadcast) y LWT
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;

// 🕒 Control de reintentos MQTT no bloqueante: último instante de intento de conexión
unsigned long lastMqttRetry = 0;
// Intervalo mínimo entre intentos de conexión MQTT (15s) para estabilizar la pila TCP/IP
const unsigned long MQTT_RETRY_INTERVAL_MS = 15000;

void connectToMqtt() {
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
  // Suscribirse a comandos duales (individual + broadcast)
  subscribeMQTTCommands(mqttClient);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("⚠️ Desconectado de MQTT.");
  
  if (WiFi.isConnected()) {
    // Si hay WiFi pero no MQTT, intentamos reconectar en 2 segundos
    xTimerStart(mqttReconnectTimer, 0);
  }
}

// 🔹 Callback de mensajes MQTT entrantes -> despachar a comandos
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total) {
  handleMqttMessage(topic, payload, properties, len, index, total);
}

void setupMQTT() {
  if (config.mqttServer == "") {
      Serial.println("⚠️ MQTT no configurado (Falta Servidor)");
      return;
  }

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  
  mqttClient.setServer(config.mqttServer.c_str(), config.mqttPort);
  
  // Credenciales opcionales
  if (config.mqttUser.length() > 0) {
      mqttClient.setCredentials(config.mqttUser.c_str(), config.mqttPassword.c_str());
  }

  // Last Will: marcar offline en estacion/reporte/<ubicacion> si el dispositivo cae
  setupMQTTWill(mqttClient);

  // Timer para reconexión automática sin bloquear (crear solo una vez)
  if (mqttReconnectTimer == nullptr) {
      mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(10000), pdFALSE, (void*)0, [](TimerHandle_t xTimer) {
        connectToMqtt();
      });
  }
}

// 🔄 Reconstruir el cliente MQTT para eliminar el estado TCP corrupto tras una caída de WiFi.
// AsyncMqttClient::connect() reutiliza el mismo AsyncClient interno que queda inválido
// tras una desconexión, provocando reintentos infinitos con motivo TCP_DISCONNECTED.
void resetMQTTClient() {
  Serial.println("🔄 Reconstruyendo cliente MQTT (reset AsyncClient interno)...");
  mqttClient.~AsyncMqttClient();
  new (&mqttClient) AsyncMqttClient();
  lastMqttRetry = 0; // Permitir reconexión inmediata
  setupMQTT();
  connectToMqtt();
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
