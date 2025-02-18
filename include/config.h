#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <bsec.h>
#include <ESP_Mail_Client.h>

extern String webUsername;
extern String webPassword;

extern const char version[];  
extern const char nombreCodigo[];
extern MB_String githubAPIURL;



extern unsigned long lastUpdateCheck;  // Variable para almacenar el último tiempo de verificación
extern const unsigned long UPDATE_INTERVAL;  // Intervalo de actualización (6 horas)


// ✅ Estructura de configuración
// 🔹 Estructura principal de configuración del ESP32
struct Config {
    String ssid;
    String password;
    String googleSheetURL;
    String thingSpeakAPIKey;
    unsigned long updateInterval;  // En milisegundos
    unsigned long channelID;       // ID del canal en ThingSpeak
    String location;               // Ubicación para Google Sheets

    // 🔹 Datos de notificación (Telegram y Email)
    MB_String telegramToken;
    MB_String chatId;
    MB_String emailSender;
    MB_String emailPassword;
    MB_String emailRecipient;
};

// 🔹 Subestructura solo para notificaciones (extraída de `Config`)
struct NotificationConfig {
    MB_String telegramToken;
    MB_String chatId;
    MB_String emailSender;
    MB_String emailPassword;
    MB_String emailRecipient;
};


NotificationConfig extractNotificationConfig(const Config& config);

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
void testFlash();


#endif
