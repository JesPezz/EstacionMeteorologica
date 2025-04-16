#include "SensorManager.h"
#include "config.h" 
#include "TimeManager.h"
#include "WiFiManager.h"
#include "GoogleSheetManager.h"
#include "BME_Sensor.h"


void setupBsecSensor() {
  
  // Verificar si el sensor responde en la dirección I2C
  Wire.beginTransmission(BME68X_I2C_ADDR_LOW);
  if (Wire.endTransmission() != 0) {  // Si el sensor no responde, reiniciar I2C
      Serial.println("🔄 Reiniciando I2C...");
      Wire.end();
      Wire.begin(21, 22);
  } else {
      Serial.println("✅ I2C ya estaba iniciado.");
  }

  // Inicializar el sensor BME680
  iaqSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  bsecPrefs.begin("bsec_data", false);
  checkIaqSensorStatus();
   // Esperar 1 segundo para que el sensor se estabilice
   delay(1000);
  Serial.println();
  loadState();
  delay(1000);
  // Verificar si el estado se aplicó correctamente
  if (iaqSensor.iaqAccuracy == 0) {
    Serial.println("⚠ Aún no hay precisión (iaqAccuracy = 0). El sensor puede estar calibrando...");
}
}


// Implementación de checkClockSync
void checkClockSync() {

    unsigned long currentTime = millis() / 1000;
    if (currentTime - lastSyncTime >= MONTH_IN_SECONDS) {
      syncClock();
      lastSyncTime = currentTime;
    }
  }

// Implementación de readSensorData
void readSensorData() {
  static unsigned long calibrationStartTime = 0;
  static unsigned long lastUpdateTime = 0;
  const unsigned long estimatedCalibrationTime = 300000; // 5 minutos
  const unsigned long updateInterval = 1000; // Actualizar cada 1s
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
                  // Alternativa 1: Usar Serial.print() con espacios para "borrar"
                  Serial.print("Progreso: ");
                  Serial.print(progress);
                  Serial.println("%   "); // Espacios adicionales
                  
                  // Alternativa 2: Usar caracteres de control ANSI (solo en terminales compatibles)
                  // Serial.print("\033[2K\rProgreso: "); // Borra línea
                  // Serial.print(progress);
                  // Serial.print("%");
                  
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
          
          // Mostrar datos normales del sensor
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






  