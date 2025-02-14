#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <bsec.h>

extern String webUsername;
extern String webPassword;

// ✅ Estructura de configuración
struct Config {
    String ssid;
    String password;
    String googleSheetURL;
    String thingSpeakAPIKey;
    unsigned long updateInterval;  // Ahora en milisegundos
    unsigned long channelID;  // Nuevo campo para ThingSpeak
    String location;  // Nuevo campo para Google Sheets
};

extern Config config;  // ✅ Declaramos la variable global

// WiFi
extern String serverName;
extern const char* ssid;
extern const char* password;

// ThingSpeak
extern unsigned long channelID;
extern const char *writeAPIKey;
extern String LOCATION;

// Timing
extern unsigned long CHANNEL_UPDATE_INTERVAL;
extern unsigned long MONTH_IN_SECONDS;
extern unsigned long STATE_SAVE_PERIOD;
extern int LED_ON_DURATION_MS;

// Sensores BME680 y BSEC
extern Bsec iaqSensor;
extern uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE];
extern uint16_t stateUpdateCounter;
extern unsigned long lastChannelUpdate;
extern unsigned long lastSyncTime;
extern String output;
extern bool prevMinuteZero;

// ✅ Funciones de configuración
bool loadConfig();
bool saveConfig(const Config &newConfig);
void initSPIFFS();
void printConfig();

#endif
