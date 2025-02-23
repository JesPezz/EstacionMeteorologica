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

void setup() {
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1);
  Serial.begin(115200);

  if (!otaInProgress) {  // 🔹 Evitar iniciar procesos si hay OTA en curso
    Serial.println("✅ Iniciando procesos después de OTA...");
    WiFi.begin("SSID", "PASSWORD");
    
}
  
initSPIFFS();
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

// ✅ Verificar que los valores cargados sean correctos
Serial.println("📜 CONFIGURACIÓN CARGADA DESDE config.json:");
Serial.println("SSID: " + config.ssid);
Serial.println("Password: " + config.password);
Serial.println("Google Sheet URL: " + config.googleSheetURL);
Serial.println("ThingSpeak API Key: " + config.thingSpeakAPIKey);
Serial.println("updateOta: " + String(config.updateOta / 1000) + " Horas");
Serial.println("Channel ID: " + String(config.channelID));
Serial.println("Location: " + config.location);
Serial.println("chatId: " + config.chatId);
Serial.println("telegramToken: " + config.telegramToken);

connectToWiFi();
startWebServer();

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

  // Imprimir el encabezado
  output = "Timestamp [ms], IAQ, IAQ accuracy, Static IAQ, CO2 equivalent, breath VOC equivalent, raw temp[°C], pressure [hPa], raw relative humidity [%], gas [Ohm], Stab Status, run in status, comp temp[°C], comp humidity [%], gas percentage";
  Serial.println(output);

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
            
}

void loop() {


//   unsigned long currentMillis = millis();  // Obtener tiempo actual

//   Serial.printf("⏳ Tiempo desde última verificación: %lu ms\n", currentMillis - lastUpdateCheck);
//   Serial.printf("🕒 Intervalo OTA configurado: %lu ms\n", config.updateOta);

//   if (currentMillis - lastUpdateCheck >= config.updateOta) {  // Comparar con el intervalo
//       lastUpdateCheck = currentMillis;  // Actualizar última verificación

//       Serial.println("🔍 Verificando nueva versión en GitHub Releases...");
//       checkForFirmwareUpdate();

//       Serial.println("🔍 Verificando actualización de index.html...");
//       checkForIndexUpdate();
//   }

//   delay(100);  // Pequeño retraso para evitar consumo innecesario de CPU
// }

  if (millis() - lastUpdateCheck >= config.updateOta) {
    Serial.printf("✅ updateOta cargado: %lu ms\n", config.updateOta);
        checkForUpdates();
        checkForIndexUpdate();
        loadState();
        Serial.println("loadState() se ha cargado.");
        lastUpdateCheck = millis();
    }

    if (otaInProgress) {
      yield(); // Alimenta el WDT
      return;  // 🔹 Si la OTA está en proceso, no ejecutamos nada más
  }
   
  checkWiFiConnection(); // Verificar la conexión WiFi
  readSensorData();      // Leer datos del sensor
  sendDataToServices();  // Enviar datos a ThingSpeak y Google Sheets
  checkClockSync();      // Sincronizar el reloj si es necesario
}





