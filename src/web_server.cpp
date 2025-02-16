#include "web_server.h"
#include "config.h"
#include "ota_update.h"
#include "base64.h"
#include "mbedtls/base64.h"
#include <Update.h>
#include "SensorManager.h"

AsyncWebServer server(80);

// 🔹 Manejo de la subida de firmware OTA Web
bool otaInProgress = false;  // 🔹 Variable global para indicar OTA en proceso

void handleOTA(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
    static size_t totalSize = 0;

    if (!index) {
        Serial.println("🚨 🔄 Suspendiendo procesos...");
        otaInProgress = true;  // 🔹 Indica que OTA está activa

        size_t firmwareSize = request->contentLength();
        Serial.printf("📥 Iniciando OTA: %s (%d bytes)\n", filename.c_str(), firmwareSize);

        if (!Update.begin(firmwareSize, U_FLASH)) {
            Serial.println("❌ No se pudo iniciar la OTA");
            request->send(500, "text/plain", "Error al iniciar actualización");
            otaInProgress = false;
            return;
        }

        totalSize = 0;
    }

    size_t written = Update.write(data, len);
    totalSize += written;

    if (written != len) {
        Serial.println("❌ Error al escribir en Flash");
        request->send(500, "text/plain", "Error al escribir en Flash");
        otaInProgress = false;
        return;
    }

    if (final) {
        Serial.println("🔄 Finalizando actualización...");

        if (Update.hasError()) {
            Serial.println("❌ Error en la transferencia OTA.");
            request->send(500, "text/plain", "Error en la transferencia OTA.");
            otaInProgress = false;
            return;
        }

        if (Update.end(true)) {
            Serial.println("✅ Firmware actualizado correctamente. Reiniciando...");
            request->send(200, "text/plain", "✅ OTA completada. Reiniciando...");
            delay(1000);
            ESP.restart();
        } else {
            Serial.println("❌ Error finalizando OTA");
            request->send(500, "text/plain", "Error finalizando OTA.");
            otaInProgress = false;
        }
    }
}

// 🔹 Iniciar el servidor web y configurar rutas
void startWebServer() {
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
        handleOTA
    );

    server.begin();
}

// 🔹 Verificar autenticación básica
bool isAuthenticated(AsyncWebServerRequest *request) {
    if (!request->hasHeader("Authorization")) {
        AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", "Unauthorized");
        response->addHeader("WWW-Authenticate", "Basic realm=\"ESP32 Config\"");
        request->send(response);
        return false;
    }

    String authHeader = request->header("Authorization");
    authHeader.replace("Basic ", "");  
    String expectedAuth = base64::encode(webUsername + ":" + webPassword);  

    if (authHeader != expectedAuth) {
        request->send(403, "text/plain", "Forbidden");
        return false;
    }

    return true;
}


