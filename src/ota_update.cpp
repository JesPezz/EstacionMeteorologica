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
#include "esp_task_wdt.h"

// 🔹 Deshabilitar el Watchdog antes de la OTA
void disableWatchdog() {
    Serial.println("🛑 Desactivando Watchdog y activando modo OTA...");
    otaInProgress = true;  // Indicar que la OTA está en curso
    esp_task_wdt_delete(NULL);  // Desactiva el Watchdog

    // 📤 Enviar notificación a Telegram
    sendTelegramMessage("🛑 OTA iniciada: Suspendiendo procesos y Watchdog.", config);
}

void enableWatchdog() {
    Serial.println("✅ Reactivando Watchdog y reanudando procesos...");
    otaInProgress = false;  // Indicar que la OTA ha finalizado
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);

    // 📤 Enviar notificación a Telegram
    sendTelegramMessage("✅ OTA finalizada: Reactivando procesos y Watchdog.", config);
}

// 🔹 Obtener la URL del firmware desde la API de GitHub
String getFirmwareURL() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, githubAPIURL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("📜 Respuesta JSON: " + payload);

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.println("❌ Error al parsear JSON");
            Serial.println("Error: " + String(error.c_str()));
            return "";
        }

        JsonObject json = doc.as<JsonObject>();

        if (!json["assets"].isNull() && json["assets"].size() > 0) {
            String firmwareURL = json["assets"][0]["browser_download_url"].as<String>();
            
            Serial.println("📥 URL del firmware: " + firmwareURL);
            return firmwareURL;
        }
    } else {
        Serial.printf("❌ Error HTTP al obtener URL del firmware. Código: %d\n", httpCode);
        writeLog("❌ Error HTTP al obtener URL del firmware. Código: " + String(httpCode));
        
    }
    return"";
}

// 🔹 Seguir redirecciones para obtener la URL final del firmware
String getFinalURL(String initialURL) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, initialURL);
    
    int httpCode = http.GET();
    if (httpCode == 302) { // Si hay redirección
        String newURL = http.getLocation();
        http.end();
        return newURL;
    }

    http.end();
    return initialURL; // Si no hay redirección, usa la original
}

void checkForUpdates() {
    Serial.println("🔍 Verificando nueva versión en GitHub Releases...");

    WiFiClientSecure client;
    client.setInsecure();
    client.stop();

    HTTPClient http;
    http.begin(client, githubAPIURL);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String jsonResponse = http.getString();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonResponse);
        
        if (error) {
            Serial.println("❌ Error al parsear JSON: " + String(error.c_str()));
            writeLog("❌ Error al parsear JSON: " + String(error.c_str()));
            return;
        }

        String newVersion = doc["tag_name"];
        // IMPORTANTE: Aquí extraemos la URL de descarga para usarla luego si es necesario
        // aunque getFirmwareURL la vuelve a pedir, es bueno tenerla o loguearla.
        String downloadURL = doc["assets"][0]["browser_download_url"];

        Serial.printf("📌 Última versión en GitHub: %s\n", newVersion.c_str());
        Serial.printf("📥 URL del firmware: %s\n", downloadURL.c_str());
        
        if (newVersion == version) {
            Serial.println("✅ El ESP32 ya está actualizado.");
            return;
         } else {
            Serial.println("🚀 Nueva versión detectada. Iniciando OTA...");
            Serial.printf("📦 Espacio libre para OTA: %u bytes\n", ESP.getFreeSketchSpace());
            sendTelegramMessage("🚀 Nueva versión detectada. Iniciando OTA...", config);
            
            // Liberar algo de memoria si es posible antes de empezar
            heap_caps_free(heap_caps_malloc(1, MALLOC_CAP_8BIT));

            downloadAndUpdate();
         }
        
    } else {
           Serial.printf("❌ Error HTTP: %d al obtener información de Releases.\n", httpCode);
           writeLog("❌ Error HTTP: " + String(httpCode) + " al obtener información de Releases.");
    }

    http.end();
}

