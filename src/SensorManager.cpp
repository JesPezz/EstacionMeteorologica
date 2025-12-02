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
bool readSensorData() {
  static unsigned long calibrationStartTime = 0;
  static unsigned long lastUpdateTime = 0;
  const unsigned long estimatedCalibrationTime = 300000; // 5 minutos de calentamiento
  const unsigned long updateInterval = 1000; 
  static int lastPercentage = -1;

  // 1. BSEC decide si es momento de medir (cada 3s en modo LP)
  if (iaqSensor.run()) {
      
      // CASO A: El sensor aún se está estabilizando (Precisión 0)
      if (iaqSensor.iaqAccuracy == 0) {
          if (calibrationStartTime == 0) {
              calibrationStartTime = millis();
              Serial.println("Iniciando calibración... 0%");
          }

          if (millis() - lastUpdateTime >= updateInterval) {
              int progress = (millis() - calibrationStartTime) * 100 / estimatedCalibrationTime;
              progress = constrain(progress, 0, 99);
              
              if (progress != lastPercentage) {
                  Serial.print("Progreso: ");
                  Serial.print(progress);
                  Serial.println("%   ");
                  lastPercentage = progress;
              }
              lastUpdateTime = millis();
          }
          // 🛑 Retornamos false aquí para NO enviar MQTT durante la fase delicada de calentamiento
          // Esto protege la calibración inicial de interferencias.
          return false; 
      } 
      
      // CASO B: El sensor tiene datos válidos (Precisión >= 0 y fase de calentamiento terminada)
      else {
          if (calibrationStartTime != 0) {
              Serial.println("\n✅ Calibración completada");
              calibrationStartTime = 0;
              lastPercentage = -1;
          }
          
          // Construimos el string de salida para Serial
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
          updateState();          // Guardar aprendizaje en NVS si es necesario

          // ✅ RETORNO CLAVE: Avisamos a main.cpp que tenemos datos frescos
          // y que el bus I2C está libre para usar MQTT.
          return true;
      }
  } 
  
  // Si BSEC no midió en este ciclo, verificamos estado pero no hacemos nada más
  else {
      checkIaqSensorStatus();
      return false;
  }
}