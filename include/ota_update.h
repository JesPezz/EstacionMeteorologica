#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include "config.h"


// 🔹 Función para verificar y aplicar actualizaciones OTA desde un servidor HTTP
void checkForUpdates();
void downloadAndUpdate();

// 🔹 Manejo de la subida de firmware desde la interfaz web
void handleOTA(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);

#endif
