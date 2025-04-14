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
#include "BME_Sensor.h"
#include <AsyncJson.h>
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <vector>
#include <AsyncEventSource.h>
#include <freertos/timers.h>
#include "WiFiManager.h"

SemaphoreHandle_t wifiMutex = NULL;
std::vector<WiFiNetwork> wifiNetworks; // Declare wifiNetworks globally

AsyncWebServerResponse* prepareCORSResponse(AsyncWebServerRequest* request, int code, String contentType);
String contentType;
AsyncWebServer server(80);
AsyncEventSource events("/events");
const char* sensorDataFile = "/sensor_data.json";
void restartESP32Task(void *parameter);
bool otaInProgress = false;  // 🔹 Indica si una OTA está en proceso

struct NetworkConfig {
    char ssid[32];       // Tamaño fijo para SSID
    char password[64];    // Tamaño fijo para contraseña
};

NetworkConfig newNet;

AsyncWebServerResponse* prepareCORSResponse(AsyncWebServerRequest* request, int code, String contentType) {
    AsyncWebServerResponse* response = request->beginResponse(code, contentType);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    return response;

}

String processorHTML(const String& var) {
    if(var == "TEMPERATURA") return String(iaqSensor.temperature);
    if(var == "HUMEDAD") return String(iaqSensor.humidity);
    return String();
}

String getSensorJson() {
    JsonDocument doc;
    
    if(xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1000))) {
        doc["temp"] = iaqSensor.temperature;
        doc["humidity"] = iaqSensor.humidity;
        xSemaphoreGive(sensorMutex);
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

void wifiScanTask(void *pvParameters) {
    while(true) {
        if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000))) {
            if (scanRequested) {
                scanRequested = false; // Resetear primero para evitar pérdidas
                xSemaphoreGive(wifiMutex);
                
                //std::vector<WiFiNetwork> newNetworks;
                WiFiManager::scanNetworks(networks);
                if (!networks.empty()) {
                    if(xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(2000))) {
                        wifiNetworks = networks;
                        xSemaphoreGive(wifiMutex);
                        Serial.printf("[WiFi] Escaneo completado. %d redes\n", wifiNetworks.size());
                    }
                } else {
                    Serial.println("[WiFi] Error en escaneo");
                }
            } else {
                xSemaphoreGive(wifiMutex);
                vTaskDelay(200 / portTICK_PERIOD_MS); // Mayor delay cuando inactivo
            }
        }
    }
}

void handleWiFiScanInternal(AsyncWebServerRequest *request) {
    if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(1000))) {
        if (!scanRequested) {
            scanRequested = true;
            xSemaphoreGive(wifiMutex);
            request->send(202, "text/plain", "Escaneo iniciado");
            Serial.println("[WiFi] Escaneo solicitado via API");
        } else {
            xSemaphoreGive(wifiMutex);
            request->send(200, "text/plain", "Escaneo ya en curso");
        }
    } else {
        request->send(503, "text/plain", "Error de sistema");
    }
}

void handleWiFiScanAPI(AsyncWebServerRequest *request) {
    // Solo actualizamos la variable global
    WiFiManager::scanNetworks(networks); // Actualiza el vector global
    
    // Respuesta muy básica
    request->send(200, "text/plain", "Redes escaneadas y actualizadas");
}

// Función para validar URLs
bool isValidURL(String url, const char* domain) {
    return url.indexOf(domain) != -1 && url.startsWith("https://");
}

void initSSETimer() {
    sseTimer = xTimerCreate(
        "SSE_Timer",
        pdMS_TO_TICKS(5000),  // Periodo de 5 segundos
        pdTRUE,               // Timer auto-reloading
        (void*)0,             // ID del timer
        [](TimerHandle_t xTimer){  // Función callback
            String json = getSensorJson();
            events.send(json.c_str(), "update");
        }
    );
}

void handleSSE(AsyncWebServerRequest *request) {
    request->send(200, "text/event-stream");
}

void handleSavedNetworks(AsyncWebServerRequest* request) { //WiFiManager::loadSavedNetworks(std::vector<WiFiNetwork>& networks)
    std::vector<WiFiNetwork> savedNetworks;
    
    if (WiFiManager::loadSavedNetworks(savedNetworks)) {
        JsonDocument doc;
        JsonArray networks = doc.to<JsonArray>();
        
        for (const auto& net : savedNetworks) {
            JsonObject obj = networks.add<JsonObject>();
            obj["ssid"] = net.ssid;
            // No enviar contraseñas por seguridad
        }
        
        String jsonResponse;
        serializeJson(doc, jsonResponse);
        request->send(200, "application/json", jsonResponse);
    } else {
        request->send(500, "text/plain", "Error cargando redes guardadas");
    }
}

