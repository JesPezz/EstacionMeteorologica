#include "MQTTManager.h"
#include "led.h"
#include "SensorManager.h" // Para acceder a los datos del sensor
#include "BME_Sensor.h"    // Para acceder a iaqSensor

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;

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

  mqttClient.connect();
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
    // Solo publicamos si estamos conectados
    if (!mqttClient.connected()) {
        Serial.println("⚠️ No se puede publicar: MQTT desconectado");
        return;
    }

    JsonDocument doc;
    
    // 1. Datos de Identificación
    doc["location"] = config.location;
    doc["device_id"] = WiFi.macAddress();
    
    // 2. Datos del Sensor (TODOS los parámetros del BME680/BSEC)
    
    // Principales
    doc["temperature"] = iaqSensor.temperature;       // Temp compensada
    doc["humidity"] = iaqSensor.humidity;             // Humedad compensada
    doc["pressure"] = iaqSensor.pressure / 100.0;     // Presión en hPa
    doc["iaq"] = iaqSensor.iaq;                       // Índice de Calidad de Aire (0-500)
    
    // Diagnóstico y Precisión (Claves para coincidir con tu CSV)
    doc["iaqAccuracy"] = iaqSensor.iaqAccuracy;       // 0=Estabilizando, 1-3=Válido
    doc["staticIaq"] = iaqSensor.staticIaq;           // IAQ sin autocalibración móvil
    doc["co2Equivalent"] = iaqSensor.co2Equivalent;   // CO2 estimado (ppm)
    doc["breathVocEquivalent"] = iaqSensor.breathVocEquivalent; // VOC estimado (ppm)
    
    // Valores "Raw" (Crudos)
    doc["rawTemperature"] = iaqSensor.rawTemperature;
    doc["rawHumidity"] = iaqSensor.rawHumidity;
    doc["gasResistance"] = iaqSensor.gasResistance;   // Resistencia del gas en Ohms
    doc["gasPercentage"] = iaqSensor.gasPercentage;
    
    // Estados del sistema
    doc["stabilizationStatus"] = iaqSensor.stabStatus; // 1 = listo
    doc["runInStatus"] = iaqSensor.runInStatus;        // 1 = listo

    // 3. Serializar a String
    String payload;
    serializeJson(doc, payload);

    // 4. Publicar
    String topic = config.mqttTopic;
    if (topic == "") topic = "estacion/datos"; 

    uint16_t packetId = mqttClient.publish(topic.c_str(), 1, false, payload.c_str());
    
    Serial.printf("📤 Publicando MQTT Completo [%s]: %s\n", topic.c_str(), payload.c_str());
    
    ledSuccess(); 
}