#include "web_server.h"
#include "config.h"
#include "base64.h"
#include "mbedtls/base64.h"
#include <Update.h>
#include "ota_update.h"

AsyncWebServer server(80);

// 🔹 Manejo de la subida de firmware OTA Web
void handleOTA(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    static size_t totalSize = 0;

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ Error: WiFi desconectado, abortando OTA.");
        request->send(500, "text/plain", "WiFi desconectado.");
        return;
    }
    
    
    if (!index) {
        size_t firmwareSize = request->contentLength();  // 📌 Obtiene el tamaño real del archivo .bin
        Serial.printf("📦 Tamaño esperado por ESP32 (contentLength): %u bytes\n", request->contentLength());
        Serial.printf("📦 Tamaño esperado del firmware: %u bytes\n", firmwareSize);
        Serial.printf("📦 Tamaño real del firmware.bin: %u bytes\n", ESP.getSketchSize());
        Serial.printf("📦 Espacio disponible para OTA: %u bytes\n", ESP.getFreeSketchSpace());
        Serial.printf("📥 Iniciando actualización OTA: %s (%d bytes)\n", filename.c_str(), firmwareSize);

        size_t freeSketchSpace = ESP.getFreeSketchSpace();  // Espacio disponible para OTA
        Serial.printf("📦 Espacio libre para OTA: %d bytes\n", freeSketchSpace);

        if (firmwareSize > freeSketchSpace) {
            Serial.println("❌ Error: El firmware es demasiado grande para OTA.");
            request->send(500, "text/plain", "Error: Firmware demasiado grande.");
            return;
        }

        if (!Update.begin(firmwareSize, U_FLASH)) {  // ✅ Reserva solo el espacio necesario
            Serial.println("❌ No se pudo iniciar la actualización");
            request->send(500, "text/plain", "Error al iniciar actualización OTA");
            return;
        }
    }

    size_t written = Update.write(data, len);
    delay(10);
    Serial.printf("⬇️ Recibiendo %d bytes | Escribiendo: %d bytes (Total recibido: %d/%d)\n", len, written, index + len, totalSize);

    if (written != len) {
        Serial.println("❌ Error: No se escribieron todos los bytes en Flash.");
        request->send(500, "text/plain", "Error al escribir en Flash");
        return;
    }

    if (final) {
        Serial.println("🔄 Finalizando actualización...");
        delay(500);
        if (Update.end()) {
            Serial.println("✅ Firmware actualizado correctamente. Reiniciando...");
            request->send(200, "text/plain", "✅ Actualización OTA completada. Reiniciando...");
            delay(2000);
            ESP.restart();
        } else {
            Serial.println("❌ Error finalizando la actualización OTA");
            Update.printError(Serial);
    
            size_t receivedSize = Update.progress();
            size_t expectedSize = Update.size();
            Serial.printf("❌ No se recibieron todos los bytes esperados. Recibido: %d / Esperado: %d\n", receivedSize, expectedSize);
    
            request->send(500, "text/plain", "❌ No se recibieron todos los bytes esperados.");
        }
    }
}


// 🔹 Agregar Ruta OTA en el WebServer
void startWebServer() {

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "Not Found");
    });

    // 🔹 Página principal protegida con autenticación
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!isAuthenticated(request)) return;
        request->send(SPIFFS, "/index.html", "text/html");
    });

    // 🔹 Configuración de parámetros (WiFi, Google Sheet, ThingSpeak, etc.)
    server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isAuthenticated(request)) return;

        Config newConfig = config;

        if (request->hasParam("ssid", true)) newConfig.ssid = request->getParam("ssid", true)->value();
        if (request->hasParam("password", true)) newConfig.password = request->getParam("password", true)->value();
        if (request->hasParam("googleSheetURL", true)) newConfig.googleSheetURL = request->getParam("googleSheetURL", true)->value();
        if (request->hasParam("thingSpeakAPIKey", true)) newConfig.thingSpeakAPIKey = request->getParam("thingSpeakAPIKey", true)->value();
        if (request->hasParam("updateInterval", true)) {
            newConfig.updateInterval = request->getParam("updateInterval", true)->value().toInt() * 60000;
        }
        if (request->hasParam("channelID", true)) newConfig.channelID = request->getParam("channelID", true)->value().toInt();
        if (request->hasParam("location", true)) newConfig.location = request->getParam("location", true)->value();

        if (saveConfig(newConfig)) {
            request->send(200, "text/plain", "✅ Configuración guardada. Reiniciando ESP32...");
            Serial.println("🔄 Reiniciando ESP32 para aplicar cambios...");
            delay(2000);
            ESP.restart();
        } else {
            request->send(500, "text/plain", "❌ Error al guardar configuración.");
        }
    });

    server.on("/update", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "📥 Subida OTA en progreso...");
        }, 
        [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
            handleOTA(request, filename, index, data, len, final);
        }
    );
     
    server.begin();
}

bool isAuthenticated(AsyncWebServerRequest *request) {
    if (!request->hasHeader("Authorization")) {
        AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", "Unauthorized");
        response->addHeader("WWW-Authenticate", "Basic realm=\"ESP32 Config\"");
        request->send(response);
        return false;
    }

    String authHeader = request->header("Authorization");
    authHeader.replace("Basic ", "");  // 🔹 Elimina "Basic " del encabezado
    String expectedAuth = base64::encode(webUsername + ":" + webPassword);  // 🔹 Codifica usuario:contraseña en Base64

    if (authHeader != expectedAuth) {
        request->send(403, "text/plain", "Forbidden");
        return false;
    }

    return true;
}