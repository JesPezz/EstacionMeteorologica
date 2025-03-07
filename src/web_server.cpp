#include "web_server.h"
#include "config.h"
#include "ota_update.h"
#include "base64.h"
#include "mbedtls/base64.h"
#include <Update.h>
#include "SensorManager.h"
#include "led.h"
#include "notifications.h"
#include <WiFi.h>
#include <ArduinoJson.h>

AsyncWebServer server(80);
void restartESP32Task(void *parameter);

// 📡 Función para devolver estado del ESP32 en JSON

void handleESPStatus(AsyncWebServerRequest *request) {
    JsonDocument doc;  // 🔹 Tamaño predefinido para evitar problemas de memoria

    doc["ip"] = WiFi.localIP().toString();
    doc["wifi"] = (WiFi.status() == WL_CONNECTED) ? "Conectado" : "Desconectado";
    doc["cpu"] = ESP.getCpuFreqMHz();
    doc["memory"] = ESP.getFreeHeap() / 1024;  // Memoria libre en bytes

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}


// 🔄 Función para reiniciar ESP32 remotamente
void handleRestart(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "ESP32 reiniciándose...");
    delay(1000);
    ESP.restart();
}

bool otaInProgress = false;  // 🔹 Indica si una OTA está en proceso

// 🔹 Manejo de la subida de firmware OTA Web
void handleOTA(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
    static size_t totalSize = 0;

    if (!index) {
        Serial.println("🚨 🔄 Suspendiendo procesos...");
        otaInProgress = true;

        size_t firmwareSize = request->contentLength();
        Serial.printf("📥 Iniciando OTA: %s (%d bytes)\n", filename.c_str(), firmwareSize);
        ledInProgress();

        if (!Update.begin(firmwareSize, U_FLASH)) {
            Serial.println("❌ No se pudo iniciar la OTA");
            errLeds();
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
        errLeds();
        request->send(500, "text/plain", "Error al escribir en Flash");
        otaInProgress = false;
        return;
    }

    if (final) {
        Serial.println("🔄 Finalizando actualización...");

        if (Update.hasError()) {
            Serial.println("❌ Error en la transferencia OTA.");
            errLeds();
            request->send(500, "text/plain", "Error en la transferencia OTA.");
            otaInProgress = false;
            return;
        }

        if (Update.end(true)) {
            Serial.println("✅ Firmware actualizado correctamente. Reiniciando...");
            request->send(200, "text/plain", "✅ OTA completada. Reiniciando...");
            ledSuccess();
            delay(1000);
            ESP.restart();
        } else {
            Serial.println("❌ Error finalizando OTA");
            errLeds();
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

    server.on("/esp_status", HTTP_GET, handleESPStatus);
    server.on("/restart", HTTP_POST, handleRestart);

    server.on("/getConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
    
        doc["ssid"] = config.ssid;
        doc["password"] = config.password;
        doc["googleSheetURL"] = config.googleSheetURL;
        doc["thingSpeakAPIKey"] = config.thingSpeakAPIKey;
        doc["channelID"] = config.channelID;
        doc["location"] = config.location;
        doc["updateOta"] = config.updateOta / 3600000;
        doc["telegramToken"] = config.telegramToken;
        doc["chatId"] = config.chatId;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // 🔹 Configuración de parámetros (WiFi, Google Sheet, ThingSpeak, etc.)
    server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        
        Serial.println("📡 Recibida solicitud POST en /config");


        if (!isAuthenticated(request)) {
            Serial.println("❌ Error: Autenticación fallida.");
            request->send(401, "text/plain", "❌ Acceso no autorizado.");
            return;
            }
    
            Serial.println("✅ Autenticación exitosa.");

        Config newConfig = config;
    
        if (request->hasParam("ssid", true)) {
            newConfig.ssid = request->getParam("ssid", true)->value();
            Serial.println("✅ SSID recibido: " + newConfig.ssid);
        }
        
        if (request->hasParam("password", true)) {
            newConfig.password = request->getParam("password", true)->value();
            Serial.println("✅ Password recibido.");
        }

        if (request->hasParam("googleSheetURL", true)) newConfig.googleSheetURL = request->getParam("googleSheetURL", true)->value();
        if (request->hasParam("thingSpeakAPIKey", true)) newConfig.thingSpeakAPIKey = request->getParam("thingSpeakAPIKey", true)->value();
        if (request->hasParam("updateOta", true)) {
            int updateOtaValue = request->getParam("updateOta", true)->value().toInt();
            if (updateOtaValue > 0) newConfig.updateOta = updateOtaValue * 3600000;
        }
        if (request->hasParam("channelID", true)) newConfig.channelID = request->getParam("channelID", true)->value().toInt();
        if (request->hasParam("location", true)) newConfig.location = request->getParam("location", true)->value();
    
        if (request->hasParam("telegramToken", true)) {
            String token = request->getParam("telegramToken", true)->value();
            if (!token.isEmpty()) newConfig.telegramToken = token;
        }
        if (request->hasParam("chatId", true)) {
            String chatId = request->getParam("chatId", true)->value();
            if (!chatId.isEmpty()) newConfig.chatId = chatId;
        }
    
        if (saveConfig(newConfig)) {
            Serial.println("✅ Configuración guardada con éxito.");
            
            Serial.println("📡 Intentando enviar respuesta HTTP...");
            request->send(200, "text/plain", "✅ Configuración guardada correctamente.");
            Serial.println("✅ Respuesta HTTP enviada.");

           // ✅ Crear una tarea para reiniciar sin bloquear el servidor
        xTaskCreate(restartESP32Task, "RestartESP32", 2048, NULL, 1, NULL);
        } else {
            Serial.println("❌ Error al guardar la configuración.");
            request->send(500, "text/plain", "❌ Error al guardar la configuración.");
        }
    });     

    // 🔹 Ruta para subir firmware OTA
    server.on("/update", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "📥 Subida OTA en progreso...");
        }, 
        handleOTA
    );

    server.begin(); // ✅ Se mueve fuera de cualquier `server.on()`
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

    String authData = webUsername + ":" + webPassword;  // Concatenación con String
    String expectedAuth = base64::encode(authData);     // Codificar en Base64

    Serial.println("🔍 authHeader: " + authHeader);
    Serial.println("🔍 expectedAuth: " + expectedAuth);

    if (authHeader != expectedAuth) {
        Serial.println("❌ Autenticación fallida");
        request->send(403, "text/plain", "Forbidden");
        return false;
    }

    return true;
}

// 🔄 ✅ Función compatible con FreeRTOS para reiniciar ESP32 sin bloquear el servidor
void restartESP32Task(void *parameter) {
    Serial.println("🔄 ESP32 se reiniciará en 3 segundos...");
    vTaskDelay(3000 / portTICK_PERIOD_MS);  // Esperar 3 segundos sin bloquear
    Serial.println("🔄 Reiniciando ESP32 ahora...");
    ESP.restart();
}