void checkForIndexUpdate() {
    if (otaInProgress) {
        return;
    }

    Serial.println("🔍 Verificando actualización de index.html...");

    WiFiClientSecure client;
    client.setInsecure();
    client.stop();

    if (!client.connect(host, 443)) {
        Serial.println("❌ Error al conectar con GitHub.");
        writeLog("❌ Error al conectar con GitHub.");
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
        response += line + "\n";

        if (line.startsWith("ETag:")) {
            remoteETag = line.substring(6);
            remoteETag.trim();
            remoteETag.replace("\"", "");
        }
    }
    client.stop();

    if (remoteETag.isEmpty()) {
        Serial.println("❌ No se encontró 'ETag'. No se puede verificar la actualización.");
        writeLog("❌ No se encontró 'ETag'. No se puede verificar la actualización.");
        return;
    }

    Serial.print("🔖 ETag de GitHub: ");
    Serial.println(remoteETag);

    String localETag = "";
    if (SPIFFS.exists(etagFilePath)) {
        File file = SPIFFS.open(etagFilePath, "r");
        if (file) {
            localETag = file.readString();
            localETag.trim();
            file.close();
        }
    } else {
        Serial.println("⚠️ Archivo index_etag.txt no encontrado. Creando...");
        localETag = "N/A";
    }

    if (remoteETag != localETag) {
        Serial.println("📥 Nueva versión detectada. Descargando index.html...");
        sendTelegramMessage("📥 Nueva versión detectada. Descargando index.html...", config);
    
        if (updateFileFromURL(indexURL, "/index.html")) {
            File file = SPIFFS.open(etagFilePath, "w");
            if (file) {
                file.print(remoteETag);
                file.close();
            }
            Serial.println("✅ index.html actualizado.");
            sendTelegramMessage("✅ index.html actualizado.", config);
            enableWatchdog();
        } else {
            Serial.println("❌ Error al actualizar index.html.");
            writeLog("❌ Error al actualizar index.html.");
            sendTelegramMessage("❌ Error al actualizar index.html.", config);
        }
    } else {
        Serial.println("✅ index.html ya está actualizado.");
    }
}

bool updateFileFromURL(const char* url, const char* path) {
    disableWatchdog();
    
    WiFiClientSecure client;
    client.setInsecure();
    client.stop();

    HTTPClient http;
    Serial.println("🌐 Descargando archivo desde: " + String(url));
    
    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        File file = SPIFFS.open(path, "w");
        if (!file) {
            Serial.println("❌ Error al abrir archivo en SPIFFS.");
            writeLog("❌ Error al abrir archivo en SPIFFS: " + String(path));
            sendTelegramMessage("❌ Error al abrir archivo en SPIFFS.", config);
            http.end();
            enableWatchdog();
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
        
        http.end();
        enableWatchdog();
        return true;
    } else {
        Serial.printf("❌ Error HTTP %d al descargar archivo.\n", httpCode);
        writeLog("❌ Error HTTP " + String(httpCode) + " al descargar archivo.");
        sendTelegramMessage("❌ Error HTTP " + String(httpCode) + " al descargar archivo.", config);
        
        http.end();
        enableWatchdog();
        return false;
    }
}

void downloadAndUpdate() {
    disableWatchdog();

    String firmwareURL = getFirmwareURL();
    if (firmwareURL == "") {
        Serial.println("❌ No se pudo obtener la URL del firmware.");
        sendTelegramMessage("❌ No se pudo obtener la URL del firmware.", config);
        enableWatchdog();
        return;
    }

    firmwareURL = getFinalURL(firmwareURL);
    Serial.println("🔗 URL final del firmware: " + firmwareURL);

    Serial.printf("📦 Espacio libre para OTA: %u bytes\n", ESP.getFreeSketchSpace());
    Serial.println("📥 Descargando firmware...");
    sendTelegramMessage("📥 Descargando firmware desde GitHub...", config);

    WiFiClientSecure client;
    client.setInsecure();
    
    HTTPClient http;
    http.begin(client, firmwareURL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        Serial.printf("📥 Tamaño del firmware: %d bytes\n", contentLength);

        if (contentLength < 100000) { // Ajustado umbral mínimo
            Serial.println("❌ Tamaño del firmware demasiado pequeño. Reintentando...");
            writeLog("❌ Tamaño del firmware demasiado pequeño.");
            http.end();
            enableWatchdog();
            return;
        }

        if (!Update.begin(contentLength, U_FLASH)) {
            Serial.println("❌ Error al iniciar la actualización.");
            writeLog("❌ Error al iniciar la actualización.");
            sendTelegramMessage("❌ Error al iniciar la actualización.", config);
            http.end();
            enableWatchdog();
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
                        Serial.println("❌ Error al escribir en flash.");
                        writeLog("❌ Error al escribir en flash.");
                        sendTelegramMessage("❌ Error al escribir en la memoria flash.", config);
                        Update.abort();
                        http.end();
                        enableWatchdog();
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
                enableWatchdog();
                ESP.restart();
            } else {
                Serial.println("❌ Error al finalizar la actualización.");
                writeLog("❌ Error al finalizar la actualización.");
                sendTelegramMessage("❌ Error al finalizar la actualización.", config);
                Update.printError(Serial);
                enableWatchdog();
            }
        } else {
            Serial.println("❌ Error: No se recibió el firmware completo.");
            writeLog("❌ Error: No se recibió el firmware completo.");
            sendTelegramMessage("❌ Error: No se recibió el firmware completo.", config);
            enableWatchdog();
        }
    } else {
        Serial.printf("❌ Error HTTP: %d al descargar firmware.\n", httpCode);
        writeLog("❌ Error HTTP: " + String(httpCode) + " al descargar firmware.");
        sendTelegramMessage("❌ Error HTTP: " + String(httpCode) + " al descargar firmware.", config);
        enableWatchdog();
    }

    http.end();
}