#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include "web_server.h"
#include "ota_update.h"

const String FIRMWARE_VERSION = "1.0";  // Cambia esto en cada versión nueva
const String FIRMWARE_URL = "http://tu-servidor.com/firmware.bin";  // URL del nuevo firmware

void checkForUpdates() {
    Serial.println("🔍 Buscando actualización...");

    HTTPClient http;
    http.begin(FIRMWARE_URL);
    int httpCode = http.GET();

    if (httpCode == 200) {
        int contentLength = http.getSize();
        Serial.printf("📥 Tamaño del firmware: %d bytes\n", contentLength);

        if (contentLength > 0) {
            if (!Update.begin(contentLength)) {
                Serial.println("❌ Error al iniciar actualización: No hay espacio suficiente");
                return;
            }

            WiFiClient *stream = http.getStreamPtr();
            size_t written = Update.writeStream(*stream);
            Serial.printf("📤 Bytes escritos en Flash: %d bytes\n", written);

            if (written == contentLength) {
                if (Update.end(true)) {  // true para verificar el checksum
                    Serial.println("✅ Firmware descargado completamente. Reiniciando...");
                    ESP.restart();
                } else {
                    Serial.println("❌ Error al finalizar la actualización");
                }
            } else {
                Serial.println("❌ Error: No se descargó todo el firmware");
            }
        } else {
            Serial.println("❌ Tamaño de firmware inválido");
        }
    } else {
        Serial.printf("❌ Error al descargar firmware. Código HTTP: %d\n", httpCode);
    }

    http.end();
}