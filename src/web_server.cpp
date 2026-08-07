#include "web_server.h"
#include "config.h"
#include "ota_update.h"
#include "base64.h"
#include "mbedtls/base64.h"
#include <Update.h>
#include "SensorManager.h"
#include "led_task.h"
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
#include "led_task.h"
SemaphoreHandle_t wifiMutex = NULL;
std::vector<WiFiNetwork> wifiNetworks; // Declare wifiNetworks globally

AsyncWebServerResponse* prepareCORSResponse(AsyncWebServerRequest* request, int code, String contentType);
String contentType;
AsyncWebServer server(80);
AsyncEventSource events("/events");
const char* sensorDataFile = "/sensor_data.json";
void restartESP32Task(void *parameter);

// Handler para descargar el log
void handleDownloadLog(AsyncWebServerRequest *request) {
    // Require Basic Auth for admin download actions
    if (webUsername.length() > 0 && !request->authenticate(webUsername.c_str(), webPassword.c_str())) {
        return request->requestAuthentication();
    }

    if (!SPIFFS.exists(LOG_FILE)) {
        request->send(404, "text/plain", "Archivo error.log no encontrado");
        return;
    }

    File file = SPIFFS.open(LOG_FILE, "r");
    if (!file || file.isDirectory()) {
        request->send(500, "text/plain", "Error al abrir el archivo");
        return;
    }

    // Configurar los headers correctamente y forzar descarga
    AsyncWebServerResponse *response = request->beginResponse(
        SPIFFS,
        LOG_FILE,
        "text/plain",
        true // fuerza como attachment
    );

    response->addHeader("Content-Disposition", "attachment; filename=\"error.log\"");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
}

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

#include "VoltageMonitor.h"

String getSensorJson() {
    JsonDocument doc;
    
    if(xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1000))) {
        doc["temp"] = iaqSensor.temperature;
        doc["humidity"] = iaqSensor.humidity;
        xSemaphoreGive(sensorMutex);
    }

    // Añadir estado de batería/voltaje para el frontend
    float v = getMeasuredVoltage();
    String status = getBatteryStatus();
    doc["battery_voltage"] = v;
    doc["battery_status"] = status; // "absent", "undervoltage", "ok", "unknown"
    
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
                    writeLog("[WiFi] Error en escaneo de redes");
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
        writeLog("[WiFi] Error al tomar el mutex de escaneo");
    }
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
    // Ensure headers that keep event-streams open and prevent caching.
    AsyncWebServerResponse *response = request->beginResponse(200, "text/event-stream", "");
    response->addHeader("Connection", "keep-alive");
    response->addHeader("Cache-Control", "no-cache");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
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
        writeLog("[WiFi] Error al cargar redes guardadas");
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

void handleWiFiScan(AsyncWebServerRequest* request) {
    if(otaInProgress) {
        request->send(503, "text/plain", "Actualización OTA en progreso.");
        return;
    }

    // Comprobar estado del escaneo asíncrono
    int scanRes = WiFi.scanComplete();
    if (scanRes == -2) {
        // No iniciado: arrancar escaneo asíncrono y devolver lista vacía temporalmente
        Serial.println("📤 Escaneo no iniciado. Lanzando WiFi.scanNetworks(true) y devolviendo [].");
        WiFi.scanNetworks(true);
        request->send(200, "application/json", "[]");
        return;
    }

    if (scanRes == 0) {
        // Sin redes encontradas
        request->send(200, "application/json", "[]");
        return;
    }

    if (scanRes > 0) {
        Serial.printf("📡 Escaneo completado: %d redes encontradas\n", scanRes);
        JsonDocument doc;
        JsonArray jsonNetworks = doc.to<JsonArray>();

        for (int i = 0; i < scanRes; ++i) {
            JsonObject obj = jsonNetworks.add<JsonObject>();
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            int enc = WiFi.encryptionType(i);
            obj["ssid"] = ssid;
            obj["rssi"] = rssi;
            obj["secured"] = enc != WIFI_AUTH_OPEN;
        }

        // Opcional: limpiar resultados en la pila de escaneo
        WiFi.scanDelete();

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        request->send(200, "application/json", jsonResponse);
        return;
    }

    // Estado de escaneo en progreso u otro: devolver la caché si la tenemos
    JsonDocument doc;
    JsonArray jsonNetworks = doc.to<JsonArray>();
    for (const auto& net : networks) {
        JsonObject obj = jsonNetworks.add<JsonObject>();
        obj["ssid"] = net.ssid;
        obj["rssi"] = net.rssi;
        obj["secured"] = net.encryptionType != WIFI_AUTH_OPEN;
    }
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
}

