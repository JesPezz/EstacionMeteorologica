#include "config.h"

String webUsername = "admin";  // Usuario por defecto
String webPassword = "admin123";  // Contraseña por defecto

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
    File file = SPIFFS.open("/settings.json", "r");
    if (!file) {
        Serial.println("❌ No se encontró el archivo de configuración.");
        return false;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.println("❌ Error al leer configuración JSON");
        return false;
    }

    // ✅ Cargar valores desde settings.json
    if (doc.containsKey("ssid")) config.ssid = doc["ssid"].as<String>();
    if (doc.containsKey("password")) config.password = doc["password"].as<String>();
    if (doc.containsKey("googleSheetURL")) config.googleSheetURL = doc["googleSheetURL"].as<String>();
    if (doc.containsKey("thingSpeakAPIKey")) config.thingSpeakAPIKey = doc["thingSpeakAPIKey"].as<String>();
    if (doc.containsKey("updateInterval")) config.updateInterval = doc["updateInterval"].as<int>() * 60000;
    if (doc.containsKey("channelID")) config.channelID = doc["channelID"].as<unsigned long>();
    if (doc.containsKey("location")) config.location = doc["location"].as<String>();
    if (doc.containsKey("webUsername")) webUsername = doc["webUsername"].as<String>();
    if (doc.containsKey("webPassword")) webPassword = doc["webPassword"].as<String>();


    // ✅ Asignar valores a las variables globales
    serverName = config.googleSheetURL;
    ssid = config.ssid.c_str();
    password = config.password.c_str();
    writeAPIKey = config.thingSpeakAPIKey.c_str();
    channelID = config.channelID;
    LOCATION = config.location;

    return true;
}

bool saveConfig(const Config &newConfig) {
    Serial.println("⚙️ Guardando nueva configuración...");

    StaticJsonDocument<512> doc;
    doc["ssid"] = newConfig.ssid.isEmpty() ? config.ssid : newConfig.ssid;
    doc["password"] = newConfig.password.isEmpty() ? config.password : newConfig.password;
    doc["googleSheetURL"] = newConfig.googleSheetURL.isEmpty() ? config.googleSheetURL : newConfig.googleSheetURL;
    doc["thingSpeakAPIKey"] = newConfig.thingSpeakAPIKey.isEmpty() ? config.thingSpeakAPIKey : newConfig.thingSpeakAPIKey;
    doc["updateInterval"] = (newConfig.updateInterval == 0) ? (config.updateInterval / 60000) : (newConfig.updateInterval / 60000);
    doc["channelID"] = (newConfig.channelID == 0) ? config.channelID : newConfig.channelID;
    doc["location"] = newConfig.location.isEmpty() ? config.location : newConfig.location;

    File file = SPIFFS.open("/settings.json", "w");
    if (!file) {
        Serial.println("❌ Error al abrir settings.json para escritura.");
        return false;
    }

    serializeJson(doc, file);
    file.close();

    Serial.println("✅ Configuración guardada exitosamente.");
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


