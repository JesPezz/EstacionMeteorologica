#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include "config.h"

void disableWatchdog();
void enableWatchdog();
void checkForUpdates();
void downloadAndUpdate();
void checkForIndexUpdate();
bool updateFileFromURL(const char* url, const char* path);

// 🔹 Manejo de la subida de firmware desde la interfaz web
void handleOTA(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);

#endif