void handleWiFiSave(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) { //WiFiManager::saveNetwork(const WiFiNetwork& network)
    Serial.println("🔍 handleWiFiSave fue llamado.");

    // Convertir los datos recibidos en una cadena
    String body = "";
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    /* Serial.println("📩 JSON Recibido:");
    Serial.println(body); */

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        Serial.print("❌ Error de parseo JSON: ");
        Serial.println(error.f_str());
        writeLog("❌ Error de parseo JSON: " + String(error.f_str()));
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
    writeLog(saved ? "✅ Red WiFi guardada correctamente." : "❌ Error al guardar la red WiFi.");
    

    if (saved) {
        request->send(200, "application/json", "{\"status\":\"success\"}");
        //delay(1000);
        //ESP.restart();
        shouldRestart = true;
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
        // Proporciona mensajes de error basados en la existencia/lectura de /config.json
        if (!SPIFFS.exists(configFilePath)) {
            writeLog(String("❌ Archivo ") + configFilePath + " no existe");
            return "Archivo config.json no existe";
        }

        File file = SPIFFS.open(configFilePath, FILE_READ);
        if (!file) {
            writeLog(String("❌ No se pudo abrir ") + configFilePath);
            return "No se pudo abrir config.json";
        }

        if (file.size() == 0) {
            file.close();
            writeLog(String("❌ ") + configFilePath + " está vacío");
            return "config.json está vacío";
        }

        file.close();
        return "Error desconocido al guardar en config.json";
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
    //delay(1000);
    //ESP.restart();
    shouldRestart = true;
}

void handleOTA(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
    static size_t totalSize = 0;
    if(!filename.endsWith(".bin")) {
        request->send(400, "text/plain", "Solo se permiten archivos .bin");
        return;
    }

    if (!index) {
        Serial.println("🚨 🔄 Suspendiendo procesos...");
        disableWatchdog(); // Desactiva el Watchdog y suspende la tarea
        
        size_t firmwareSize = request->contentLength();
        Serial.printf("📥 Iniciando OTA: %s (%d bytes)\n", filename.c_str(), firmwareSize);
        signalLed(LED_PROGRESS);
        
        if (!Update.begin(firmwareSize, U_FLASH)) {
            Serial.println("❌ No se pudo iniciar la OTA");
            writeLog("❌ No se pudo iniciar la OTA");
            signalLed(LED_ERROR);
            request->send(500, "text/plain", "Error al iniciar actualización");
            enableWatchdog(); // Reactiva el Watchdog y reanuda la tarea 
            return;
        }
        totalSize = 0;
    }

    size_t written = Update.write(data, len);
    totalSize += written;

    if (written != len) {
        Serial.println("❌ Error al escribir en Flash");
        writeLog("❌ Error al escribir en Flash");
        signalLed(LED_ERROR);
        request->send(500, "text/plain", "Error al escribir en Flash");
        otaInProgress = false;
        return;
    }

    if (final) {
        Serial.println("🔄 Finalizando actualización...");

        if (Update.hasError()) {
            Serial.println("❌ Error en la transferencia OTA.");
            writeLog("❌ Error en la transferencia OTA.");
            signalLed(LED_ERROR);
            request->send(500, "text/plain", "Error en la transferencia OTA.");
            otaInProgress = false;
            return;
        }

        if (Update.end(true)) {
            Serial.println("✅ Firmware actualizado correctamente. Reiniciando...");
            request->send(200, "text/plain", "✅ OTA completada. Reiniciando...");
            signalLed(LED_SUCCESS);
            delay(1000);
            ESP.restart();
        } else {
            Serial.println("❌ Error finalizando OTA");
            writeLog("❌ Error finalizando OTA.");
            signalLed(LED_ERROR);
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
        if (!SPIFFS.exists("/index.html")) {
            request->send(404, "text/plain", "File Not Found");
            return;
        }
        request->send(SPIFFS, "/index.html", "text/html");
    });
    
    server.on("/chart.umd.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/chart.umd.min.js", "application/javascript");
      });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(204); // Respuesta vacía (No Content)
    });
    // AsyncEventSource provides the /events endpoint via the handler below.
    // Avoid registering a separate handler that sends a plain 200 which would close the connection.
    server.addHandler(&events);
    server.on("/api/savedNetworks", HTTP_GET, handleSavedNetworks);
    server.on("/api/wifi/scanInternal", HTTP_GET, [](AsyncWebServerRequest *request){
        handleWiFiScanInternal(request);
    });
    server.on("/api/wifi/scan", HTTP_GET, handleWiFiScan);
    server.on("/saveWiFi", HTTP_POST, [](AsyncWebServerRequest *request) {},
          NULL, handleWiFiSave);

    // Endpoint: Backup (descarga de config.json)
    server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request){
        if (webUsername.length() > 0 && !request->authenticate(webUsername.c_str(), webPassword.c_str())) {
            return request->requestAuthentication();
        }
        if (!SPIFFS.exists(configFilePath)) {
            request->send(404, "application/json", "{\"error\":\"config.json not found\"}");
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(
            SPIFFS,
            configFilePath,
            "application/json",
            true // attachment
        );
        response->addHeader("Content-Disposition", "attachment; filename=\"config_backup.json\"");
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });

    // Endpoint: Restore (subida y reemplazo de config.json)
    server.on("/api/restore", HTTP_POST,
        [](AsyncWebServerRequest *request){
            // finalize handler is empty because upload handler will manage write and response
            request->send(400, "application/json", "{\"error\":\"Use multipart file upload to /api/restore\"}");
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        // Filename may be arbitrary; write chunks to temporary file
        const char* tmpPath = "/config_tmp.json";
        if (!index) {
            // First chunk - create/overwrite file
            File f = SPIFFS.open(tmpPath, FILE_WRITE);
            if (!f) {
                writeLog("❌ Error al crear archivo temporal de restauración");
                request->send(500, "application/json", "{\"error\":\"Cannot create temp file\"}");
                return;
            }
            f.close();
        }
        // Append chunk
        File f = SPIFFS.open(tmpPath, FILE_APPEND);
        if (!f) {
            writeLog("❌ Error al abrir archivo temporal para append");
            request->send(500, "application/json", "{\"error\":\"Cannot open temp file\"}");
            return;
        }
        f.write(data, len);
        f.close();

        if (final) {
            // Validate JSON
            File tf = SPIFFS.open(tmpPath, FILE_READ);
            if (!tf) {
                writeLog("❌ Error al leer archivo temporal de restauración");
                request->send(500, "application/json", "{\"error\":\"Cannot read temp file\"}");
                return;
            }

            // Parse into dynamic doc
            const size_t buf = tf.size() + 1024;
            DynamicJsonDocument doc(buf);
            DeserializationError err = deserializeJson(doc, tf);
            tf.close();
            if (err) {
                writeLog(String("❌ Error parseando JSON de restore: ") + err.c_str());
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                SPIFFS.remove(tmpPath);
                return;
            }

            // Basic validation: expect at least one of known keys
            if (!doc.containsKey("mqttServer") && !doc.containsKey("location") && !doc.containsKey("savedNetworks")) {
                writeLog("❌ JSON de restauración no contiene claves esperadas");
                request->send(400, "application/json", "{\"error\":\"JSON missing expected keys\"}");
                SPIFFS.remove(tmpPath);
                return;
            }

            // Overwrite /config.json
            File cf = SPIFFS.open(configFilePath, FILE_WRITE);
            if (!cf) {
                writeLog("❌ No se pudo abrir /config.json para escritura");
                request->send(500, "application/json", "{\"error\":\"Cannot write config.json\"}");
                SPIFFS.remove(tmpPath);
                return;
            }
            // Re-open temp and copy
            File tf2 = SPIFFS.open(tmpPath, FILE_READ);
            if (!tf2) {
                cf.close();
                writeLog("❌ No se pudo reabrir temp file para copiar");
                request->send(500, "application/json", "{\"error\":\"Cannot read temp file\"}");
                SPIFFS.remove(tmpPath);
                return;
            }
            // Copy contents
            while (tf2.available()) {
                cf.write(tf2.read());
            }
            tf2.close();
            cf.close();
            SPIFFS.remove(tmpPath);

            // Reload config into runtime and persist via saveConfig to normalize format
            if (!loadConfig()) {
                writeLog("⚠️ Restauración: no se pudo recargar config.json después de sobrescribir");
            } else {
                // Re-save to ensure normalized formatting
                if (!saveConfig(config)) writeLog("⚠️ Restauración: no se pudo guardar config.json tras validar");
            }

            // Log and respond
            writeLog("✅ Restauración de configuración completada. Reiniciando...");
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Restauración exitosa. Reiniciando...\"}");

            // Programar reinicio en tarea separada para no bloquear
            xTaskCreate([](void*){
                vTaskDelay(pdMS_TO_TICKS(1000));
                ESP.restart();
                vTaskDelete(NULL);
            }, "restart_after_restore", 2048, NULL, 1, NULL);
        }
    });
    
    server.on("/sensor_data", HTTP_GET, handleSensorData);
    server.on("/esp_status", HTTP_GET, handleESPStatus);
    server.on("/restart", HTTP_POST, handleRestart);
    // Use the dedicated handler that opens the file and sets Content-Disposition
    server.on("/downloadLog", HTTP_GET, handleDownloadLog);
