#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <bsec.h>
#include <Preferences.h>

extern const char* configFilePath;

extern const char* host;
extern const char* url;
extern const char* etagFilePath;

extern TaskHandle_t thingSpeakTaskHandle;

extern bool otaInProgress;
extern const char* indexURL;

extern String webUsername;
extern String webPassword;

extern const char version[];  
extern const char nombreCodigo[];
extern String githubAPIURL;




// extern unsigned long lastUpdateCheck;  // Variable para almacenar el último tiempo de verificación
// extern const unsigned long updateOta;  // Intervalo de actualización (6 horas)



// ✅ Estructura de configuración

struct Config {
    String ssid;
    String password;
    String googleSheetURL;
    String thingSpeakAPIKey;
    unsigned long updateInterval;
    unsigned long channelID;
    unsigned long updateOta;
    String location;
    String telegramToken;
    String chatId;
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
extern bool prevFiveMinutes;  // Para envío cada 5 minutos
extern bool prevTenMinutes;   // Para envío cada 10 minutos
extern bool prevMinuteZero;   // Para envío cada minuto

// ✅ Funciones de configuración
bool loadConfig();
bool saveConfig(const Config &newConfig);
void initSPIFFS();
void printConfig();
void testFlash();


#endif
