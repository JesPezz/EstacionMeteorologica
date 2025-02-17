#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>


// 🔹 Función para verificar y aplicar actualizaciones OTA desde un servidor HTTP
void checkForUpdates();

// 🔹 Manejo de la subida de firmware desde la interfaz web
void handleOTA(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);

void downloadAndUpdate(MB_String githubAPIURL, const NotificationConfig& notificationConfig);

#endif
