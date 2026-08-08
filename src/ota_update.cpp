#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include "web_server.h"
#include "ota_update.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"
#include "led_task.h"
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
    // Verificar conectividad WiFi antes de intentar HTTP
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ No hay conexión WiFi. Abortando petición a GitHub.");
        writeLog("❌ No hay conexión WiFi. Abortando petición a GitHub.");
        return "";
    }

    WiFiClientSecure client;
    client.setInsecure(); // Nota: preferible usar setCACert() en producción si se dispone del CA
    client.setTimeout(10); // timeout en segundos para operaciones de cliente TLS

    HTTPClient http;
    http.setTimeout(10000); // Timeout explícito en ms (10s)
    
    String apiURL = getTestMode() 
        ? "https://api.github.com/repos/JesPezz/EstacionMeteorologica/releases?per_page=1" 
        : githubAPIURL;
    http.begin(client, apiURL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("📜 Respuesta JSON: " + payload);

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.println("❌ Error al parsear JSON");
            Serial.println("Error: " + String(error.c_str()));
            http.end();
            return "";
        }

        JsonObject json;
        if (getTestMode()) {
            JsonArray arr = doc.as<JsonArray>();
            if (arr.size() > 0) {
                json = arr[0].as<JsonObject>();
            } else {
                http.end();
                return "";
            }
        } else {
            json = doc.as<JsonObject>();
        }

        if (!json["assets"].isNull() && json["assets"].size() > 0) {
            String firmwareURL = json["assets"][0]["browser_download_url"].as<String>();
            
            Serial.println("📥 URL del firmware: " + firmwareURL);
            http.end();
            return firmwareURL;
        }
    } else {
        Serial.printf("❌ Error HTTP al obtener URL del firmware. Código: %d\n", httpCode);
        writeLog("❌ Error HTTP al obtener URL del firmware. Código: " + String(httpCode));
        http.end();
        
    }
    return "";
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

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi no conectado. Omitiendo verificación de Releases.");
        writeLog("❌ WiFi no conectado. Omitiendo verificación de Releases.");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Preferible usar certificados CA en producción
    client.setTimeout(10);

    HTTPClient http;
    http.setTimeout(10000);
    String apiURL = getTestMode() 
        ? "https://api.github.com/repos/JesPezz/EstacionMeteorologica/releases?per_page=1" 
        : githubAPIURL;
    http.begin(client, apiURL);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String jsonResponse = http.getString();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonResponse);
        
        if (error) {
            Serial.println("❌ Error al parsear JSON: " + String(error.c_str()));
            writeLog("❌ Error al parsear JSON: " + String(error.c_str()));
            http.end();
            return;
        }

        JsonObject json;
        if (getTestMode()) {
            JsonArray arr = doc.as<JsonArray>();
            if (arr.size() > 0) {
                json = arr[0].as<JsonObject>();
            } else {
                http.end();
                return;
            }
        } else {
            json = doc.as<JsonObject>();
        }

        String newVersion = json["tag_name"];
        String downloadURL = json["assets"][0]["browser_download_url"];

        Serial.printf("📌 Última versión en GitHub: %s\n", newVersion.c_str());
        Serial.printf("📥 URL del firmware: %s\n", downloadURL.c_str());
        
        if (newVersion == version) {
            Serial.println("✅ El ESP32 ya está actualizado.");
            http.end();
            return;
         } else {
            Serial.println("🚀 Nueva versión detectada. Iniciando OTA...");
            Serial.printf("📦 Espacio libre para OTA: %u bytes\n", ESP.getFreeSketchSpace());
            sendTelegramMessage("🚀 Nueva versión detectada. Iniciando OTA...", config);
            
            // Liberar algo de memoria si es posible antes de empezar
            heap_caps_free(heap_caps_malloc(1, MALLOC_CAP_8BIT));

            http.end();
            downloadAndUpdate();
         }
        
    } else {
           Serial.printf("❌ Error HTTP: %d al obtener información de Releases.\n", httpCode);
           writeLog("❌ Error HTTP: " + String(httpCode) + " al obtener información de Releases.");
           http.end();
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
    client.setInsecure(); // Omitir validación estricta de certificados y ahorrar memoria RAM SSL
    client.setTimeout(30); // Timeout del socket a 30s para descargas grandes
    
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
            // 🔄 Alimentar el Watchdog y dar tiempo a MbedTLS para evitar cierres de socket durante la descarga
            yield();
            vTaskDelay(1);

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
            // Esperar un momento para asegurar entrega de notificaciones y evitar re-activar watchdog justo antes del reinicio
            delay(1000);
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