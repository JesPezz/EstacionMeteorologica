#include "notifications.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// 🔹 Función para enviar mensaje por Telegram
void sendTelegramMessage(const String &mensaje, const Config &config) {
    String telegramToken = config.telegramToken;
    String chatId = config.chatId;
    
    if (telegramToken.isEmpty() || chatId.isEmpty()) {
        Serial.println("❌ Telegram: Configuración no válida.");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();  

    HTTPClient http;
    String url = "https://api.telegram.org/bot" + telegramToken + 
                 "/sendMessage?chat_id=" + chatId + 
                 "&text=" + mensaje;

    Serial.println("📤 Enviando Telegram: " + url);
    http.begin(client, url);
    int httpCode = http.GET();
    http.end();

    if (httpCode == 200) {
        Serial.println("✅ Mensaje enviado por Telegram.");
    } else {
        Serial.println("❌ Error enviando Telegram. Código: " + String(httpCode));
    }
}

void saveNotificationConfig() {
    // Guardar las credenciales en config.json
    File file = SPIFFS.open("/config.json", "w");
    if (!file) {
        Serial.println("❌ Error al abrir el archivo de configuración para escritura.");
        return;
    }

    JsonDocument doc;
    doc["telegramToken"] = config.telegramToken;
    doc["chatId"] = config.chatId;
    
    serializeJson(doc, file);
    file.close();
}
