#include "WiFiManager.h"
#include "config.h"
#include <WiFi.h>
#include "notifications.h"
#include "GoogleSheetManager.h"


void connectToWiFi() {

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid.c_str(), config.password.c_str());

    Serial.print("Conectando a WiFi ");
    
    
    int attempts = 0;
    
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ Conectado a WiFi.");
        Serial.println("📡 IP del ESP32: "+ WiFi.localIP().toString());
                                
    } else {
        Serial.println("\n❌ No se pudo conectar. Activando Modo AP...");
        startAPMode();
    }
}
void startAPMode() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_Config", "12345678");

    Serial.println("🔗 Conéctate a 'ESP32_Config' y accede a:");
    Serial.println(WiFi.softAPIP());
}

bool reconnectWiFi() {
  Serial.println("🔄 Intentando reconectar a WiFi...");
  WiFi.disconnect();
  delay(1000);
  WiFi.reconnect();

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(1000);
      Serial.print(".");
      attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ Reconectado a WiFi.");
      return true;
  } else {
      Serial.println("\n❌ No se pudo reconectar a WiFi.");
      return false;
  }
}

void checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ WiFi desconectado. Intentando reconectar...");
        if (reconnectWiFi()) {
            unsigned long startTime = millis();
            while (millis() - startTime < 5000) { // Esperar 5 segundos para estabilizar la conexión
                delay(100);
            }
            sendAllReadingsToGoogleSheet(); // 🔄 Enviar datos guardados después de reconectar
        }
    }
}
