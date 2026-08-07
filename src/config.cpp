#include "config.h"
#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include <bsec.h> 

// --- Definición de Variables ---
std::vector<WiFiNetwork> networks;
Config config;

const char* configFilePath = "/config.json";
const char* LOG_FILE = "/error.log";
const char* host = "raw.githubusercontent.com";
const char* url = "/JesPezz/EstacionMeteorologica/main/Data/index.html";
const char* etagFilePath = "/index_etag.txt";
const char* indexURL = "https://raw.githubusercontent.com/JesPezz/EstacionMeteorologica/main/Data/index.html";
const char nombreCodigo[] = "EstacionMQTT"; 
String githubAPIURL = "https://api.github.com/repos/JesPezz/EstacionMeteorologica/releases/latest"; 

String webUsername = "admin";
String webPassword = "admin123";
const char* version = "v4.3.4.3-MQTT";

unsigned long lastScanTime = 0;
const int scanInterval = 15000;
unsigned long lastUpdateCheck = 0;

// Variables RTOS
TimerHandle_t sseTimer = nullptr; 
SemaphoreHandle_t sensorMutex = NULL; 
bool otaInProgress = false; 
bool shouldRestart = false;

// Variables BSEC
Bsec iaqSensor; 
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0}; 
uint16_t stateUpdateCounter = 0; 

String output;
bool scanRequested = false;

// --- Funciones ---
String getDateTimeString() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        return "00-00-00 00:00:00";
    }
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
}

void writeLog(const String &message) {
    if (!SPIFFS.begin(true)) return;
    File logFile = SPIFFS.open(LOG_FILE, "a");
    if (!logFile) return;
    if (logFile.size() > MAX_LOG_SIZE) {
        logFile.close();
        SPIFFS.remove(LOG_FILE);
        logFile = SPIFFS.open(LOG_FILE, "a");
    }
    logFile.print("[" + getDateTimeString() + "] " + message + "\n");
    logFile.close();
}

void initSPIFFS() {
    if (!SPIFFS.begin(true)) Serial.println("❌ Error SPIFFS");
    else Serial.println("✅ SPIFFS OK");
}