void sendSSEData(TimerHandle_t xTimer) {
    String json = getSensorJson();
    events.send(json.c_str(), "update");
}

void sendSensorData() {
    String jsonData = getSensorJson(); // Usa tu función existente
    events.send(jsonData.c_str(), "update");
}

void handleWiFiConnect(AsyncWebServerRequest *request) {
    // 1. Parsear JSON de la solicitud
    JsonDocument requestDoc;
    if (!parseRequestJSON(request, requestDoc)) return;

    String ssid = requestDoc["ssid"] | "";
    String password = requestDoc["password"] | "";

    if (ssid.isEmpty() || password.isEmpty()) {
        request->send(400, "application/json", "{\"error\":\"SSID/Password requeridos\"}");
        return;
    }

    handleWiFiSave(request, nullptr, 0, 0, 0); // 🔥 Guarda en SPIFFS

    // 3. Conectar a la red
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(ssid.c_str(), password.c_str());

    // Esperar conexión (con timeout)
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(500);
        Serial.print(".");
    }

    // 4. Responder al cliente
    JsonDocument responseDoc;
    if (WiFi.status() == WL_CONNECTED) {
        responseDoc["status"] = "connected";
        responseDoc["ip"] = WiFi.localIP().toString();
    } else {
        responseDoc["status"] = "failed";
        responseDoc["error"] = "Timeout de conexión";
    }

    String responseJson;
    serializeJson(responseDoc, responseJson);
    request->send(200, "application/json", responseJson);

    // 5. Reiniciar para aplicar cambios (opcional)
    delay(1000);
    ESP.restart();
}

// Función auxiliar para parsear JSON
bool parseRequestJSON(AsyncWebServerRequest* request, JsonDocument& doc) {
    String body;
    if (request->hasParam("plain", true)) {
        body = request->getParam("plain", true)->value();
    } else {
        request->send(400, "application/json", "{\"error\":\"Invalid request format\"}");
        return false;
    }

    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return false;
    }
    return true;
}

void handleWiFiScan(AsyncWebServerRequest* request) { //envia la lista de redes WiFi disponibles de networks de WiFiManager::scanNetworks(std::vector<WiFiNetwork>& networks)
    
    if(otaInProgress) {
        request->send(503, "text/plain", "Actualización OTA en progreso. Intente más tarde.");
        return;
    }

    Serial.println("📤 Enviando lista de redes WiFi almacenadas...");

    JsonDocument doc;
    JsonArray jsonNetworks = doc.to<JsonArray>();

    for (const auto& net : networks) {  // 🔹 Leer redes desde `networks`
        JsonObject obj = jsonNetworks.add<JsonObject>();
        obj["ssid"] = net.ssid;
        obj["rssi"] = net.rssi;
        obj["secured"] = net.encryptionType != WIFI_AUTH_OPEN;
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    Serial.println(jsonResponse);  // 🔹 Ver JSON en Serial Monitor

    request->send(200, "application/json", jsonResponse);
}

void handleWiFiSave(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) { //WiFiManager::saveNetwork(const WiFiNetwork& network)
    Serial.println("🔍 handleWiFiSave fue llamado.");

    // Convertir los datos recibidos en una cadena
    String body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    Serial.println("📩 JSON Recibido:");
    Serial.println(body);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        Serial.print("❌ Error de parseo JSON: ");
        Serial.println(error.f_str());
        request->send(400, "application/json", "{\"error\":\"Invalid JSON format\"}");
        return;
    }

    String ssid = doc["ssid"] | "";
    String password = doc["password"] | "";

    if (ssid.isEmpty() || password.isEmpty()) {
        request->send(400, "application/json", "{\"error\":\"Missing parameters\"}");
        return;
    }

    Serial.println("✅ SSID: " + ssid);
    Serial.println("✅ Password: " + password);

    // Guardar la red WiFi
    NetworkConfig newNet;
    strncpy(newNet.ssid, ssid.c_str(), sizeof(newNet.ssid) - 1);
    newNet.ssid[sizeof(newNet.ssid) - 1] = '\0'; // Ensure null-termination
    strncpy(newNet.password, password.c_str(), sizeof(newNet.password) - 1);
    newNet.password[sizeof(newNet.password) - 1] = '\0'; // Ensure null-termination

    Serial.println("🔄 Intentando guardar la red WiFi...");
    WiFiNetwork wifiNet;
    strncpy(wifiNet.ssid, newNet.ssid, sizeof(wifiNet.ssid) - 1);
    wifiNet.ssid[sizeof(wifiNet.ssid) - 1] = '\0'; // Ensure null-termination
    strncpy(wifiNet.password, newNet.password, sizeof(wifiNet.password) - 1);
    wifiNet.password[sizeof(wifiNet.password) - 1] = '\0'; // Ensure null-termination

    bool saved = WiFiManager::saveNetwork(wifiNet);
    Serial.print("🔎 Resultado de saveNetwork: ");
    Serial.println(saved ? "ÉXITO" : "FALLÓ");
    

    if (saved) {
        request->send(200, "application/json", "{\"status\":\"success\"}");
        delay(1000);
        ESP.restart();
      } else {
        // Nuevo: Enviar motivo específico del error
        String errorMsg = "{\"error\":\"No se pudo guardar\",\"details\":\"";
            WiFiManager wifiManager; // Crea una instancia si no existe
            errorMsg += wifiManager.getLastError();        errorMsg += "\",\"spiffs\":";
        errorMsg += SPIFFS.totalBytes() - SPIFFS.usedBytes(); // Espacio libre
        errorMsg += "}";
        request->send(500, "application/json", errorMsg);
      }
    }

    const char* WiFiManager::getLastError() {
        if (!SPIFFS.exists("/wifi.json")) {  // Cambiado a wifi.json
            return "Archivo wifi.json no existe";
        }
        
        File file = SPIFFS.open("/wifi.json", FILE_READ);
        if (!file) {
            return "No se pudo abrir wifi.json";
        }
        
        if (file.size() == 0) {
            file.close();
            return "wifi.json está vacío";
        }
        
        file.close();
        return "Error desconocido al guardar en wifi.json";
    }

