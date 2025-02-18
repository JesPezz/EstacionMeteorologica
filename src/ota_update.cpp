#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include "web_server.h"
#include "ota_update.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"
#include "led.h"
#include "notifications.h"
#include "extras/MB_String.h"

NotificationConfig notificationConfig = extractNotificationConfig(config);

void checkForUpdates() {
    Serial.println("🔍 Verificando nueva versión en GitHub Releases...");
    String githubAPIURLString = githubAPIURL.c_str();
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, githubAPIURLString);
    int httpCode = http.GET();

    if (httpCode == 200) {
        MB_String jsonResponse = http.getString();
        MB_String mensaje = "📜 Respuesta JSON: ";
        mensaje += jsonResponse;
        Serial.println(mensaje.c_str());

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonResponse.c_str());

        if (error) {
            Serial.print("❌ Error al parsear JSON: ");
            Serial.println(error.c_str());
            return;
        }

        MB_String newVersion = doc["tag_name"].as<String>();
        MB_String githubAPIURL = doc["assets"][0]["browser_download_url"].as<String>();

        Serial.print("📌 Última versión en GitHub: ");
        Serial.println(newVersion.c_str());
        Serial.print("📥 URL del firmware: ");
        Serial.println(githubAPIURL.c_str());

        if (newVersion == version) {
            Serial.println("✅ El ESP32 ya está actualizado.");
            return;
        } else {
            Serial.println("🚀 Nueva versión detectada. Iniciando OTA...");
            sendTelegramMessage("🔧 Nueva actualización OTA iniciada", notificationConfig);
            sendEmailNotification("Nueva actualización OTA iniciada", notificationConfig);
            downloadAndUpdate();  // Pasar NotificationConfig
        }
    } else {
        Serial.printf("❌ Error HTTP: %d al verificar actualizaciones.\n", httpCode);
    }

    http.end();
}

void downloadAndUpdate() {  // ✅ Eliminamos los parámetros innecesarios
    Serial.println("📥 Descargando firmware desde GitHub...");
    sendTelegramMessage("🔧 Actualización en progreso", notificationConfig);
    sendEmailNotification("Actualización en progreso", notificationConfig);
    ledInProgress();

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, githubAPIURL.c_str());  // ✅ Usamos githubAPIURL global

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        Serial.printf("📥 Tamaño del firmware: %d bytes\n", contentLength);

        if (contentLength > 0 && contentLength <= ESP.getFreeSketchSpace()) {
            if (!Update.begin(contentLength)) {
                Serial.println("❌ No hay suficiente espacio para actualizar.");
                errLeds();
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
                    sendTelegramMessage("✅ Actualización exitosa", notificationConfig);
                    sendEmailNotification("Actualización exitosa", notificationConfig);
                    ledSuccess();
                    ESP.restart();
                } else {
                    Serial.println("❌ Error al finalizar la actualización.");
                    sendTelegramMessage("❌ Error en la actualización", notificationConfig);
                    sendEmailNotification("Error en la actualización", notificationConfig);
                    errLeds();
                    Update.printError(Serial);
                }
            } else {
                Serial.println("❌ Error: No se recibió el firmware completo.");
                errLeds();
            }
        }
    } else {
        Serial.printf("❌ Error HTTP: %d al descargar firmware.\n", httpCode);
        errLeds();
    }

    http.end();  // ✅ Cerrar conexión HTTP
}