server.on("/logview", HTTP_GET, [](AsyncWebServerRequest *request){
    if(SPIFFS.exists(LOG_FILE)) {
        request->send(SPIFFS, LOG_FILE, "text/plain");
    } else {
        request->send(200, "text/plain", "El archivo de log no existe");
    }
});
    server.on("/getConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        
        // Campos Generales
        doc["location"] = config.location;
        doc["altitude"] = config.altitude;
        doc["thingSpeakAPIKey"] = config.thingSpeakAPIKey;
        doc["channelID"] = config.channelID;
        doc["telegramToken"] = config.telegramToken;
        doc["chatId"] = config.chatId;
        doc["webUsername"] = webUsername;
        doc["webPassword"] = webPassword;
        doc["updateOta"] = config.updateOta;
        
        // Campos MQTT (Nuevos)
        doc["mqttServer"] = config.mqttServer;
        doc["mqttPort"] = config.mqttPort;
        doc["mqttUser"] = config.mqttUser;
        doc["mqttPassword"] = config.mqttPassword;
        doc["mqttTopic"] = config.mqttTopic;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    
    // --- NUEVO POST CONFIG CON DEBUG ---
    server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){
        Serial.println("\n📨 RECIBIDA PETICIÓN POST /config"); // <--- DEBUG 1
        
        if(!isAuthenticated(request)) {
            Serial.println("⛔ Acceso denegado (Auth)");
            request->send(401, "text/plain", "Unauthorized");
            return;
        }
        
        if(!request->hasParam("plain", true)) {
            Serial.println("⚠️ Error: No hay cuerpo (body) en la petición");
            request->send(400, "text/plain", "No data");
            return;
        }

        String body = request->getParam("plain", true)->value();
        Serial.println("📦 JSON Recibido:"); // <--- DEBUG 2
        Serial.println(body);                // <--- DEBUG 3 (Veremos qué envía el navegador)

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);
        if(error) {
            Serial.print("❌ Error JSON: ");
            Serial.println(error.c_str());
            request->send(400, "text/plain", "JSON Error");
            return;
        }

        Config newConfig = config;

        // General
        if (!doc["location"].isNull()) newConfig.location = doc["location"].as<String>();
        if (!doc["altitude"].isNull()) newConfig.altitude = doc["altitude"].as<float>();
        if (!doc["thingSpeakAPIKey"].isNull()) newConfig.thingSpeakAPIKey = doc["thingSpeakAPIKey"].as<String>();
        if (!doc["channelID"].isNull()) newConfig.channelID = doc["channelID"].as<long>();
        if (!doc["telegramToken"].isNull()) newConfig.telegramToken = doc["telegramToken"].as<String>();
        if (!doc["chatId"].isNull()) newConfig.chatId = doc["chatId"].as<String>();
        
        if(!doc["updateOta"].isNull()) {
            long otaValue = doc["updateOta"];
            newConfig.updateOta = (otaValue < 100) ? otaValue * 3600000 : otaValue;
        }

        // MQTT (Nuevos)
        if (!doc["mqttServer"].isNull()) newConfig.mqttServer = doc["mqttServer"].as<String>();
        if (!doc["mqttPort"].isNull()) newConfig.mqttPort = doc["mqttPort"].as<int>();
        if (!doc["mqttUser"].isNull()) newConfig.mqttUser = doc["mqttUser"].as<String>();
        if (!doc["mqttPassword"].isNull()) newConfig.mqttPassword = doc["mqttPassword"].as<String>();
        if (!doc["mqttTopic"].isNull()) newConfig.mqttTopic = doc["mqttTopic"].as<String>();

        // Guardar
        if(saveConfig(newConfig)) {
            Serial.println("💾 Configuración guardada en SPIFFS correctamente.");
            Serial.println("🔄 Reiniciando en 1 segundo...");
            request->send(200, "text/plain", "Saved. Restarting...");
            shouldRestart = true; // Usamos la bandera segura
        } else {
            Serial.println("❌ Error al escribir en SPIFFS");
            request->send(500, "text/plain", "Save Error");
        }
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){});
    
    // OTA Handler
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200, "text/plain", "OTA..."); }, handleOTA);

    server.begin();
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
        writeLog("❌ Autenticación fallida en la petición de configuración");
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

    // Siempre incluir estado/voltaje de batería para que el frontend no reciba objetos incompletos
    float v = getMeasuredVoltage();
    String status = getBatteryStatus();
    // Si no hay lectura válida, marcar como ausente
    if (v <= 0.0f || status == "unknown") {
        status = "absent";
        v = 0.00;
    }
    doc["battery_voltage"] = v;
    doc["battery_status"] = status; // "absent", "undervoltage", "ok", "unknown"

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
        writeLog("[CRITICAL] Fallo al crear wifiMutex");
        ESP.restart();
    }
}

void initSensorMutex() {
    sensorMutex = xSemaphoreCreateMutex();
    if (sensorMutex == NULL) {
        Serial.println();
        Serial.println("❌ Error creando mutex de sensores");
        Serial.println();
        writeLog("❌ Error creando mutex de sensores");
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
    Serial.printf("Location: %s\n", config.location.c_str());
    Serial.printf("MQTT Server: %s\n", config.mqttServer.c_str());
    Serial.printf("MQTT Port: %d\n", config.mqttPort);
    Serial.printf("MQTT Topic: %s\n", config.mqttTopic.c_str());
    Serial.printf("UpdateOTA: %ld ms\n", config.updateOta);
    Serial.println("----------------------------");
}