void handleESPStatus(AsyncWebServerRequest *request) {
    AsyncWebServerResponse* response = prepareCORSResponse(request, 200, "application/json");
    JsonDocument doc;
    
    doc["ip"] = WiFi.localIP().toString();
    doc["wifi"] = WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado";
    doc["ssid"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "Desconectado";
    doc["cpu"] = ESP.getCpuFreqMHz();
    doc["memory"] = ESP.getFreeHeap() / 1024;

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);

}

void handleRestart(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "ESP32 reiniciándose...");
    delay(1000);
    ESP.restart();
}

void handleOTA(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
    static size_t totalSize = 0;
    if(!filename.endsWith(".bin")) {
        request->send(400, "text/plain", "Solo se permiten archivos .bin");
        return;
    }

    if (!index) {
        Serial.println("🚨 🔄 Suspendiendo procesos...");
        void disableWatchdog(); // Desactiva el Watchdog y suspende la tarea
        
        size_t firmwareSize = request->contentLength();
        Serial.printf("📥 Iniciando OTA: %s (%d bytes)\n", filename.c_str(), firmwareSize);
        ledInProgress();
        
        if (!Update.begin(firmwareSize, U_FLASH)) {
            Serial.println("❌ No se pudo iniciar la OTA");
            errLeds();
            request->send(500, "text/plain", "Error al iniciar actualización");
            void enableWatchdog(); // Reactiva el Watchdog y reanuda la tarea 
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

void startWebServer() {

    server.on("/api/*", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(204);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        request->send(response);
    });
    

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("📂 Petición recibida para /");
        if (!SPIFFS.exists("/index.html")) {
            Serial.println("❌ index.html no encontrado en SPIFFS");
            request->send(404, "text/plain", "File Not Found");
            return;
        }
        Serial.println();
        Serial.println("📤 Enviando index.html...");
        Serial.println();
        request->send(SPIFFS, "/index.html", "text/html");
    });
    
    server.on("/chart.umd.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/chart.umd.min.js", "application/javascript");
      });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(204); // Respuesta vacía (No Content)
    });
    server.on("/events", HTTP_GET, handleSSE);
    server.addHandler(&events);
    server.on("/api/savedNetworks", HTTP_GET, handleSavedNetworks);
    server.on("/api/wifi/scanInternal", HTTP_GET, [](AsyncWebServerRequest *request){
        handleWiFiScanInternal(request);
    });
    server.on("/api/wifi/scan", HTTP_GET, handleWiFiScan);
    server.on("/api/wifi/connect", HTTP_POST, handleWiFiConnect);
    server.on("/saveWiFi", HTTP_POST, [](AsyncWebServerRequest *request) {},
          NULL, handleWiFiSave);
    
    server.on("/sensor_data", HTTP_GET, handleSensorData);
    server.on("/esp_status", HTTP_GET, handleESPStatus);
    server.on("/restart", HTTP_POST, handleRestart);

    server.on("/getConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
    
        // 🔄 Usar add() para cadenas C-style (si config usa char* o const char*)
        doc["googleSheetURL"] = config.googleSheetURL;
        doc["thingSpeakAPIKey"] = config.thingSpeakAPIKey;
        doc["location"] = config.location;
        doc["telegramToken"] = config.telegramToken;
        doc["chatId"] = config.chatId;
        doc["webUsername"] = webUsername;
        doc["webPassword"] = webPassword;
    
        // ✅ Campos que no son cadenas C-style:
        doc["channelID"] = config.channelID;
        doc["updateOta"] = config.updateOta;
    
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/debug", HTTP_POST, [](AsyncWebServerRequest *request){
        String response = "Método: " + String(request->methodToString()) + "\n";
        response += "Content-Type: " + request->contentType() + "\n";
        response += "Content-Length: " + String(request->contentLength()) + "\n";
        response += "Headers:\n";
        
        for(size_t i=0; i<request->headers(); i++){
            const AsyncWebHeader* h = request->getHeader(i);
            response += "  " + h->name() + ": " + h->value() + "\n";
        }
        
        request->send(200, "text/plain", response);
    });

    // 1. Manejador OPTIONS para CORS