bool loadConfig() {
    // Migrator: Si existe /wifi.json, migrar redes a /config.json y eliminar el archivo legacy
    if (SPIFFS.exists("/wifi.json")) {
        writeLog("ℹ️ Encontrado archivo legacy /wifi.json. Iniciando migración a /config.json...");
        File wf = SPIFFS.open("/wifi.json", "r");
        if (wf) {
            size_t sz = wf.size();
            size_t bufSize = sz + 1024;
            DynamicJsonDocument wdoc(bufSize);
            DeserializationError werr = deserializeJson(wdoc, wf);
            wf.close();
            if (!werr) {
                // Obtener array raíz o campo savedNetworks
                JsonArray arr;
                if (wdoc.is<JsonArray>()) arr = wdoc.as<JsonArray>();
                else if (wdoc["savedNetworks"].is<JsonArray>()) arr = wdoc["savedNetworks"].as<JsonArray>();

                if (!arr.isNull()) {
                    // Cargar config.json si existe para fusionar
                    if (SPIFFS.exists(configFilePath)) {
                        File cf = SPIFFS.open(configFilePath, "r");
                        if (cf) {
                            size_t csz = cf.size();
                            size_t cbuf = csz + 1024;
                            DynamicJsonDocument cdoc(cbuf);
                            if (!deserializeJson(cdoc, cf)) {
                                // Cargar savedNetworks existentes si hay
                                if (cdoc["savedNetworks"].is<JsonArray>()) {
                                    config.savedNetworks.clear();
                                    for (JsonObject o : cdoc["savedNetworks"].as<JsonArray>()) {
                                        WiFiNetwork wn;
                                        strlcpy(wn.ssid, o["ssid"] | "", sizeof(wn.ssid));
                                        strlcpy(wn.password, o["password"] | "", sizeof(wn.password));
                                        config.savedNetworks.push_back(wn);
                                    }
                                }
                            }
                            cf.close();
                        }
                    }

                    // Fusionar redes desde wifi.json evitando duplicados (por ssid)
                    for (JsonVariant v : arr) {
                        String ssid = "";
                        String pwd = "";
                        if (v.is<JsonObject>()) {
                            JsonObject jo = v.as<JsonObject>();
                            if (jo["ssid"].is<const char*>()) ssid = String((const char*)jo["ssid"]);
                            else if (jo["ssid"].is<String>()) ssid = jo["ssid"].as<String>();
                            if (jo["password"].is<const char*>()) pwd = String((const char*)jo["password"]);
                            else if (jo["password"].is<String>()) pwd = jo["password"].as<String>();
                        } else if (v.is<const char*>()) {
                            ssid = String((const char*)v.as<const char*>());
                        }

                        if (ssid.length() == 0) continue;
                        bool found = false;
                        for (const auto &e : config.savedNetworks) {
                            if (ssid.equals(String(e.ssid))) { found = true; break; }
                        }
                        if (!found) {
                            WiFiNetwork wn;
                            strlcpy(wn.ssid, ssid.c_str(), sizeof(wn.ssid));
                            strlcpy(wn.password, pwd.c_str(), sizeof(wn.password));
                            wn.rssi = 0;
                            wn.encryptionType = 0;
                            config.savedNetworks.push_back(wn);
                        }
                    }

                    // Persistir config fusionado
                    if (saveConfig(config)) {
                        writeLog("✅ Migración de wifi.json a config.json completada exitosamente.");
                        // Eliminar archivo legacy
                        SPIFFS.remove("/wifi.json");
                    } else {
                        writeLog("❌ Falló al guardar config.json tras migración.");
                    }
                }
            } else {
                writeLog(String("❌ Error parseando /wifi.json: ") + werr.c_str());
            }
        } else {
            writeLog("❌ No se pudo abrir /wifi.json para migración.");
        }
    }

    // Continuar con carga normal de config.json
    JsonDocument doc;
    File file = SPIFFS.open(configFilePath, "r");
    if (!file) return false;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;

    if (doc["location"].is<String>()) config.location = doc["location"].as<String>();
    if (doc["altitude"].is<float>()) config.altitude = doc["altitude"].as<float>();
    else config.altitude = 320.0;
    if (doc["telegramToken"].is<String>()) config.telegramToken = doc["telegramToken"].as<String>();
    if (doc["chatId"].is<String>()) config.chatId = doc["chatId"].as<String>();
    if (doc["webUsername"].is<String>()) webUsername = doc["webUsername"].as<String>();
    if (doc["webPassword"].is<String>()) webPassword = doc["webPassword"].as<String>();
    
    if (doc["updateOta"].is<unsigned long>()) config.updateOta = doc["updateOta"].as<unsigned long>() * 3600000;
    else config.updateOta = 3600000;
    if (doc["thingSpeakAPIKey"].is<String>()) config.thingSpeakAPIKey = doc["thingSpeakAPIKey"].as<String>();
    if (doc["channelID"].is<long>()) config.channelID = doc["channelID"].as<long>();

    // MQTT
    if (doc["mqttServer"].is<String>()) config.mqttServer = doc["mqttServer"].as<String>();
    config.mqttPort = doc["mqttPort"] | 1883;
    if (doc["mqttUser"].is<String>()) config.mqttUser = doc["mqttUser"].as<String>();
    if (doc["mqttPassword"].is<String>()) config.mqttPassword = doc["mqttPassword"].as<String>();
    if (doc["mqttTopic"].is<String>()) config.mqttTopic = doc["mqttTopic"].as<String>();

    // Voltage monitoring fields (optional in config.json)
    if (doc["vbatPin"].is<int>()) config.vbatPin = doc["vbatPin"].as<int>();
    else config.vbatPin = 35; // default ADC pin (change according to your hardware)

    if (doc["vdivRatio"].is<float>()) config.vdivRatio = doc["vdivRatio"].as<float>();
    else if (doc["vdivRatio"].is<int>()) config.vdivRatio = (float)doc["vdivRatio"].as<int>();
    else config.vdivRatio = 2.0; // default divider ratio (e.g., 2 => equal resistors)

    if (doc["voltageThreshold"].is<float>()) config.voltageThreshold = doc["voltageThreshold"].as<float>();
    else if (doc["voltageThreshold"].is<int>()) config.voltageThreshold = (float)doc["voltageThreshold"].as<int>();
    else config.voltageThreshold = 3.3; // default threshold in volts

    if (doc["voltageCheckIntervalMs"].is<unsigned long>()) config.voltageCheckIntervalMs = doc["voltageCheckIntervalMs"].as<unsigned long>();
    else if (doc["voltageCheckIntervalMs"].is<int>()) config.voltageCheckIntervalMs = (unsigned long)doc["voltageCheckIntervalMs"].as<int>();
    else config.voltageCheckIntervalMs = 60000; // default: 60s

    if (doc["minDetectVoltage"].is<float>()) config.minDetectVoltage = doc["minDetectVoltage"].as<float>();
    else if (doc["minDetectVoltage"].is<int>()) config.minDetectVoltage = (float)doc["minDetectVoltage"].as<int>();
    else config.minDetectVoltage = 0.2; // default: 0.2V

    // savedNetworks: cargar si existe
    if (doc["savedNetworks"].is<JsonArray>()) {
        config.savedNetworks.clear();
        for (JsonObject obj : doc["savedNetworks"].as<JsonArray>()) {
            WiFiNetwork wn;
            strlcpy(wn.ssid, obj["ssid"] | "", sizeof(wn.ssid));
            strlcpy(wn.password, obj["password"] | "", sizeof(wn.password));
            wn.rssi = 0;
            wn.encryptionType = 0;
            config.savedNetworks.push_back(wn);
        }
    }

    return true;
}

