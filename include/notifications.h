#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino.h>           // Para String y funciones básicas
#include <ArduinoJson.h>       // Para manejar JSON (en caso de enviar datos estructurados)
#include <SPIFFS.h>            // Para manejar el sistema de archivos (si lees configuraciones)
#include <FS.h>                // Para acceso a archivos en SPIFFS
#include <WiFiClientSecure.h>  // Para conexiones seguras (ej. enviar correos con SSL)
#include "config.h"

void sendTelegramMessage(const String &mensaje, const Config &config); 
void saveNotificationConfig();
#endif