server.on("/config", HTTP_OPTIONS, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    request->send(response);
});

// 2. Manejador POST mejorado
server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println();
    Serial.println("\n--- PETICIÓN CONFIG RECIBIDA ---");
    Serial.println();
    // Verificar autenticación
    if(!isAuthenticated(request)) {
        request->send(401, "text/plain", "Acceso no autorizado");
        return;
    }

    // Verificar si tiene body
    if(!request->hasParam("plain", true)) {
        Serial.println();
        Serial.println("ERROR: No se recibió parámetro 'plain'");
        Serial.println();
        request->send(400, "text/plain", "No se recibieron datos");
        return;
    }

    // Procesar el body
    String body = request->getParam("plain", true)->value();
    Serial.println();
    Serial.println("Body recibido: " + body);
    Serial.println();
    // Parsear JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if(error) {
        Serial.println();
        Serial.print("ERROR parseando JSON: ");
        Serial.println(error.c_str());
        Serial.println();
        request->send(400, "text/plain", "Error en formato JSON");
        return;
    }

    // Procesar configuración
    Config newConfig = config;
    
    if(!doc["updateOta"].isNull()) {
        long otaValue = doc["updateOta"];
        newConfig.updateOta = (otaValue < 100) ? otaValue * 3600000 : otaValue;
        Serial.println();
        Serial.printf("Nuevo updateOta: %ld ms (%d horas)\n", newConfig.updateOta, otaValue);
        Serial.println();
    }
        if (!doc["googleSheetURL"].isNull()) {
            newConfig.googleSheetURL = doc["googleSheetURL"].as<String>();
        }
        if (!doc["thingSpeakAPIKey"].isNull()) {
            newConfig.thingSpeakAPIKey = doc["thingSpeakAPIKey"].as<String>();
        }
        if (!doc["channelID"].isNull()) {
            newConfig.channelID = doc["channelID"];
        }
        if (!doc["location"].isNull()) {
            newConfig.location = doc["location"].as<String>();
        }
        if (!doc["telegramToken"].isNull()) {
            newConfig.telegramToken = doc["telegramToken"].as<String>();
        }
        if (!doc["chatId"].isNull()) {
            newConfig.chatId = doc["chatId"].as<String>();
        }
    
         // Guardar configuración
    if(saveConfig(newConfig)) {
        request->send(200, "text/plain", "Configuración guardada. Reiniciando...");
        delay(500);
        ESP.restart();
    } else {
        request->send(500, "text/plain", "Error al guardar configuración");
    }
}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    // Manejador para recibir el cuerpo RAW
    Serial.println();
    Serial.printf("Recibiendo datos: %d/%d bytes\n", index + len, total);
    Serial.println();
});

server.on("/test", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("plain", true)) {
        String body = request->getParam("plain", true)->value();
        request->send(200, "text/plain", "Recibido: " + body);
    } else {
        request->send(200, "text/plain", "No se recibió body");
    }
});

    // 🔹 Ruta para subir firmware OTA
    server.on("/update", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "📥 Subida OTA en progreso...");
        }, 
        handleOTA
    );

    server.begin(); // ✅ Se mueve fuera de cualquier server.on()
}

