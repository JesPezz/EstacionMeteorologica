#ifndef CONFIG_H
#define CONFIG_H
#pragma once

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>
#include <bsec.h>         
#include <freertos/semphr.h> 
#include <freertos/timers.h> 

// --- Constantes Globales ---
extern const char* configFilePath;
extern const char* LOG_FILE;
extern const char* indexURL;
extern const char* version;
extern const char* host;
extern const char* url;
extern const char* etagFilePath;
extern const char nombreCodigo[]; 
extern String githubAPIURL;       

#define MAX_LOG_SIZE 10240 

// --- Estructuras ---
struct WiFiNetwork {
    char ssid[32];
    char password[64];
    int rssi;
    uint8_t encryptionType;
};

struct Config {
    String location;
    String telegramToken;
    String chatId;
    unsigned long updateOta;
    
    // MQTT
    String mqttServer;
    int mqttPort;
    String mqttUser;
    String mqttPassword;
    String mqttTopic;
};

// --- Variables Globales ---
extern Config config;
extern std::vector<WiFiNetwork> networks;
extern String webUsername;
extern String webPassword;

// Variables BSEC y Sistema
extern Bsec iaqSensor;            
extern uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE]; 
extern uint16_t stateUpdateCounter; 
extern unsigned long lastScanTime;
extern const int scanInterval;
extern String output;
extern bool scanRequested;
extern bool otaInProgress;        
extern unsigned long lastUpdateCheck; 
extern bool shouldRestart;

// RTOS Handles
extern SemaphoreHandle_t sensorMutex; 
extern TimerHandle_t sseTimer;        

// Funciones
void writeLog(const String &message);
void initSPIFFS();
bool loadConfig();
bool saveConfig(const Config &newConfig);
void printConfig();
void testFlash();
String getDateTimeString();

#endif