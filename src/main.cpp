#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include <EEPROM.h>
#include <vector>
#include <ThingSpeak.h>
#include "config.h"
#include "BME_Sensor.h"
#include "WiFiManager.h"
#include "TimeManager.h"
//#include <bme68xLibrary.h>
#include "web_server.h"
#include "BME_sensor.h"
#include <bsec.h>
#include "SensorManager.h"
#include "ota_update.h"
#include "esp_ota_ops.h"
#include "led.h"
#include "notifications.h"
#include "web_server.h"
#include <freertos/timers.h>
#include "MQTTManager.h"
#include "led_task.h"
#include "OfflineManager.h"

// En main.cpp (variables globales o estáticas)
int lastProcessedHour = -1; 
unsigned long backlogWaitTime = 0;
bool backlogReady = false;
int lastProcessedMinute = -1; // Variable auxiliar para pruebas


void setup() {
  
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1);
  Serial.begin(115200);
  
  if(!SPIFFS.begin(true)) {
    Serial.println("Error al montar SPIFFS");
    writeLog("❌ Error al montar SPIFFS");
    delay(1000);
    ESP.restart();
  }
  Serial.println();
  Serial.println("SPIFFS montado correctamente");

  // Test de escritura SPIFFS
File testFile = SPIFFS.open("/test.txt", FILE_WRITE);
if (!testFile) {
  Serial.println("¡Error crítico! SPIFFS no permite escritura");
  writeLog("❌ ¡Error crítico! SPIFFS no permite escritura");
} else {
  testFile.println("Prueba");
  testFile.close();
  SPIFFS.remove("/test.txt");
}

  testFlash();


// // Prueba de logging con marca de tiempo
//     writeLog("=================================");
//     writeLog("    Prueba de sistema de logs    ");
//     writeLog("Fecha: " + getDateTimeString());
//     writeLog("=================================");

//     // Verificar si se escribió correctamente
//     if(SPIFFS.exists(LOG_FILE)) {
//         File logFile = SPIFFS.open(LOG_FILE, "r");
//         Serial.println("Contenido del log:");
//         while(logFile.available()) {
//             Serial.write(logFile.read());
//         }
//         logFile.close();
//     } else {
//         Serial.println("No se creó el archivo de log");
//     }

//     Serial.println("=== Prueba de SPIFFS ===");
// File file = SPIFFS.open(LOG_FILE, "r");
// if(file){
//     Serial.printf("Tamaño del log: %d bytes\n", file.size());
//     file.close();
// } else {
//     Serial.println("¡No se pudo abrir el archivo!");
// }


   // Verifica espacio libre
   size_t total = SPIFFS.totalBytes();
   size_t used = SPIFFS.usedBytes();
   Serial.printf("SPIFFS: %d/%d bytes usados\n", used, total);
  writeLog("SPIFFS: " + String(used) + "/" + String(total) + " bytes usados");
   
   if (total - used < 250000) { // 250KB mínimo recomendado
    Serial.println("Espacio insuficiente en SPIFFS");
    writeLog("❌ Espacio insuficiente en SPIFFS");
    return;
  }
  
  loadConfig();
  setupMQTT();
   // Definir el nombre del código y la ubicación
  Serial.println();
  Serial.print("Version: ");
  Serial.print(nombreCodigo);
  Serial.print(" ");
  Serial.println(version);
  Serial.println("Ubicacion: " + config.location);
  
    
  printWiFiNetwork();
  Serial.println();
  
  esp_partition_t *runningPartition = (esp_partition_t *)esp_ota_get_running_partition();
  Serial.printf("🔍 Ejecutando desde la partición: %s\n", runningPartition->label);
  
  // Obtener el tamaño total de la Flash
  Serial.printf("📦 Tamaño total de la Flash: %u bytes (%.2f MB)\n", ESP.getFlashChipSize(), ESP.getFlashChipSize() / (1024.0 * 1024.0));
  
  // Obtener el tamaño de la partición OTA
  Serial.printf("📦 Tamaño de la partición actual: %u bytes (%.2f MB)\n", ESP.getSketchSize(), ESP.getSketchSize() / (1024.0 * 1024.0));
  Serial.printf("📦 Espacio libre para OTA: %u bytes (%.2f MB)\n", ESP.getFreeSketchSpace(), ESP.getFreeSketchSpace() / (1024.0 * 1024.0));
  setupLedTask();
  startAPMode();
  startWebServer();
  Serial.println();
  iaqSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  setupBsecSensor();
  Serial.println();

  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  Serial.println(output);
  Serial.println();

