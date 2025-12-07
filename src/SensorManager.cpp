#include "SensorManager.h"
#include "config.h" 
#include "TimeManager.h"
#include "WiFiManager.h"
#include "BME_Sensor.h"

// Variables estáticas para el control de tiempo
static unsigned long lastSyncTime = 0;
const unsigned long MONTH_IN_SECONDS = 30 * 24 * 60 * 60; 

// Array de configuración externa (la llave binaria)
extern const uint8_t bsec_config_iaq[];

void setupBsecSensor() {
  Wire.beginTransmission(BME68X_I2C_ADDR_LOW);
  if (Wire.endTransmission() != 0) {
      Serial.println("🔄 Reiniciando I2C...");
      Wire.end();
      Wire.begin(21, 22);
  } else {
      Serial.println("✅ I2C ya estaba iniciado.");
  }

  // 1. Configurar BSEC antes de iniciar el sensor físico
  Serial.println("\n⏳ Cargando configuración BSEC...");
  iaqSensor.setConfig(bsec_config_iaq);

  // 2. Iniciar el sensor
  iaqSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  checkIaqSensorStatus();

  // 3. Gestión de memoria (NVS)
  bsecPrefs.begin("bsec_data", false);
  
  // ⚠️ IMPORTANTE: Mantener comentada la línea de borrado para no reiniciar el aprendizaje
  // bsecPrefs.clear(); 
  
  delay(1000);
  Serial.println();
  loadState(); // Cargar historial previo de calibración
  delay(1000);
  
  if (iaqSensor.iaqAccuracy == 0) {
    Serial.println("⚠ Aún no hay precisión. El sensor puede estar calibrando...");
  }
}

void checkClockSync() {
    unsigned long currentTime = millis() / 1000;
    if (currentTime - lastSyncTime >= MONTH_IN_SECONDS) {
      syncClock();
      lastSyncTime = currentTime;
    }
}

// 🟢 CAMBIO PRINCIPAL: Ahora devuelve bool para sincronizar MQTT
// En SensorManager.cpp

bool readSensorData() {
  // Solo verificamos si la librería BSEC completó un ciclo de medición
  if (iaqSensor.run()) {
      
      // Construimos el string de salida para Serial (Igual que antes)
      output = String(iaqSensor.iaq);
      output += ", " + String(iaqSensor.iaqAccuracy);
      output += ", " + String(iaqSensor.staticIaq);
      output += ", " + String(iaqSensor.co2Equivalent);
      output += ", " + String(iaqSensor.breathVocEquivalent);
      output += ", " + String(iaqSensor.rawTemperature);
      output += ", " + String(iaqSensor.pressure);
      output += ", " + String(iaqSensor.rawHumidity);
      output += ", " + String(iaqSensor.gasResistance);
      output += ", " + String(iaqSensor.stabStatus);
      output += ", " + String(iaqSensor.runInStatus);
      output += ", " + String(iaqSensor.temperature);
      output += ", " + String(iaqSensor.humidity);
      output += ", " + String(iaqSensor.gasPercentage);
      
      Serial.println(output); // Ver datos en monitor serie
      
      updateState(); // Guardar estado en NVS si corresponde

      // ✅ RETORNAMOS TRUE SIEMPRE QUE HAYA DATOS
      // No importa si accuracy es 0, 1 o 3.
      return true;
  } 
  
  // Si no hubo medición nueva
  else {
      checkIaqSensorStatus();
      return false;
  }
}

void populateSensorJson(JsonDocument& doc) {
    // 1. Datos de Identificación (Copiados de config global)
    doc["location"] = config.location;
    doc["device_id"] = WiFi.macAddress();
    doc["ts_api_key"] = config.thingSpeakAPIKey; // Si aun lo usas
    doc["ts_channel"] = config.channelID;
    
    // Agregar Timestamp actual (importante para datos offline)
    doc["timestamp"] = getFormattedDateTime(); // Usando tu TimeManager

    // 2. Datos del Sensor (Tomados de la variable global iaqSensor)
    doc["temperature"] = iaqSensor.temperature;
    doc["humidity"] = iaqSensor.humidity;
    doc["pressure"] = iaqSensor.pressure / 100.0;
    doc["iaq"] = iaqSensor.iaq;
    
    // Diagnósticos
    doc["iaqAccuracy"] = iaqSensor.iaqAccuracy;
    doc["staticIaq"] = iaqSensor.staticIaq;
    doc["co2Equivalent"] = iaqSensor.co2Equivalent;
    doc["breathVocEquivalent"] = iaqSensor.breathVocEquivalent;
    
    // Datos crudos
    doc["rawTemperature"] = iaqSensor.rawTemperature;
    doc["rawHumidity"] = iaqSensor.rawHumidity;
    doc["gasResistance"] = iaqSensor.gasResistance;
    doc["gasPercentage"] = iaqSensor.gasPercentage;
    doc["stabilizationStatus"] = iaqSensor.stabStatus;
    doc["runInStatus"] = iaqSensor.runInStatus;
}