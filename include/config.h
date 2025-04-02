#ifndef CONFIG_H
#define CONFIG_H
#pragma once
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <bsec.h>
#include <Preferences.h>
#include <freertos/semphr.h>
#include <freertos/FreeRTOS.h>
#include <vector>
#include <AsyncTCP.h>          
#include <ESPAsyncWebServer.h>

extern bool scanRequested;
extern unsigned long lastScanTime;
extern const int scanInterval;
extern String scannedNetworks;
extern TimerHandle_t sseTimer;  // Declaración externa
extern std::vector<String> storedReadings;
extern Preferences preferences;
extern SemaphoreHandle_t sensorMutex;
class AsyncEventSource;
extern AsyncEventSource events;
String getSensorJson();

// Definir la estructura WiFiNetwork
struct WiFiNetwork {
    char ssid[32];
    char password[64];
    int rssi;
    uint8_t encryptionType;
};

// Declarar el vector de redes como variable global
extern std::vector<WiFiNetwork> networks;

// ✅ Estructura global para datos del sensor
struct SensorData {
    float temperature;
    float humidity;
    float pressure;
    float iaq;
    uint8_t iaqAccuracy;
    bool dataValid = false;
};

extern SensorData currentSensorData; // Declaración global

extern const char* configFilePath;

extern const char* host;
extern const char* url;
extern const char* etagFilePath;

extern TaskHandle_t thingSpeakTaskHandle;

extern bool otaInProgress;
extern const char* indexURL;

extern String webUsername;
extern String webPassword;

extern const char* version;  
extern const char nombreCodigo[];
extern String githubAPIURL;


struct Config {
    String googleSheetURL;
    String thingSpeakAPIKey;
    String location;
    String telegramToken;
    String chatId;
    unsigned long updateInterval;
    unsigned long channelID;
    unsigned long updateOta;
};

    extern Config config;


    // Timing
extern unsigned long CHANNEL_UPDATE_INTERVAL;
extern unsigned long MONTH_IN_SECONDS;
extern unsigned long STATE_SAVE_PERIOD;
extern int LED_ON_DURATION_MS;
extern unsigned long lastUpdateCheck;

// Sensores BME680 y BSEC
extern Bsec iaqSensor;
extern uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE];
extern uint16_t stateUpdateCounter;
extern unsigned long lastChannelUpdate;
extern unsigned long lastSyncTime;
extern String output;
extern bool prevMinuteZero;   // Para envío cada minuto

// ✅ Funciones de configuración
bool loadConfig();
bool saveConfig(const Config &newConfig);
void initSPIFFS();
void printConfig();
void testFlash();


#endif