initWiFiScanner();
initSensorMutex();
initSSETimer();
Serial.println();
printConfig();  // ✅ Ver los valores actuales de configuración
  
  pinMode(LED_BUILTIN, OUTPUT);
  
  
  // Sincronizar el reloj una vez al mes
  syncClock();
  
  
  // Configurar sensores
  bsec_virtual_sensor_t sensorList[13] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_GAS_PERCENTAGE
  };
  
  iaqSensor.updateSubscription(sensorList, 13, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();
  //checkForIndexUpdate();
  checkForUpdates();
  Serial.println("📜 ARCHIVOS DEL SISTEMA");
  listSPIFFS();
  Serial.println();
  sendTelegramMessage("ℹ️ Estado del ESP32", config);
  Serial.println();
  
  // Imprimir el encabezado
  output = "Timestamp [ms], IAQ, IAQ accuracy, Static IAQ, CO2 equivalent, breath VOC equivalent, raw temp[°C], pressure [hPa], raw relative humidity [%], gas [Ohm], Stab Status, run in status, comp temp[°C], comp humidity [%], gas percentage";
  Serial.println(output);
  Serial.println();
  xTaskCreatePinnedToCore(
    wifiScanTask,    // Función
    "WiFiScanner",   // Nombre
    4096,           // Stack size (suficiente para JSON)
    NULL,           // Parámetros
    1,              // Prioridad (1 = baja, debajo de WiFi/HTTP)
    NULL,           // Handle
    0               // Núcleo (evitar core donde corre AsyncTCP)
);
  
  sseTimer = xTimerCreate(
    "SSETimer",
    pdMS_TO_TICKS(5000),
    pdTRUE,
    (void*)0,
    sendSSEData
);
xTimerStart(sseTimer, 0);
            
}

void loop() {
  // 1. MANTENIMIENTO DEL SISTEMA
  checkWiFiConnection(); 
  
  if (otaInProgress) {
      yield(); 
      return; 
  }
  
  // Actualizaciones OTA y reinicio
  if (millis() - lastUpdateCheck >= config.updateOta) {
      stateUpdateCounter = 0;
      updateState();
      //checkForIndexUpdate();
      checkForUpdates();
      loadState();
      lastUpdateCheck = millis();
  }

  if (shouldRestart) {
      Serial.println("🔄 Reiniciando sistema...");
      delay(1000);
      ESP.restart();
  }

  // ✅ ESCANEO MANUAL (Solicitado desde la Web)
  if (scanRequested) {
      Serial.println("🔍 Escaneo solicitado por usuario web...");
      WiFiManager::scanNetworks(networks); 
      scanRequested = false; 
  }

  // 2. GESTIÓN DEL SENSOR (BSEC)
  bool nuevosDatos = readSensorData(); 

  // 3. LÓGICA PRINCIPAL
  
  // CASO A: ONLINE (WiFi + MQTT) ✅
  if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
      
      // A1. Envío en Tiempo Real
      if (nuevosDatos) {
          static unsigned long lastPublish = 0;
          if (millis() - lastPublish >= 3000) {
              publishSensorData(); 
              lastPublish = millis();
              Serial.println("✅ Datos enviados en tiempo real.");
          }
      }

      // A2. Recuperación de Backlog (con espera aleatoria)
      if (SPIFFS.exists(BACKLOG_FILE)) {
          if (!backlogReady && backlogWaitTime == 0) {
              long wait = random(5000, 20000); 
              backlogWaitTime = millis() + wait;
              Serial.printf("⏳ Backlog detectado. Esperando %d ms...\n", wait);
              backlogReady = true;
          }

          if (backlogReady && millis() > backlogWaitTime) {
              writeLog("🚀 Iniciando recuperación de Backlog. Conexión restablecida.");
              processBacklog(); 
              backlogWaitTime = 0; 
              backlogReady = false; 
          }
      }
  } 
  
  // CASO B: OFFLINE ❌
  else {
      // Intentar reconectar MQTT si hay WiFi
      if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
          static unsigned long lastMqttAttempt = 0;
          if (millis() - lastMqttAttempt > 10000) { 
              lastMqttAttempt = millis();
              Serial.println("📡 WiFi OK. Intentando recuperar MQTT...");
              connectToMqtt();
          }
      }

      // B1. Guardar Respaldo
      if (nuevosDatos) {
          struct tm timeinfo;
          getCurrentTime(&timeinfo);

          // // 🧪 MODO PRUEBAS (Corregido)

          // if (timeinfo.tm_min != lastProcessedMinute) {
    
          //     Serial.printf("⏱️ Nuevo minuto detectado (%d). Guardando respaldo...\n", timeinfo.tm_min);
          //     writeLog("⚠️ Offline: Guardando respaldo en SPIFFS. Hora: " + getFormattedDateTime());
          //     lastProcessedMinute = timeinfo.tm_min; // Marcamos como guardado inmediatamente
            
          // 🏭 MODO PRODUCCIÓN (Guardar cada hora en punto)
          
          if (timeinfo.tm_min == 0 && timeinfo.tm_hour != lastProcessedHour) {
              Serial.println("⏱️ Hora en punto detectada. Guardando respaldo...");

              // Registro en el log del sistema
              writeLog("⚠️ Offline: Guardando respaldo en SPIFFS. Hora: " + getFormattedDateTime());
              
              // Actualizamos el candado para que no guarde 20 veces en el mismo minuto
              lastProcessedHour = timeinfo.tm_hour; 

              JsonDocument doc; 
              populateSensorJson(doc); 
              saveToBacklog(doc); 
          }
      }
  }

  checkClockSync();
  yield(); 
}