// 🔹 Verificar autenticación básica
bool isAuthenticated(AsyncWebServerRequest *request) {
    // 🔹 Verificar si la cabecera Authorization está presente
    if (!request->hasHeader("Authorization")) {
        AsyncWebServerResponse *response = request->beginResponse(401, "text/plain", "Unauthorized");
        response->addHeader("WWW-Authenticate", "Basic realm=\"ESP32 Config\"");
        request->send(response);
        return false;  // 🔹 Salir inmediatamente
    }

    // 🔹 Generar el string usuario:contraseña
    String authData = String(webUsername) + ":" + String(webPassword);

    // 🔹 Obtener y limpiar la cabecera Authorization
    String authHeader = request->header("Authorization");
    authHeader.replace("Basic ", "");  
    authHeader.trim();

    // 🔹 Decodificar la autenticación Base64 y comparar con el usuario real
    size_t len;
    unsigned char decoded[64];
    mbedtls_base64_decode(decoded, sizeof(decoded), &len, (const unsigned char*)authHeader.c_str(), authHeader.length());
    String decodedAuth = String((char*)decoded).substring(0, len);
    decodedAuth.trim();  // 🔹 Asegurar que no tenga espacios extra
    Serial.println();
    Serial.println("🔍 authHeader (Base64): " + authHeader);
    Serial.println("🔍 decodedAuth: " + decodedAuth);
    Serial.println("🔍 expectedAuth: " + authData);
    Serial.println();

    // 🔹 Comparar credenciales
    if (decodedAuth != authData) {
        Serial.println();
        Serial.println("❌ Autenticación fallida");
        Serial.println();
        request->send(403, "text/plain", "Forbidden");
        return false;
    }

    return true;
}


// 🔄 ✅ Función compatible con FreeRTOS para reiniciar ESP32 sin bloquear el servidor
void restartESP32Task(void *parameter) {
    Serial.println();
    Serial.println("Reiniciando en 3 segundos...");
    Serial.println();
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP.restart();
    vTaskDelete(NULL);
}

void handleSensorData(AsyncWebServerRequest *request) {
    JsonDocument doc;
    AsyncWebServerResponse* response = prepareCORSResponse(request, 200, "application/json");
    
    if(xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        const char* precisiones[] = {"No disponible", "Baja", "Media", "Alta"};
        uint8_t precision = iaqSensor.iaqAccuracy > 3 ? 0 : iaqSensor.iaqAccuracy;

        doc["temp"] = iaqSensor.temperature;
        doc["humidity"] = iaqSensor.humidity;
        doc["pressure"] = iaqSensor.pressure;
        doc["air_quality"] = iaqSensor.iaq;
        doc["precision"] = precisiones[precision];
        doc["version"] = version;

        xSemaphoreGive(sensorMutex);
    } else {
        doc["error"] = "Timeout de sensor";
    }

    String json;
    serializeJson(doc, json); // Serializa a String
    request->send(200, "application/json", json); // Envía respuesta
}

void initWiFiScanner() {
    wifiMutex = xSemaphoreCreateMutex();
    if (!wifiMutex) {
        Serial.println();
        Serial.println("[CRITICAL] Fallo al crear wifiMutex");
        Serial.println();
        ESP.restart();
    }
}

void initSensorMutex() {
    sensorMutex = xSemaphoreCreateMutex();
    if (sensorMutex == NULL) {
        Serial.println();
        Serial.println("❌ Error creando mutex de sensores");
        Serial.println();
    }
}

void listSPIFFS() {

    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file) {
      Serial.printf("Archivo: %s, Tamaño: %d\n", file.name(), file.size());
      file = root.openNextFile();
    }
  }

  void logConfig(const char* title, const Config &config) {
    Serial.println();
    Serial.println(title);
    Serial.println("----------------------------");
    Serial.printf("GoogleSheetURL: %s\n", config.googleSheetURL.c_str());
    Serial.printf("ThingSpeakAPIKey: %s\n", config.thingSpeakAPIKey.c_str());
    Serial.printf("ChannelID: %d\n", config.channelID);
    Serial.printf("Location: %s\n", config.location.c_str());
    Serial.printf("updateOta: %ld ms (%d horas)\n", 
                 config.updateOta, config.updateOta/3600000);
    Serial.printf("TelegramToken: %s\n", config.telegramToken.c_str());
    Serial.printf("ChatID: %s\n", config.chatId.c_str());
    Serial.println("----------------------------");
    Serial.println();
}
