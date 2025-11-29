#include "SensorManager.h"
#include "config.h" 
#include "TimeManager.h"
#include "WiFiManager.h"
#include "BME_Sensor.h"

// 🟢 CORRECCIÓN: Definimos las variables de tiempo AQUÍ como static (locales a este archivo)
// Esto soluciona el error de "not declared" inmediatamente.
static unsigned long lastSyncTime = 0;
const unsigned long MONTH_IN_SECONDS = 30 * 24 * 60 * 60; 

void setupBsecSensor() {
  Wire.beginTransmission(BME68X_I2C_ADDR_LOW);
  if (Wire.endTransmission() != 0) {
      Serial.println("🔄 Reiniciando I2C...");
      Wire.end();
      Wire.begin(21, 22);
  } else {
      Serial.println("✅ I2C ya estaba iniciado.");
  }

  iaqSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  bsecPrefs.begin("bsec_data", false);
  checkIaqSensorStatus();
  delay(1000);
  Serial.println();
  loadState();
  delay(1000);
  if (iaqSensor.iaqAccuracy == 0) {
    Serial.println("⚠ Aún no hay precisión. El sensor puede estar calibrando...");
  }
}

void checkClockSync() {
    unsigned long currentTime = millis() / 1000;
    // Ahora usa las variables estáticas definidas arriba
    if (currentTime - lastSyncTime >= MONTH_IN_SECONDS) {
      syncClock();
      lastSyncTime = currentTime;
    }
}

void readSensorData() {
  static unsigned long calibrationStartTime = 0;
  static unsigned long lastUpdateTime = 0;
  const unsigned long estimatedCalibrationTime = 300000; 
  const unsigned long updateInterval = 1000; 
  static int lastPercentage = -1;

  if (iaqSensor.run()) {
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
      } else {
          if (calibrationStartTime != 0) {
              Serial.println("\n✅ Calibración completada");
              calibrationStartTime = 0;
              lastPercentage = -1;
          }
          
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
          Serial.println(output);
          updateState();
      }
  } else {
      checkIaqSensorStatus();
  }
}