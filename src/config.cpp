#include "config.h"

String webUsername = "admin";  // Usuario por defecto
String webPassword = "admin123";  // Contraseña por defecto

const char version[] = "v2.2";
const char nombreCodigo[] = "EstacionThingSpeak";

unsigned long lastUpdateCheck = 0;  // Inicializa lastUpdateCheck a 0
const unsigned long UPDATE_INTERVAL = 1 * 60 * 1000;  // 6 horas en milisegundos

MB_String githubAPIURL = "https://api.github.com/repos/JesPezz/EstacionMeteorologica/releases/latest";

Config config;
String serverName;
const char* ssid;
const char* password;
const char* writeAPIKey;
unsigned long channelID;
String LOCATION;
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
bool prevMinuteZero = false;

void initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("❌ Error al montar SPIFFS");
    }
}

bool loadConfig() {
    JsonDocument doc;  // Usar JsonDocument en lugar de StaticJsonDocument

    // Cargar el archivo JSON desde el sistema de archivos
    File file = SPIFFS.open("/config.json", "r");
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
    if (doc["ssid"].is<String>()) config.ssid = doc["ssid"].as<String>();
    if (doc["password"].is<String>()) config.password = doc["password"].as<String>();
    if (doc["googleSheetURL"].is<String>()) config.googleSheetURL = doc["googleSheetURL"].as<String>();
    if (doc["thingSpeakAPIKey"].is<String>()) config.thingSpeakAPIKey = doc["thingSpeakAPIKey"].as<String>();
    if (doc["updateInterval"].is<int>()) config.updateInterval = doc["updateInterval"].as<int>() * 60000;
    if (doc["channelID"].is<unsigned long>()) config.channelID = doc["channelID"].as<unsigned long>();
    if (doc["location"].is<String>()) config.location = doc["location"].as<String>();
    if (doc["webUsername"].is<String>()) webUsername = doc["webUsername"].as<String>();
    if (doc["webPassword"].is<String>()) webPassword = doc["webPassword"].as<String>();

    return true;
}

bool saveConfig(const Config& config) {
    JsonDocument doc;  // Usar JsonDocument en lugar de StaticJsonDocument

    // Asignar valores al JSON
    doc["ssid"] = config.ssid;
    doc["password"] = config.password;
    doc["googleSheetURL"] = config.googleSheetURL;
    doc["thingSpeakAPIKey"] = config.thingSpeakAPIKey;
    doc["updateInterval"] = config.updateInterval / 60000;  // Convertir a minutos
    doc["channelID"] = config.channelID;
    doc["location"] = config.location;
    doc["webUsername"] = webUsername;
    doc["webPassword"] = webPassword;

    // Guardar el JSON en el sistema de archivos
    File file = SPIFFS.open("/config.json", "w");
    if (!file) {
        Serial.println("Error al abrir el archivo de configuración para escritura.");
        return false;
    }

    serializeJson(doc, file);
    file.close();

    return true;
 }

void printConfig() {
    File file = SPIFFS.open("/settings.json", "r");
    if (!file) {
        Serial.println("❌ No se pudo abrir settings.json para lectura.");
        return;
    }

    Serial.println("📜 Configuración actual en settings.json:");
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