bool saveConfig(const Config& newConfig) {
    // Use a dynamic document sized reasonably for config + networks
    const size_t bufferSize = 8192;
    DynamicJsonDocument doc(bufferSize);
    doc["location"] = newConfig.location;
    doc["altitude"] = newConfig.altitude;
    doc["telegramToken"] = newConfig.telegramToken;
    doc["chatId"] = newConfig.chatId;
    doc["webUsername"] = webUsername;
    doc["webPassword"] = webPassword;
    doc["updateOta"] = newConfig.updateOta / 3600000;
    doc["thingSpeakAPIKey"] = newConfig.thingSpeakAPIKey;
    doc["channelID"] = newConfig.channelID;
    
    doc["mqttServer"] = newConfig.mqttServer;
    doc["mqttPort"] = newConfig.mqttPort;
    doc["mqttUser"] = newConfig.mqttUser;
    doc["mqttPassword"] = newConfig.mqttPassword;
    doc["mqttTopic"] = newConfig.mqttTopic;

    // Voltage monitoring fields
    doc["vbatPin"] = newConfig.vbatPin;
    doc["vdivRatio"] = newConfig.vdivRatio;
    doc["voltageThreshold"] = newConfig.voltageThreshold;
    doc["voltageCheckIntervalMs"] = newConfig.voltageCheckIntervalMs;
    doc["minDetectVoltage"] = newConfig.minDetectVoltage;

    // savedNetworks
    JsonArray arr = doc.createNestedArray("savedNetworks");
    for (const auto& wn : newConfig.savedNetworks) {
        JsonObject o = arr.createNestedObject();
        o["ssid"] = String(wn.ssid);
        o["password"] = String(wn.password);
    }

    File file = SPIFFS.open(configFilePath, "w");
    if (!file) return false;
    if (serializeJson(doc, file) == 0) {
        file.close();
        return false;
    }
    file.close();
    return true;
}

void printConfig() { Serial.println("Config Cargada"); }
void testFlash() { SPIFFS.begin(); }
