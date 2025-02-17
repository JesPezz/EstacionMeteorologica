#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "ESP_Mail_Client.h"
#include <HTTPClient.h>
#include <Arduino.h>           // Para String y funciones básicas
#include <ArduinoJson.h>       // Para manejar JSON (en caso de enviar datos estructurados)
#include <SPIFFS.h>            // Para manejar el sistema de archivos (si lees configuraciones)
#include <FS.h>                // Para acceso a archivos en SPIFFS
#include <WiFiClientSecure.h>  // Para conexiones seguras (ej. enviar correos con SSL)
#include "config.h"

// Estructura para almacenar las credenciales de notificación
struct NotificationConfig {
    String telegramToken;
    String chatId;
    String emailSender;
    String emailPassword;
    String emailRecipient;
};

// Declaraciones de funciones
void sendTelegramMessage(const String& message, const NotificationConfig& config);
void sendEmailNotification(const String& message, const NotificationConfig& config);
void saveNotificationConfig(const NotificationConfig& config);
NotificationConfig convertToNotificationConfig(const Config& config);
#endif