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
const char* version = "v4.0.0-MQTT"; 

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
    JsonDocument doc;
    File file = SPIFFS.open(configFilePath, "r");
    if (!file) return false;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;

    if (doc["location"].is<String>()) config.location = doc["location"].as<String>();
    if (doc["telegramToken"].is<String>()) config.telegramToken = doc["telegramToken"].as<String>();
    if (doc["chatId"].is<String>()) config.chatId = doc["chatId"].as<String>();
    if (doc["webUsername"].is<String>()) webUsername = doc["webUsername"].as<String>();
    if (doc["webPassword"].is<String>()) webPassword = doc["webPassword"].as<String>();
    
    if (doc["updateOta"].is<unsigned long>()) config.updateOta = doc["updateOta"].as<unsigned long>() * 3600000;
    else config.updateOta = 3600000;

    // MQTT
    if (doc["mqttServer"].is<String>()) config.mqttServer = doc["mqttServer"].as<String>();
    config.mqttPort = doc["mqttPort"] | 1883;
    if (doc["mqttUser"].is<String>()) config.mqttUser = doc["mqttUser"].as<String>();
    if (doc["mqttPassword"].is<String>()) config.mqttPassword = doc["mqttPassword"].as<String>();
    if (doc["mqttTopic"].is<String>()) config.mqttTopic = doc["mqttTopic"].as<String>();

    return true;
}

bool saveConfig(const Config& newConfig) {
    JsonDocument doc;
    doc["location"] = newConfig.location;
    doc["telegramToken"] = newConfig.telegramToken;
    doc["chatId"] = newConfig.chatId;
    doc["webUsername"] = webUsername;
    doc["webPassword"] = webPassword;
    doc["updateOta"] = newConfig.updateOta / 3600000;
    
    doc["mqttServer"] = newConfig.mqttServer;
    doc["mqttPort"] = newConfig.mqttPort;
    doc["mqttUser"] = newConfig.mqttUser;
    doc["mqttPassword"] = newConfig.mqttPassword;
    doc["mqttTopic"] = newConfig.mqttTopic;

    File file = SPIFFS.open(configFilePath, "w");
    if (!file) return false;
    serializeJson(doc, file);
    file.close();
    return true;
}

void printConfig() { Serial.println("Config Cargada"); }
void testFlash() { SPIFFS.begin(); }