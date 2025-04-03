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
#include "GoogleSheetManager.h"
#include "ThingSpeakManager.h"
#include "TimeManager.h"
#include <bme68xLibrary.h>
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

void setup() {

  Serial.begin(115200);
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ Error al montar SPIFFS");
        return;
    }
    Serial.println("✅ SPIFFS montado correctamente");


  Serial.println("📂 Verificando existencia de /index.html...");
if (SPIFFS.exists("/index.html")) {
    Serial.println("✅ Archivo encontrado.");
} else {
    Serial.println("❌ El archivo no existe en SPIFFS.");
}



EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1);

printWiFiNetwork();
if (!otaInProgress) {  // 🔹 Evitar iniciar procesos si hay OTA en curso
  Serial.println("✅ Iniciando procesos después de OTA...");
  WiFi.begin("SSID", "PASSWORD");
}


loadConfig();
  
  esp_partition_t *runningPartition = (esp_partition_t *)esp_ota_get_running_partition();
  Serial.printf("🔍 Ejecutando desde la partición: %s\n", runningPartition->label);

  const esp_partition_t* running = esp_ota_get_running_partition();
  Serial.printf("📌 Arrancando desde la partición: %s\n", running->label);

  // Obtener el tamaño total de la Flash
  Serial.printf("📦 Tamaño total de la Flash: %u bytes (%.2f MB)\n", ESP.getFlashChipSize(), ESP.getFlashChipSize() / (1024.0 * 1024.0));

  // Obtener el tamaño de la partición OTA
  Serial.printf("📦 Tamaño de la partición actual: %u bytes (%.2f MB)\n", ESP.getSketchSize(), ESP.getSketchSize() / (1024.0 * 1024.0));
  Serial.printf("📦 Espacio libre para OTA: %u bytes (%.2f MB)\n", ESP.getFreeSketchSpace(), ESP.getFreeSketchSpace() / (1024.0 * 1024.0));

  testFlash();

  
  setupBsecSensor();

  if (!loadConfig()) {
    Serial.println("No hay configuración guardada. Iniciando en modo AP...");
}
startAPMode();

// ✅ Verificar que los valores cargados sean correctos
Serial.println("📜 CONFIGURACIÓN CARGADA DESDE config.json:");
Serial.println("Google Sheet URL: " + config.googleSheetURL);
Serial.println("ThingSpeak API Key: " + config.thingSpeakAPIKey);
Serial.println("updateOta: " + String(config.updateOta / 3600000) + " Horas");
Serial.println("Channel ID: " + String(config.channelID));
Serial.println("Location: " + config.location);
Serial.println("chatId: " + config.chatId);
Serial.println("telegramToken: " + config.telegramToken);

// scanTicker.attach(15.0, triggerNetworkScan);
WiFiManager::scanNetworks(networks);
initWiFiScanner();
initSensorMutex();
startWebServer();
initSSETimer();
printConfig();  // ✅ Ver los valores actuales de configuración

  // Definir el nombre del código y la ubicación
  Serial.print("Version: ");
  Serial.print(nombreCodigo);
  Serial.print(" ");
  Serial.println(version);
  Serial.println("Ubicacion: " + config.location);
  pinMode(LED_BUILTIN, OUTPUT);
  iaqSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  Serial.println(output);
  checkIaqSensorStatus();

  loadState();
  Serial.println("loadState() se ha cargado.");
  
  // Sincronizar el reloj una vez al mes
  syncClock();
  sendTelegramMessage("ℹ️ Estado del ESP32", config);


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
  checkForIndexUpdate();
  checkForUpdates();
  listSPIFFS();


  // Imprimir el encabezado
  output = "Timestamp [ms], IAQ, IAQ accuracy, Static IAQ, CO2 equivalent, breath VOC equivalent, raw temp[°C], pressure [hPa], raw relative humidity [%], gas [Ohm], Stab Status, run in status, comp temp[°C], comp humidity [%], gas percentage";
  Serial.println(output);

  xTaskCreatePinnedToCore(
    wifiScanTask,    // Función
    "WiFiScanner",   // Nombre
    4096,           // Stack size (suficiente para JSON)
    NULL,           // Parámetros
    1,              // Prioridad (1 = baja, debajo de WiFi/HTTP)
    NULL,           // Handle
    0               // Núcleo (evitar core donde corre AsyncTCP)
);

Serial.println("[WiFi] Escáner inicializado en Core 0");



  // Iniciar tarea FreeRTOS para enviar datos a ThingSpeak
  xTaskCreatePinnedToCore(
    taskSendToThingSpeak,   // Función que ejecutará la tarea
    "SendToThingSpeak",     // Nombre de la tarea
    4096,                   // Tamaño de la pila de la tarea
    NULL,                   // Parámetros de la tarea
    1,                      // Prioridad de la tarea
    &thingSpeakTaskHandle,  // Manejador de la tarea
    0                       // Núcleo en el que se ejecutará la tarea (núcleo 1)
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
  checkWiFiConnection(); // Verificar la conexión WiFi y reconectar si es necesario
  yield();
  if (millis() - lastUpdateCheck >= config.updateOta) {
        stateUpdateCounter = 0;  // Restablecer el contador
        updateState();  // Llamar a la función
        checkForIndexUpdate();
        checkForUpdates();
        loadState();
        Serial.println("loadState() se ha cargado.");
        lastUpdateCheck = millis();
    }

    if (otaInProgress) {
      yield(); // Alimenta el WDT
      return;  // 🔹 Si la OTA está en proceso, no ejecutamos nada más
  }
   
  readSensorData();      // Leer datos del sensor
  
  // Enviar datos a Google Sheets en el intervalo normal (usando isHourOnTheDot)
  bool currentMinuteZero = isHourOnTheDot();
  if (!prevMinuteZero && currentMinuteZero) {
    googlesheet(); // Envía los datos a Google Sheets
  }
  prevMinuteZero = currentMinuteZero;

  // //Enviar datos a Google Sheets en el intervalo de prueba (usando millis)
  // static unsigned long lastUploadTime = 0;
  // if (millis() - lastUploadTime >= 30000) { // 30000 ms = .5 minutes
  //   googlesheet();
  //   lastUploadTime = millis();
  // }

  checkClockSync();
}





