#include "WiFiManager.h"
#include "config.h"

// Implementa connectToWiFi, reconnectWiFi, checkWiFiConnection (copiar código original)

void connectToWiFi() {
    // Inicia la conexión WiFi
    WiFi.begin(ssid, password);
    
    // Espera hasta que esté conectado
    Serial.print("Conectando a WiFi ");
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 10) { 
      delay(1000);
      Serial.print(".");
      retries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Conectado a WiFi.");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Error: No se pudo conectar a WiFi después de varios intentos.");
    }
  }

  bool reconnectWiFi(void)
{
  // Intenta reconectar WiFi si no está conectado
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Conexión WiFi perdida. Intentando reconectar...");
    
    // Intenta reconectar al WiFi
    WiFi.begin(ssid, password);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 10) {
      delay(500);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi reconectado.");
      return true; // Reconexión exitosa
    } else {
      Serial.println("\nFallo la reconexión WiFi.");
      return false; // Reconexión fallida
    }
  }
  
  return true; // WiFi ya estaba conectado
}

void checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi desconectado. Intentando reconectar...");
      connectToWiFi(); // Llama a la función para intentar reconectar
    }
  }