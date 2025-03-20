#include "config.h"
#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>

SemaphoreHandle_t sensorMutex = xSemaphoreCreateMutex(); // Crear el semáforo

const char* configFilePath = "/config.json";

const char* host = "raw.githubusercontent.com";
const char* url = "/JesPezz/EstacionMeteorologica/main/Data/index.html";
const char* etagFilePath = "/index_etag.txt";

TaskHandle_t thingSpeakTaskHandle = NULL;

const char* indexURL = "https://raw.githubusercontent.com/JesPezz/EstacionMeteorologica/main/Data/index.html";

String webUsername = "admin";  // Usuario por defecto
String webPassword = "admin123";  // Contraseña por defecto

const char* version = "v3.3";
const char nombreCodigo[] = "EstacionThingSpeak";

unsigned long lastUpdateCheck = 0;  // Inicializa lastUpdateCheck a 0


String githubAPIURL = "https://api.github.com/repos/JesPezz/EstacionMeteorologica/releases/latest";


Config config;


String googleSheetURL;
const char* ssid;
const char* password;
const char* thingSpeakAPIKey;
unsigned long channelID;
String location;
unsigned long CHANNEL_UPDATE_INTERVAL = 60 * 1000;
unsigned long MONTH_IN_SECONDS = 30 * 24 * 60 * 60;
unsigned long STATE_SAVE_PERIOD = 360 * 60 * 1000;
int LED_ON_DURATION_MS = 1000;

// ✅ Definir variables globales aquí
Bsec iaqSensor;
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;
unsigned long lastChannelUpdate = 0;
unsigned long lastSyncTime = 0;
String output;
bool prevFiveMinutes = false;  // Para envío cada 5 minutos
bool prevTenMinutes = false;   // Para envío cada 10 minutos
bool prevMinuteZero = false;   // Para envío cada minuto

void initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ Error al montar SPIFFS");
    } else {
        Serial.println("✅ SPIFFS montado correctamente.");
    }
}

bool loadConfig() {
    JsonDocument doc;  // Usar JsonDocument en lugar de StaticJsonDocument

    // Cargar el archivo JSON desde el sistema de archivos
    File file = SPIFFS.open(configFilePath, "r");
    if (!file) {
        Serial.println("Error al abrir el archivo de configuración.");
        return false;
    }

    // Parsear el JSON
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.println("Error al parsear el archivo de configuración.");
        file.close();
        return false;
    }

    file.close();

    // Leer valores del JSON
    if (doc["googleSheetURL"].is<String>()) config.googleSheetURL = doc["googleSheetURL"].as<String>();
    if (doc["thingSpeakAPIKey"].is<String>()) config.thingSpeakAPIKey = doc["thingSpeakAPIKey"].as<String>();
    if (doc["channelID"].is<unsigned long>()) config.channelID = doc["channelID"].as<unsigned long>();
    if (doc["location"].is<String>()) config.location = doc["location"].as<String>();
    if (doc["webUsername"].is<String>()) webUsername = doc["webUsername"].as<String>();
    if (doc["webPassword"].is<String>()) webPassword = doc["webPassword"].as<String>();
    if (doc["telegramToken"].is<String>()) config.telegramToken = doc["telegramToken"].as<String>();
    if (doc["chatId"].is<String>()) config.chatId = doc["chatId"].as<String>();
    if (doc["updateOta"].is<unsigned long>()) {
        config.updateOta = doc["updateOta"].as<unsigned long>() * 3600000;  // 🔹 Convertir horas → ms
    } else {
        Serial.println("⚠️ updateOta no encontrado, usando valor por defecto.");
        config.updateOta = 3600000;  // 🔹 1 hora por defecto
    }

    Serial.printf("✅ updateOta cargado desde config.json: %lu ms\n", config.updateOta);
    return true;
}

bool saveConfig(const Config& config) {
    JsonDocument doc;

    // Asignar valores al JSON
    doc["googleSheetURL"] = config.googleSheetURL;
    doc["thingSpeakAPIKey"] = config.thingSpeakAPIKey;
    doc["channelID"] = config.channelID;
    doc["location"] = config.location;
    doc["webUsername"] = webUsername;
    doc["webPassword"] = webPassword;
    doc["telegramToken"] = config.telegramToken;
    doc["chatId"] = config.chatId;
    doc["updateOta"] = config.updateOta / 3600000;  // 🔹 Guarda en horas

    // Guardar el JSON en el sistema de archivos
    File file = SPIFFS.open(configFilePath, "w");
    if (!file) {
        Serial.println("Error al abrir el archivo de configuración para escritura.");
        return false;
    }

    serializeJson(doc, file);
    file.close();

    return true;
 }

void printConfig() {
    File file = SPIFFS.open(configFilePath, "r");
    if (!file) {
        Serial.println("❌ No se pudo abrir settings.json para lectura.");
        return;
    }

    Serial.println("📜 Configuración actual en config.json:");
    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.println();
    file.close();
}

void testFlash() {
    Serial.println("🔍 Probando memoria flash...");
    if (!SPIFFS.begin()) {
        Serial.println("❌ Error: SPIFFS no inicializado.");
    } else {
        Serial.println("✅ SPIFFS funcionando correctamente.");
    }
}