#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include "config.h"

void disableWatchdog(); // Desactiva el Watchdog y suspende la tarea
void enableWatchdog(); // Reactiva el Watchdog y reanuda la tarea 
void checkForUpdates();
void downloadAndUpdate();

// 🔹 Manejo de la subida de firmware desde la interfaz web
void handleOTA(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);

#endif
