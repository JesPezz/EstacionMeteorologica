#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>

// 🔹 Función para verificar y aplicar actualizaciones OTA desde un servidor HTTP
void checkForUpdates();

// 🔹 Manejo de la subida de firmware
void handleOTA(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

#endif
