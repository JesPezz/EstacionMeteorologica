#include "notifications.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// 🔹 Función para enviar mensaje por Telegram
void sendTelegramMessage(const String &mensaje, const Config &config) {
    if (config.telegramToken.isEmpty() || config.chatId.isEmpty()) {
        Serial.println("❌ Telegram: Configuración no válida.");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    
    // Obtener la IP del ESP32
    String ipAddress = WiFi.localIP().toString();

    // Construir el mensaje con la IP y ubicación
    String mensajeConInfo = mensaje + 
                            "\n📡 IP: " + ipAddress + 
                            "\n📍 Ubicación: " + config.location;

    // Construir la URL de Telegram
    String url = "https://api.telegram.org/bot" + config.telegramToken + 
                 "/sendMessage?chat_id=" + config.chatId + 
                 "&text=" + mensajeConInfo;

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
