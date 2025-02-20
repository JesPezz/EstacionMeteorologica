#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <bsec.h>


extern const char* indexURL;
extern const char* lastModifiedPath;

extern String webUsername;
extern String webPassword;

extern const char version[];  
extern const char nombreCodigo[];
extern String githubAPIURL;




extern unsigned long lastUpdateCheck;  // Variable para almacenar el último tiempo de verificación
extern const unsigned long UPDATE_INTERVAL;  // Intervalo de actualización (6 horas)
extern const unsigned long UPDATE_INTERVAL1;


// ✅ Estructura de configuración

struct Config {
    String ssid;
    String password;
    String googleSheetURL;
    String thingSpeakAPIKey;
    unsigned long updateInterval;
    unsigned long channelID;
    String location;
    String telegramToken;
    String chatId;
    String emailSender;
    String emailPassword;
    String emailRecipient;
};

// 🔹 Subestructura solo para notificaciones (extraída de `Config`)
struct NotificationConfig {
    String telegramToken;
    String chatId;
    String emailSender;
    String emailPassword;
    String emailRecipient;
};
    extern Config config;  
    extern NotificationConfig notificationConfig;  // ✅ Solo declaración

    NotificationConfig extractNotificationConfig(const Config& config);  // ✅ Declaración de la función (NO eliminar)




// WiFi
extern String googleSheetURL;
extern const char* ssid;
extern const char* password;

// ThingSpeak
extern unsigned long channelID;
extern const char *writeAPIKey;
extern String location;

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
void testFlash();


#endif
