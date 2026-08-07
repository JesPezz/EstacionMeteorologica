#ifndef OFFLINE_MANAGER_H
#define OFFLINE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// DECLARACIÓN: Informa al compilador que esta variable existe en alguna parte.
extern const char* BACKLOG_FILE;

// Guarda el JSON actual en un archivo de texto en SPIFFS
void saveToBacklog(JsonDocument& doc);

// Revisa si hay datos guardados y, si hay MQTT, los envía
void processBacklog();

#endif