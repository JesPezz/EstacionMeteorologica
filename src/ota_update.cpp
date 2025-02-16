#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include "web_server.h"
#include "ota_update.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "config.h"


void checkForUpdates() {
    Serial.println("🔍 Verificando nueva versión en GitHub Releases...");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, githubAPIURL);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String jsonResponse = http.getString();  // 📌 Guardar la respuesta en un String
        Serial.println("📜 Respuesta JSON: " + jsonResponse);

        JsonDocument doc;  // 📌 Crear el buffer JSON
        DeserializationError error = deserializeJson(doc, jsonResponse);  // Usar 'doc' en lugar de 'json'
        
        if (error) {
            Serial.println("❌ Error al parsear JSON: " + String(error.c_str()));
            return;
        }

        String newVersion = doc["tag_name"];  // 📌 Extraer versión
        String firmwareURL = doc["assets"][0]["browser_download_url"];  // 📌 Extraer URL del firmware

        Serial.printf("📌 Última versión en GitHub: %s\n", newVersion.c_str());
        Serial.printf("📥 URL del firmware: %s\n", firmwareURL.c_str());

        if (newVersion == version) {
            Serial.println("✅ El ESP32 ya está actualizado.");
            return;
        } else {
            Serial.println("🚀 Nueva versión detectada. Iniciando OTA...");
            downloadAndUpdate(firmwareURL);
        }
    } else {
        Serial.printf("❌ Error HTTP: %d al obtener información de Releases.\n", httpCode);
    }

    http.end();
}

void downloadAndUpdate(String firmwareURL) {
    Serial.println("📥 Descargando firmware desde GitHub...");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, firmwareURL);
    int httpCode = http.GET();

    if (httpCode == 200) {
        int contentLength = http.getSize();
        Serial.printf("📥 Tamaño del firmware: %d bytes\n", contentLength);

        if (contentLength > 0 && contentLength <= ESP.getFreeSketchSpace()) {
            if (!Update.begin(contentLength)) {
                Serial.println("❌ No hay suficiente espacio para actualizar.");
                return;
            }

            WiFiClient *stream = http.getStreamPtr();
            uint8_t buffer[1024];
            size_t written = 0;

            while (written < contentLength) {
                int availableBytes = stream->available();
                if (availableBytes > 0) {
                    int toRead = min(availableBytes, (int)sizeof(buffer));
                    int bytesRead = stream->readBytes(buffer, toRead);
                    if (bytesRead > 0) {
                        written += Update.write(buffer, bytesRead);
                    }
                }
            }

            Serial.printf("📤 Bytes escritos en Flash: %d bytes\n", written);

            if (written == contentLength) {
                if (Update.end(true)) {
                    Serial.println("✅ Firmware actualizado correctamente. Reiniciando...");
                    ESP.restart();
                } else {
                    Serial.println("❌ Error al finalizar la actualización.");
                    Update.printError(Serial);
                }
            } else {
                Serial.println("❌ Error: No se recibió el firmware completo.");
            }
        }
    } else {
        Serial.printf("❌ Error HTTP: %d al descargar firmware.\n", httpCode);
    }

    http.end();
}