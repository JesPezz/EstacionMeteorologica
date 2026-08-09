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
extern const char* version;
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
    float altitude;
    String telegramToken;
    String chatId;
    unsigned long updateOta;
    String thingSpeakAPIKey;
    long channelID;
    
    // MQTT
    String mqttServer;
    int mqttPort;
    String mqttUser;
    String mqttPassword;
    String mqttTopic;

    // Voltage monitoring (hardware)
    int vbatPin;                  // ADC pin used to measure battery/rail via voltage divider
    float vdivRatio;              // Voltage divider ratio (Vout = Vin / vdivRatio)
    float voltageThreshold;       // Voltage threshold (Volts) below which a fault is logged
    unsigned long voltageCheckIntervalMs; // How often to check voltage (ms)
    float minDetectVoltage;        // Voltage below which we assume battery is absent (e.g., 0.2 V)

    // Redes WiFi guardadas (ssid + password)
    std::vector<WiFiNetwork> savedNetworks;
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
extern SemaphoreHandle_t spiffsMutex; // Serializa acceso a SPIFFS (evita panics por acceso concurrente)        

// Funciones
void writeLog(const String &message);
void initSPIFFS();
bool loadConfig();
bool saveConfig(const Config &newConfig);
void printConfig();
void testFlash();
String getDateTimeString();
bool getTestMode();
void setTestMode(bool enable);

#endif