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
#include "esp_ota_ops.h"


const char* host = "raw.githubusercontent.com";
const char* url = "/JesPezz/EstacionMeteorologica/main/Data/index.html";
const char* etagFilePath = "/index_etag.txt";

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
        String githubAPIURL = doc["assets"][0]["browser_download_url"];  // 📌 Extraer URL del firmware

        Serial.printf("📌 Última versión en GitHub: %s\n", newVersion.c_str());
        Serial.printf("📥 URL del firmware: %s\n", githubAPIURL.c_str());

        if (newVersion == version) {
            Serial.println("✅ El ESP32 ya está actualizado.");
            return;
        } else {
            Serial.println("🚀 Nueva versión detectada. Iniciando OTA...");
            sendTelegramMessage("🚀 Nueva versión detectada. Iniciando OTA...", config);
            downloadAndUpdate();
        }
    } else {
        Serial.printf("❌ Error HTTP: %d al obtener información de Releases.\n", httpCode);
    }

    http.end();
}
    
void checkForIndexUpdate() {
    Serial.println("🔍 Verificando actualización de index.html...");

    WiFiClientSecure client;
    client.setInsecure();  // Deshabilita la verificación SSL

    if (!client.connect(host, 443)) {
        Serial.println("❌ Error al conectar con GitHub.");
        return;
    }

    // Enviar solicitud HEAD
    String request = "HEAD ";
    request += url;
    request += " HTTP/1.1\r\nHost: ";
    request += host;
    request += "\r\nUser-Agent: ESP32\r\nConnection: close\r\n\r\n";

    client.print(request);

    // Leer respuesta del servidor
    String response = "";
    String remoteETag = "";
    while (client.connected() || client.available()) {
        String line = client.readStringUntil('\n');
        response += line;
        response += "\n";


        // Buscar y extraer el ETag
        if (line.startsWith("ETag:")) {
            remoteETag = line.substring(6);
            remoteETag.trim();  // Eliminar espacios extra
            remoteETag.replace("\"", "");  // Eliminar comillas
        }
    }
    client.stop();

    if (remoteETag.isEmpty()) {
        Serial.println("❌ No se encontró 'ETag'. No se puede verificar la actualización.");
        return;
    }

    Serial.print("🔖 ETag de GitHub: ");
    Serial.println(remoteETag);



    // Leer el ETag almacenado en SPIFFS
    String localETag = "";
    if (SPIFFS.exists(etagFilePath)) {
        File file = SPIFFS.open(etagFilePath, "r");
        if (file) {
            localETag = file.readString();
            file.close();
        }
    } else {
        Serial.println("⚠️ Archivo index_etag.txt no encontrado. Creando...");
        localETag = "N/A";  // Valor inicial para forzar la primera descarga
    }

    // Comparar ETag remoto con el local
    if (remoteETag != localETag) {
        Serial.println("📥 Nueva versión detectada. Descargando index.html...");
        sendTelegramMessage("📥 Nueva versión detectada. Descargando index.html...", config);
        if (updateFileFromURL(url, "/index.html")) {
            // Guardar el nuevo ETag en SPIFFS
            File file = SPIFFS.open(etagFilePath, "w");
            if (file) {
                file.print(remoteETag);
                file.close();
            }
            Serial.println("✅ index.html actualizado.");
            sendTelegramMessage("✅ index.html actualizado.", config);
        }
    } else {
        Serial.println("✅ index.html ya está actualizado.");
    }
}

bool updateFileFromURL(const char *url2, const char *path) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url2);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        File file = SPIFFS.open(path, "w");
        if (!file) {
            Serial.println("❌ Error al abrir archivo en SPIFFS.");
            sendTelegramMessage("❌ Error al abrir archivo en SPIFFS.", config);
            otaInProgress = false;
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
        sendTelegramMessage("✅ Archivo actualizado desde GitHub.", config);
        return true;
    } else {
        Serial.printf("❌ Error HTTP %d al descargar archivo.\n", httpCode);
        sendTelegramMessage("❌ Error HTTP %d al descargar archivo.\n", config);
        return false;
    }

    http.end();
}

void downloadAndUpdate() {
    Serial.printf("📦 Espacio libre para OTA: %u bytes\n", ESP.getFreeSketchSpace());
    Serial.println("📥 Descargando firmware desde GitHub...");
    sendTelegramMessage("📥 Descargando firmware desde GitHub...", config);

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ Error: No hay conexión WiFi.");
        sendTelegramMessage("❌ Error: No hay conexión WiFi.", config);
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); // ⚠️ Usar certificado raíz válido en producción

    HTTPClient http;
    http.begin(client, githubAPIURL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        Serial.printf("📥 Tamaño del firmware: %d bytes\n", contentLength);

        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        Serial.println("❌ No se encontró una partición OTA válida.");
        sendTelegramMessage("❌ No se encontró una partición OTA válida.", config);
        return;
}


        if (contentLength > 0 && contentLength <= ESP.getFreeSketchSpace()) {
            if (!Update.begin(contentLength, U_FLASH)) {
                Serial.println("❌ No hay suficiente espacio para actualizar.");
                sendTelegramMessage("❌ No hay suficiente espacio para actualizar.", config);
                http.end();
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
                        
                        if (Update.write(buffer, bytesRead) != bytesRead) {
                            Serial.printf("❌ Error al escribir en flash. Bytes escritos: %d de %d\n", Update.write(buffer, bytesRead), bytesRead);
                            sendTelegramMessage("❌ Error al escribir en la memoria flash.", config);
                            Update.abort();
                            http.end();
                            return;
                        }
                        
                        written += bytesRead;
                    }
                }
            }

            Serial.printf("📤 Bytes escritos en Flash: %d bytes\n", written);
            sendTelegramMessage("📤 Bytes escritos en Flash: " + String(written) + " bytes", config);

            if (written == contentLength) {
                if (Update.end()) {
                    Serial.println("✅ Firmware actualizado correctamente. Reiniciando...");
                    sendTelegramMessage("✅ Firmware actualizado correctamente. Reiniciando...", config);
                    http.end();
                    ESP.restart();
                } else {
                    Serial.println("❌ Error al finalizar la actualización.");
                    sendTelegramMessage("❌ Error al finalizar la actualización.", config);
                    Update.printError(Serial);
                }
            } else {
                Serial.println("❌ Error: No se recibió el firmware completo.");
                sendTelegramMessage("❌ Error: No se recibió el firmware completo.", config);
            }
        } else {
            Serial.println("❌ Error: Tamaño de firmware inválido o insuficiente espacio.");
            sendTelegramMessage("❌ Error: Tamaño de firmware inválido o insuficiente espacio.", config);
        }
    } else {
        Serial.printf("❌ Error HTTP: %d al descargar firmware.\n", httpCode);
        sendTelegramMessage("❌ Error HTTP: " + String(httpCode) + " al descargar firmware.", config);
    }

    http.end();
}