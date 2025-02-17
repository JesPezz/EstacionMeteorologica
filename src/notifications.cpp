#include "notifications.h"
#include <HTTPClient.h>


// Enviar mensaje por Telegram
void sendTelegramMessage(const String& message, const NotificationConfig& config) {
    if (config.telegramToken.isEmpty() || config.chatId.isEmpty()) {
        Serial.println("❌ Credenciales de Telegram no configuradas.");
        return;
    }

    MB_String url = "https://api.telegram.org/bot";
    url += config.telegramToken;
    url += "/sendMessage";

    MB_String payload = "chat_id=";
    payload += config.chatId;
    payload += "&text=";
    payload += message;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url.c_str());  // Usar `client` con `WiFiClientSecure`
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST(payload.c_str());

    if (httpCode == 200) {
        Serial.println("✅ Notificación enviada por Telegram.");
    } else {
        Serial.printf("❌ Error al enviar notificación por Telegram. Código: %d\n", httpCode);
    }

    http.end();
}

// Enviar correo electrónico
void sendEmailNotification(const String& message, const NotificationConfig& config) {
    if (config.emailSender.isEmpty() || config.emailPassword.isEmpty() || config.emailRecipient.isEmpty()) {
        Serial.println("❌ Credenciales de correo electrónico no configuradas.");
        return;
    }

    ESP_Mail_Session session;
    session.server.host_name = "smtp.gmail.com";  // Servidor SMTP de Gmail
    session.server.port = 465;                   // Puerto seguro
    session.login.email = config.emailSender;
    session.login.password = config.emailPassword;
    session.login.user_domain = "";

    SMTP_Message email;
    email.sender.name = "Estación Meteorológica";
    email.sender.email = config.emailSender;
    email.subject = "Notificación de Actualización OTA";
    email.addRecipient("Destinatario", config.emailRecipient);
    email.text.content = message;

    SMTPSession smtp;

    if (!smtp.connect(&session)) {
        Serial.println("❌ Error al conectar con el servidor SMTP.");
        return;
    }

    if (!MailClient.sendMail(&smtp, &email)) {
        Serial.println("❌ Error al enviar el correo electrónico.");
    } else {
        Serial.println("✅ Notificación enviada por correo electrónico.");
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

NotificationConfig convertToNotificationConfig(const Config& config) {
    NotificationConfig notificationConfig;
    notificationConfig.telegramToken = config.telegramToken;
    notificationConfig.chatId = config.chatId;
    notificationConfig.emailSender = config.emailSender;
    notificationConfig.emailPassword = config.emailPassword;
    notificationConfig.emailRecipient = config.emailRecipient;
    return notificationConfig;
}