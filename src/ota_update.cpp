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
            sendEmailNotification("🚀 Nueva versión detectada. Iniciando OTA...", config);
            sendTelegramMessage("🚀 Nueva versión detectada. Iniciando OTA...", config);
            downloadAndUpdate();  // Pasar NotificationConfig
        }
    } else {
        Serial.printf("❌ Error HTTP: %d al verificar actualizaciones.\n", httpCode);
    }

    http.end();
}

void checkForIndexUpdate() {
    Serial.println("🔍 Verificando actualización de index.html...");

    WiFiClientSecure client;
    client.setInsecure();  // Permite conexiones HTTPS sin verificación de certificado

    HTTPClient http;
    http.begin(client, indexURL);
    http.addHeader("User-Agent", "ESP32");  // GitHub bloquea requests sin User-Agent

    int httpCode = http.sendRequest("HEAD");  // Solo pedimos las cabeceras
    if (httpCode == HTTP_CODE_OK) {
        String remoteLastModified = http.header("Last-Modified");  // Obtener fecha de modificación
        String mensaje = "📅 Última modificación en GitHub: ";
        mensaje += remoteLastModified;
        Serial.println(mensaje);



        // Leer la fecha almacenada localmente
        File file = SPIFFS.open(lastModifiedPath, "r");
        String localLastModified;
        if (file) {
            localLastModified = file.readString();
            file.close();
        }

        // Comparar fechas
        if (remoteLastModified != localLastModified) {
            Serial.println("📥 Nueva versión detectada. Descargando...");
            if (updateFileFromURL(indexURL, "/index.html")) {
                // Guardar la nueva fecha
                File outFile = SPIFFS.open(lastModifiedPath, "w");
                if (outFile) {
                    outFile.print(remoteLastModified);
                    outFile.close();
                }
                Serial.println("✅ index.html actualizado.");
            }
        } else {
            Serial.println("✅ index.html ya está actualizado.");
        }
    } else {
        Serial.printf("❌ Error HTTP %d al verificar index.html\n", httpCode);
    }

    http.end();
}

bool updateFileFromURL(const char *url, const char *path) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        File file = SPIFFS.open(path, "w");
        if (!file) {
            Serial.println("❌ Error al abrir archivo en SPIFFS.");
            return false;
        }

        WiFiClient *stream = http.getStreamPtr();
        uint8_t buffer[512];
        int bytesRead;
        while ((bytesRead = stream->readBytes(buffer, sizeof(buffer))) > 0) {
            file.write(buffer, bytesRead);
        }

        file.close();
        Serial.println("✅ Archivo actualizado desde GitHub.");
        return true;
    } else {
        Serial.printf("❌ Error HTTP %d al descargar archivo.\n", httpCode);
        return false;
    }

    http.end();
}

void downloadAndUpdate() {  // ✅ Eliminamos los parámetros innecesarios
    Serial.println("📥 Descargando firmware desde GitHub...");
    sendTelegramMessage("📥 Descargando firmware desde GitHub...", config);
    sendEmailNotification("📥 Descargando firmware desde GitHub...", config);
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
                    sendTelegramMessage("✅ Firmware actualizado correctamente. Reiniciando...", config);
                    sendEmailNotification("✅ Firmware actualizado correctamente. Reiniciando...", config);
                    ledSuccess();
                    ESP.restart();
                } else {
                    Serial.println("❌ Error al finalizar la actualización.");
                    sendTelegramMessage("❌ Error al finalizar la actualización.", config);
                    sendEmailNotification("❌ Error al finalizar la actualización.", config);
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
