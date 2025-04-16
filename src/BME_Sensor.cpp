#include "BME_Sensor.h"
#include "config.h"
#include <Preferences.h>
#include <EEPROM.h>
#include "led.h"
#include <bsec.h>

#define STATE_SAVE_PERIOD 360  // Intervalo en minutos (360 = 6 horas)
byte lastStoredAccuracy = 0xFF; // Valor inicial inválido
Preferences bsecPrefs;

// Implementa checkIaqSensorStatus, loadState, updateState, etc. (copiar código original)

void checkIaqSensorStatus(void)
{
  if (iaqSensor.bsecStatus != BSEC_OK) {
    if (iaqSensor.bsecStatus < BSEC_OK) {
      output = "BSEC error code : " + String(iaqSensor.bsecStatus);
      Serial.println();
      Serial.println(output);
      Serial.println();
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BSEC warning code : " + String(iaqSensor.bsecStatus);
      Serial.println();
      Serial.println(output);
      Serial.println();
    }
  }

  if (iaqSensor.bme68xStatus != BME68X_OK) {
    if (iaqSensor.bme68xStatus < BME68X_OK) {
      output = "BME68X error code : " + String(iaqSensor.bme68xStatus);
      Serial.println();
      Serial.println(output);
      Serial.println();
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BME68X warning code : " + String(iaqSensor.bme68xStatus);
      Serial.println();
      Serial.println(output);
      Serial.println();
    }
  }
}

void printHexDump(const uint8_t* data, size_t size, uint8_t bytesPerLine = 16) {
  for (size_t i = 0; i < size; i += bytesPerLine) {
    // Dirección
    Serial.printf("%04X: ", i);
    
    // Bytes en HEX
    for (size_t j = 0; j < bytesPerLine; j++) {
      if (i + j < size) {
        Serial.printf("%02X ", data[i + j]);
      } else {
        Serial.print("   ");
      }
    }
    
    // Caracteres ASCII (si son imprimibles)
    Serial.print(" ");
    for (size_t j = 0; j < bytesPerLine; j++) {
      if (i + j < size) {
        uint8_t c = data[i + j];
        Serial.write((c >= 32 && c <= 126) ? c : '.');
      }
    }
    Serial.println();
    if (i + bytesPerLine >= size) break;
  }
}

void loadState() {
  bsecPrefs.begin("bsec_data", true); // Modo lectura
  
  if (bsecPrefs.isKey("state")) {
      size_t stateSize = bsecPrefs.getBytes("state", bsecState, BSEC_MAX_STATE_BLOB_SIZE);
      if (stateSize == BSEC_MAX_STATE_BLOB_SIZE) {
          Serial.println("\n✅ Estado de calibración cargado desde NVS");
          Serial.printf("📦 Tamaño: %d bytes\n", stateSize);
          Serial.println("📝 Primeros 32 bytes (hexdump):");
          printHexDump(bsecState, 32); // Muestra solo los primeros 32 bytes
          iaqSensor.setState(bsecState);  // Aplica el estado directamente
          Serial.println("✅ Estado aplicado al sensor BSEC");
          checkIaqSensorStatus();
      } else {
          Serial.printf("⚠ Tamaño incorrecto: %d (esperado %d)\n", stateSize, BSEC_MAX_STATE_BLOB_SIZE);
      }
  } else {
      Serial.println("⚠ No hay estado guardado en NVS. Iniciando calibración desde cero.");
  }
  
  bsecPrefs.end();
}

void updateState() {
  bool shouldUpdate = false;
  byte currentAccuracy = iaqSensor.iaqAccuracy;

  // 1. Guardar en cambios de precisión
  if ((lastStoredAccuracy != currentAccuracy) && 
      (currentAccuracy == 1 || currentAccuracy == 2 || currentAccuracy == 3)) {
      shouldUpdate = true;
      Serial.printf("\n🔄 Cambio de precisión %d→%d\n", lastStoredAccuracy, currentAccuracy);
      lastStoredAccuracy = currentAccuracy;
  }

  // 2. Guardado periódico
  if (currentAccuracy >= 3) {
      if ((stateUpdateCounter * STATE_SAVE_PERIOD * 60000UL) < millis()) {
          shouldUpdate = true;
          stateUpdateCounter++;
          Serial.println("\n⏰ Guardado periódico programado");
      }
  }

  if (shouldUpdate) {
      iaqSensor.getState(bsecState);
      checkIaqSensorStatus();

      Serial.println("💾 Guardando estado actual:");
      Serial.printf("🔢 Precisión: %d\n", currentAccuracy);
      Serial.println("📝 Primeros 32 bytes a guardar:");
      printHexDump(bsecState, 32);

      bsecPrefs.begin("bsec_data", false);
      bool saveResult = bsecPrefs.putBytes("state", bsecState, BSEC_MAX_STATE_BLOB_SIZE);
      bsecPrefs.end();

      if (saveResult) {
          Serial.println("✅ Guardado en NVS exitoso");
          //uploadCalibrationToServer(); // Descomenta para subir al servidor
      } else {
          Serial.println("❌ Error al guardar en NVS");
      }
  }
}
