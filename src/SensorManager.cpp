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
  checkIaqSensorStatus();
  loadState();
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

   unsigned long time_trigger = millis();
    if (iaqSensor.run()) {
      output = String(time_trigger);
      output += ", " + String(iaqSensor.iaq);
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
    } else {
      checkIaqSensorStatus();
}
}





  