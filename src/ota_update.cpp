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

// 🔹 Verificar nueva versión en GitHub Releases y devolver la URL de descarga
String checkForUpdates() {
    Serial.println("🔍 Verificando nueva versión en GitHub Releases...");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi no conectado. Omitiendo verificación de Releases.");
        writeLog("❌ WiFi no conectado. Omitiendo verificación de Releases.");
        return "";
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

    if (httpCode == HTTP_CODE_OK) {
        String jsonResponse = http.getString();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonResponse);

        if (error) {
            Serial.println("❌ Error al parsear JSON: " + String(error.c_str()));
            writeLog("❌ Error al parsear JSON: " + String(error.c_str()));
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

        String newVersion = json["tag_name"];
        String downloadURL = json["assets"][0]["browser_download_url"];

        Serial.printf("📌 Última versión en GitHub: %s\n", newVersion.c_str());
        Serial.printf("📥 URL del firmware: %s\n", downloadURL.c_str());

        http.end();

        if (newVersion == version) {
            Serial.println("✅ El ESP32 ya está actualizado.");
            return "";
        }

        Serial.println("🚀 Nueva versión detectada.");
        Serial.printf("📦 Espacio libre para OTA: %u bytes\n", ESP.getFreeSketchSpace());
        return downloadURL;
    }

    Serial.printf("❌ Error HTTP: %d al obtener información de Releases.\n", httpCode);
    writeLog("❌ Error HTTP: " + String(httpCode) + " al obtener información de Releases.");
    http.end();
    return "";
}

void downloadAndUpdate(const String &firmwareURL) {
    disableWatchdog();

    if (firmwareURL == "") {
        Serial.println("❌ No se pudo obtener la URL del firmware.");
        sendTelegramMessage("❌ No se pudo obtener la URL del firmware.", config);
        enableWatchdog();
        return;
    }

Serial.println("🔗 URL del firmware: " + firmwareURL);

    Serial.printf("📦 Espacio libre para OTA: %u bytes\n", ESP.getFreeSketchSpace());
    sendTelegramMessage("🚀 Nueva versión detectada. Iniciando OTA...", config);

    const int MAX_ATTEMPTS = 3;
    const unsigned long STALL_TIMEOUT_MS = 30000;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        if (attempt > 1) {
            Serial.printf("🔄 Reintento %d/%d de la descarga...\n", attempt, MAX_ATTEMPTS);
            delay(1000);
        }

        WiFiClientSecure client;
        client.setInsecure(); // Omitir validación estricta de certificados y ahorrar memoria RAM SSL
        client.setTimeout(30); // Timeout del socket a 30s para descargas grandes

        HTTPClient http;
        http.setTimeout(30000);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Manejar el redirect 302 de GitHub internamente
        http.begin(client, firmwareURL);
        int httpCode = http.GET();

        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("❌ Error HTTP: %d al descargar firmware.\n", httpCode);
            writeLog("❌ Error HTTP: " + String(httpCode) + " al descargar firmware.");
            http.end();
            continue;
        }

        int contentLength = http.getSize();
        Serial.printf("📥 Tamaño del firmware: %d bytes\n", contentLength);

        if (contentLength < 100000) { // Ajustado umbral mínimo
            Serial.println("❌ Tamaño del firmware demasiado pequeño.");
            writeLog("❌ Tamaño del firmware demasiado pequeño.");
            http.end();
            continue;
        }

        if (!Update.begin(contentLength, U_FLASH)) {
            Serial.println("❌ Error al iniciar la actualización.");
            writeLog("❌ Error al iniciar la actualización.");
            http.end();
            continue;
        }

        WiFiClient *stream = http.getStreamPtr();
        // Búfer en heap para no consumir stack del loopTask durante la descarga
        const size_t BUFFER_SIZE = 2048;
        uint8_t *buffer = (uint8_t*)malloc(BUFFER_SIZE);
        if (buffer == nullptr) {
            Serial.println("❌ Out of memory al reservar búfer de descarga.");
            http.end();
            continue;
        }
        size_t written = 0;
        unsigned long lastDataAt = millis();

        while (written < contentLength) {
            // 🔄 Alimentar al sistema y dar tiempo a MbedTLS para evitar cierres de socket durante la descarga
            yield();
            vTaskDelay(1);

            int availableBytes = stream->available();
            if (availableBytes > 0) {
                lastDataAt = millis();
                int toRead = min(availableBytes, (int)BUFFER_SIZE);
                int bytesRead = stream->readBytes(buffer, toRead);
                if (bytesRead > 0) {
                    if (Update.write(buffer, bytesRead) != bytesRead) {
                        Serial.println("❌ Error al escribir en flash.");
                        writeLog("❌ Error al escribir en flash.");
                        break;
                    }
                    written += bytesRead;
                }
            } else if (millis() - lastDataAt > STALL_TIMEOUT_MS) {
                // Sin datos durante mucho tiempo: la conexión TLS se cortó (ej. error -76)
                Serial.printf("⏱️ Timeout sin datos durante %lu ms. Conexión perdida (error SSL -76).\n", STALL_TIMEOUT_MS);
                break;
            }
        }

        free(buffer);
        http.end();

        if (written == contentLength) {
            if (Update.end()) {
                Serial.println("✅ Firmware actualizado correctamente. Reiniciando...");
                sendTelegramMessage("✅ Firmware actualizado correctamente. Reiniciando...", config);
                delay(1000);
                ESP.restart();
            } else {
                Serial.println("❌ Error al finalizar la actualización.");
                writeLog("❌ Error al finalizar la actualización.");
                Update.printError(Serial);
                Update.abort();
            }
        } else {
            Serial.printf("⚠️ Descarga incompleta: %u/%d bytes.\n", (unsigned)written, contentLength);
            writeLog("⚠️ Descarga incompleta en intento " + String(attempt) + ": " + String(written) + "/" + String(contentLength) + " bytes");
            Update.abort();
        }
    }

    Serial.println("❌ OTA falló tras " + String(MAX_ATTEMPTS) + " intentos.");
    sendTelegramMessage("❌ OTA falló tras " + String(MAX_ATTEMPTS) + " intentos.", config);
    writeLog("❌ OTA falló tras " + String(MAX_ATTEMPTS) + " intentos.");
    enableWatchdog();
}