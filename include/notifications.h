#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ESP-Mail-Client.h>

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

#endif