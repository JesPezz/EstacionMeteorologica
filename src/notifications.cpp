#include "notifications.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// 🔹 Función para enviar mensaje por Telegram
String urlEncode(const String &value) {
    String encoded = "";
    char c;
    char buf[4];
    for (size_t i = 0; i < value.length(); i++) {
        c = value.charAt(i);
        if (isalnum(c)) {
            encoded += c;  // Caracteres alfanuméricos quedan iguales
        } else if (c == ' ') {
            encoded += "%20";  // Espacios → %20
        } else if (c == '\n') {
            encoded += "%0A";  // Saltos de línea → %0A
        } else {
            snprintf(buf, sizeof(buf), "%%%02X", c);  // Otros caracteres → %HEX
            encoded += buf;
        }
    }
    return encoded;
}

void sendTelegramMessage(const String &mensaje, const Config &config) {
    if (config.telegramToken.isEmpty() || config.chatId.isEmpty()) {
        Serial.println("❌ Telegram: Configuración no válida.");
        writeLog("❌ Telegram: Configuración no válida.");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    
    // Obtener la IP del ESP32
    String ipAddress = WiFi.localIP().toString();

    // Construir el mensaje con la IP y ubicación
    String mensajeConInfo = mensaje + "\n📡 IP: " + ipAddress + "\n📍 Ubicación: " + config.location;

    // 📌 Aplicar `urlEncode()` para evitar problemas de caracteres especiales y saltos de línea
    String mensajeCodificado = urlEncode(mensajeConInfo);

    // Construir la URL correctamente codificada
    String url = "https://api.telegram.org/bot" + config.telegramToken + 
                 "/sendMessage?chat_id=" + config.chatId + 
                 "&text=" + mensajeCodificado;

    Serial.println("📤 Enviando Telegram: " + url);
    
    http.begin(client, url);
    int httpCode = http.GET();
    http.end();

    if (httpCode == 200) {
        Serial.println("✅ Mensaje enviado por Telegram.");
    } else {
        Serial.println("❌ Error enviando Telegram. Código: " + String(httpCode));
        writeLog("❌ Error enviando Telegram. Código: " + String(httpCode));
    }
}


void saveNotificationConfig() {
    // Guardar las credenciales en config.json
    File file = SPIFFS.open("/config.json", "w");
    if (!file) {
        Serial.println("❌ Error al abrir el archivo de configuración para escritura.");
        writeLog("❌ Error al abrir el archivo de configuración para escritura: /config.json");
        return;
    }

    JsonDocument doc;
    doc["telegramToken"] = config.telegramToken;
    doc["chatId"] = config.chatId;
    
    serializeJson(doc, file);
    file.close();
}
