#include "notifications.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// 🔹 Función para enviar mensaje por Telegram
void sendTelegramMessage(const MB_String& mensaje, const NotificationConfig& config) {
    if (config.telegramToken.length() == 0 || config.chatId.length() == 0) {
        Serial.println("❌ Telegram: Configuración no válida.");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();  

    HTTPClient http;
    MB_String url = "https://api.telegram.org/bot" + config.telegramToken + "/sendMessage?chat_id=" + config.chatId + "&text=" + mensaje;

    Serial.println(("📤 Enviando Telegram: " + url).c_str());
    http.begin(client, url.c_str());
    int httpCode = http.GET();
    http.end();

    if (httpCode == 200) {
        Serial.println("✅ Mensaje enviado por Telegram.");
    } else {
    MB_String code = httpCode;
        Serial.println(("❌ Error enviando Telegram. Código: " + code).c_str());
    }
}

// 🔹 Función para enviar email
void sendEmailNotification(const MB_String& subject, const NotificationConfig& config) {
    if (config.emailSender.length() == 0 || config.emailPassword.length() == 0 || config.emailRecipient.length() == 0) {
        Serial.println("❌ Email: Configuración no válida.");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();  

    HTTPClient http;
    MB_String url = "https://api.emailservice.com/send";  

    MB_String payload = "{";
    payload += "\"from\": \"" + config.emailSender + "\",";
    payload += "\"to\": \"" + config.emailRecipient + "\",";
    payload += "\"subject\": \"" + subject + "\",";
    payload += "\"body\": \"Notificación desde ESP32\"";
    payload += "}";

    Serial.println("📤 Enviando Email...");
    http.begin(client, url.c_str());
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(payload.c_str());
    http.end();

    if (httpCode == 200) {
        Serial.println("✅ Email enviado correctamente.");
    } else {
        MB_String code = httpCode;
        Serial.println(("❌ Error enviando Email. Código: " + code).c_str());
    }
}


void saveNotificationConfig(const NotificationConfig& config) {
    // Guardar las credenciales en config.json
    File file = SPIFFS.open("/config.json", "w");
    if (!file) {
        Serial.println("❌ Error al abrir el archivo de configuración para escritura.");
        return;
    }

    JsonDocument doc;
    doc["telegramToken"] = config.telegramToken;
    doc["chatId"] = config.chatId;
    doc["emailSender"] = config.emailSender;
    doc["emailPassword"] = config.emailPassword;
    doc["emailRecipient"] = config.emailRecipient;

    serializeJson(doc, file);
    file.close();
}