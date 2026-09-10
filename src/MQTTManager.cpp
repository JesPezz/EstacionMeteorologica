#include "MQTTManager.h"
#include "led_task.h"
#include "SensorManager.h" // Para acceder a los datos del sensor
#include "BME_Sensor.h"    // Para acceder a iaqSensor
#include "led_task.h"
#include "MQTTCommands.h"  // Comandos MQTT dual (individual + broadcast) y LWT
#include "OfflineManager.h" // saveToBacklog() para respaldar cuando el broker está caído
espMqttClient mqttClient;
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

void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  Serial.println("⚠️ Desconectado de MQTT.");
  
  if (WiFi.isConnected()) {
    // Si hay WiFi pero no MQTT, intentamos reconectar en 2 segundos
    xTimerStart(mqttReconnectTimer, 0);
  }
}

// 🔹 Callback de mensajes MQTT entrantes -> despachar a comandos
void onMqttMessage(const espMqttClientTypes::MessageProperties& properties,
                   const char* topic, const uint8_t* payload,
                   size_t len, size_t index, size_t total) {
  handleMqttMessage(topic, const_cast<char*>(reinterpret_cast<const char*>(payload)),
                    properties, len, index, total);
}

void setupMQTT() {
  if (config.mqttServer == "") {
      Serial.println("⚠️ MQTT no configurado (Falta Servidor)");
      return;
  }

  // Desactivar modem sleep del WiFi para evitar timeouts de keepalive MQTT:
  // con la tarea síncrona de espMqttClient, el ahorro de energía del modem retrasa
  // los PINGREQ y Mosquitto expulsa al dispositivo ("exceeded timeout").
  WiFi.setSleep(false);

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

// 🔄 Reset del cliente MQTT tras una caída de WiFi.
// espMqttClient (síncrono con WiFiClient) no sufre el estado TCP corrupto del
// AsyncClient de AsyncMqttClient, pero conviene forzar una desconexión limpia y
// vaciar la cola para que la reconexión arranque desde un estado conocido.
void resetMQTTClient() {
  Serial.println("🔄 Reiniciando cliente MQTT (reset cola + reconexión)...");
  mqttClient.disconnect(true);
  mqttClient.clearQueue();
  lastMqttRetry = 0; // Permitir reconexión inmediata
  setupMQTT();
  connectToMqtt();
}

void publishSensorData() {
    // 🔍 Broker no disponible (aunque el WiFi pueda estar OK): en lugar de perder los
    // datos, respaldarlos en backlog inmediatamente para recuperarlos al reconectar.
    if (!mqttClient.connected()) {
        static unsigned long lastBrokerBackup = 0;
        if (millis() - lastBrokerBackup >= 60000) { // máx. 1 respaldo/min en la ventana de caída
            lastBrokerBackup = millis();
            static JsonDocument doc;
            doc.clear();
            populateSensorJson(doc);
            saveToBacklog(doc);
            writeLog("⚠️ Broker no disponible: dato respaldado en backlog.");
            Serial.println("⚠️ Broker no disponible: dato respaldado en backlog.");
        }
        return;
    }

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
        // 🛡️ QoS 0 en tiempo real: telemetría cada 3s tolera pérdida puntual, y evita los
        // PUBACK entrantes del broker que disparaban una fuga de memoria en AsyncTCP-esphome
        // (~184B por publicación, heap agotado en horas -> esp_wifi_init falla con NO_MEM).
        mqttClient.publish(config.mqttTopic.c_str(), 0, false, payloadBuffer);
        Serial.printf("📤 Publicando MQTT Completo [%s]: %s\n", config.mqttTopic.c_str(), payloadBuffer);
        signalLed(LED_SUCCESS); 
    } else {
        Serial.println("❌ Error al serializar JSON para MQTT");
    }
}
