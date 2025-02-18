#include "notifications.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "ESP_Mail_Client.h"

// 🔹 Función para enviar mensaje por Telegram
void sendTelegramMessage(const MB_String &mensaje, const Config &config) {
    MB_String telegramToken = config.telegramToken.c_str();
    MB_String chatId = config.chatId.c_str();
    if (config.telegramToken.length() == 0 || config.chatId.length() == 0) {
        Serial.println("❌ Telegram: Configuración no válida.");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();  

    HTTPClient http;
    MB_String url = MB_String("https://api.telegram.org/bot") + MB_String(config.telegramToken) + 
                 MB_String("/sendMessage?chat_id=") + MB_String(config.chatId) + 
                 MB_String("&text=") + MB_String(mensaje);


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
#include "ESP_Mail_Client.h"

void sendEmailNotification(const MB_String &subject, const Config &config) {
    MB_String emailSender = config.emailSender.c_str();
    MB_String emailPassword = config.emailPassword.c_str();
    MB_String emailRecipient = config.emailRecipient.c_str();

    SMTPSession smtp;
    ESP_Mail_Session session;
    
    session.server.host_name = "smtp.server.com";
    session.server.port = 465;
    session.login.email = emailSender;
    session.login.password = emailPassword;
    
    SMTP_Message message;
    message.sender.name = "ESP32 Notification";
    message.sender.email = emailSender;
    message.subject = subject.c_str();
    message.addRecipient("", emailRecipient);

    // 📌 Construir el cuerpo del mensaje con datos de `config`
    MB_String emailBody;
    emailBody += "🔧 ESP32 Notificación OTA\n";
    emailBody += "SSID: " + MB_String(config.ssid) + "\n";
    emailBody += "Ubicación: " + MB_String(config.location) + "\n";
    emailBody += "Google Sheets URL: " + MB_String(config.googleSheetURL) + "\n";
    emailBody += "ThingSpeak API: " + MB_String(config.thingSpeakAPIKey) + "\n";
    emailBody += "Canal ID: " + MB_String(config.channelID) + "\n";

    message.text.content = emailBody.c_str();

    if (!smtp.connect(&session)) {
        Serial.println("❌ Error al conectar con SMTP.");
        return;
    }

    if (MailClient.sendMail(&smtp, &message)) {
        Serial.println("✅ Notificación enviada correctamente.");
    } else {
        Serial.println("❌ Error al enviar email.");
        Serial.println(smtp.errorReason());  // 🔹 Muestra el error específico en el puerto serie.
    }
 }

void saveNotificationConfig() {
    // Guardar las credenciales en config.json
    File file = SPIFFS.open("/config.json", "w");
    if (!file) {
        Serial.println("❌ Error al abrir el archivo de configuración para escritura.");
        return;
    }

    StaticJsonDocument<512> doc;
    doc["telegramToken"] = config.telegramToken;
    doc["chatId"] = config.chatId;
    doc["emailSender"] = config.emailSender;
    doc["emailPassword"] = config.emailPassword;
    doc["emailRecipient"] = config.emailRecipient;

    serializeJson(doc, file);
    file.close();
  }