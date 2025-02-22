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

    // 🛑 Suspender la tarea de ThingSpeak
    if (thingSpeakTaskHandle != NULL) {
        Serial.println("🛑 Suspendiendo tarea ThingSpeak...");
        vTaskSuspend(thingSpeakTaskHandle);
    }

    // 📤 Enviar notificación a Telegram
    sendTelegramMessage("🛑 OTA iniciada: Suspendiendo procesos y Watchdog.", config);
}


void enableWatchdog() {
    Serial.println("✅ Reactivando Watchdog y reanudando procesos...");
    otaInProgress = false;  // Indicar que la OTA ha finalizado
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);

    // ✅ Reanudar la tarea de ThingSpeak
    if (thingSpeakTaskHandle != NULL) {
        Serial.println("✅ Reanudando tarea ThingSpeak...");
        vTaskResume(thingSpeakTaskHandle);
    }

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
        String jsonResponse = http.getString();  // 📌 Guardar la respuesta en un String
        //Serial.println("📜 Respuesta JSON: " + jsonResponse);
       

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
            Serial.printf("📦 Espacio libre para OTA: %u bytes\n", ESP.getFreeSketchSpace());
            sendTelegramMessage("🚀 Nueva versión detectada. Iniciando OTA...", config);
            
            // esp_task_wdt_init(30, true);
            // esp_task_wdt_add(NULL);
            heap_caps_free(heap_caps_malloc(1, MALLOC_CAP_8BIT));

            downloadAndUpdate();
         }
        
         } else {
           Serial.printf("❌ Error HTTP: %d al obtener información de Releases.\n", httpCode);
    }

    http.end();
}
    
    
void checkForIndexUpdate() {

    if (otaInProgress) {
        return;  // 🔹 Si la OTA está en proceso, no ejecutamos nada más
    }

    Serial.println("🔍 Verificando actualización de index.html...");

    WiFiClientSecure client;
    client.setInsecure();  // Deshabilita la verificación SSL
    client.stop();

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
    disableWatchdog();
    WiFiClientSecure client;
    client.setInsecure();
    client.stop();

    HTTPClient http;
    http.begin(client, url2);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        File file = SPIFFS.open(path, "w");
        if (!file) {
            Serial.println("❌ Error al abrir archivo en SPIFFS.");
            sendTelegramMessage("❌ Error al abrir archivo en SPIFFS.", config);
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
        enableWatchdog();
        return true;
    } else {
        Serial.printf("❌ Error HTTP %d al descargar archivo.\n", httpCode);
        sendTelegramMessage("❌ Error HTTP %d al descargar archivo.\n", config);
        enableWatchdog();
        return false;
    }

    http.end();
}

void downloadAndUpdate() {
    disableWatchdog(); // Desactivar Watchdog para evitar reinicios

    String firmwareURL = getFirmwareURL();
    if (firmwareURL == "") {
        Serial.println("❌ No se pudo obtener la URL del firmware.");
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

        if (contentLength < 1000000) { // Si el tamaño es sospechosamente bajo, reintentar
            Serial.println("❌ Tamaño del firmware demasiado pequeño. Reintentando...");
            http.end();
            enableWatchdog();
            return;
        }

        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
        if (update_partition == NULL) {
            Serial.println("❌ No se encontró una partición OTA válida.");
            sendTelegramMessage("❌ No se encontró una partición OTA válida.", config);
            http.end();
            enableWatchdog();
            return;
        }

        if (contentLength > ESP.getFreeSketchSpace()) {
            Serial.println("❌ No hay suficiente espacio para actualizar.");
            sendTelegramMessage("❌ No hay suficiente espacio para actualizar.", config);
            http.end();
            enableWatchdog();
            return;
        }

        if (!Update.begin(contentLength, U_FLASH)) {
            Serial.println("❌ Error al iniciar la actualización.");
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
                sendTelegramMessage("❌ Error al finalizar la actualización.", config);
                Update.printError(Serial);
                enableWatchdog();
            }
        } else {
            Serial.println("❌ Error: No se recibió el firmware completo.");
            sendTelegramMessage("❌ Error: No se recibió el firmware completo.", config);
            enableWatchdog();
        }
    } else {
        Serial.printf("❌ Error HTTP: %d al descargar firmware.\n", httpCode);
        sendTelegramMessage("❌ Error HTTP: " + String(httpCode) + " al descargar firmware.", config);
        enableWatchdog();
    }

    http.end();

}

