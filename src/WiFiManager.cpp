#include "WiFiManager.h"
#include "config.h"
#include <WiFi.h>
#include "notifications.h"
#include "GoogleSheetManager.h"
#include <Preferences.h>
#include "FS.h"
#include "SPIFFS.h"

std::vector<WiFiNetwork> savedNetworks;
bool APmode = false;
unsigned long lastScanTime = 0;
const int scanInterval = 15000; // Escaneo cada 15 segundos

void printWiFiConfig() {
    File file = SPIFFS.open("/wifi.json", "r");
    if (!file) {
        Serial.println("❌ No se encontró wifi.json.");
        return;
    }

    Serial.println("📜 Configuración de redes WiFi:");
    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.println();
    file.close();
}


// 🔹 Guardar redes en wifi.json
bool saveWiFiCredentialsToFile(const String &ssid, const String &password) {
    savedNetworks.push_back({ssid, password, 0});
    
    JsonDocument doc;
    JsonArray networks = doc.to<JsonArray>();

    for (auto &net : savedNetworks) {
        JsonObject obj = networks.add<JsonObject>();
        obj["ssid"] = net.ssid;
        obj["password"] = net.password;
    }

    File file = SPIFFS.open("/wifi.json", "w");
    if (!file) return false;

    serializeJson(doc, file);
    file.close();
    return true;
}

// 🔹 Cargar redes desde wifi.json
void loadWiFiConfig() {
    savedNetworks.clear();
    File file = SPIFFS.open("/wifi.json", "r");
    if (!file) return;

    JsonDocument doc;
    deserializeJson(doc, file);
    file.close();

    for (JsonObject obj : doc.as<JsonArray>()) {
        savedNetworks.push_back({obj["ssid"].as<String>(), obj["password"].as<String>()});
    }
}

void scanWiFiNetworks() {
    Serial.println("🔍 Escaneando redes WiFi...");
    int numNetworks = WiFi.scanNetworks();
    
    if (numNetworks == 0) {
        Serial.println("❌ No se encontraron redes WiFi.");
    } else {
        Serial.println("✅ Redes encontradas:");
        for (int i = 0; i < numNetworks; i++) {
            Serial.printf("  %d: %s (%d dBm)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        }
    }
}


std::vector<WiFiNetwork> getAvailableNetworks() {
    std::vector<WiFiNetwork> networks;
    int numNetworks = WiFi.scanNetworks();
    for (int i = 0; i < numNetworks; i++) {
        WiFiNetwork net = {WiFi.SSID(i), "", WiFi.RSSI(i)};
        networks.push_back(net);
    }
    return networks;
}

// 🔹 Conectarse a la mejor red disponible
bool connectToBestWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    scanWiFiNetworks();
    loadWiFiConfig();

    WiFiNetwork bestNetwork;
    int bestSignal = -100;
    bool foundNetwork = false;

    for (auto &saved : savedNetworks) {
        for (int i = 0; i < WiFi.scanComplete(); i++) {
            if (WiFi.SSID(i) == saved.ssid && WiFi.RSSI(i) > bestSignal) {
                bestNetwork = saved;
                bestSignal = WiFi.RSSI(i);
                foundNetwork = true;
            }
        }
    }

    if (!foundNetwork) {
        Serial.println("❌ No se encontraron redes guardadas. Activando Modo AP...");
        startAPMode();
        return false;
    }

    Serial.println("📡 Conectando a la mejor red: " + bestNetwork.ssid);
    WiFi.begin(bestNetwork.ssid.c_str(), bestNetwork.password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ Conectado a WiFi.");
        Serial.println("📡 IP del ESP32: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("\n❌ No se pudo conectar. Forzando Modo AP...");
        WiFi.disconnect();
        delay(100);
        startAPMode();
        return false;
    }
}

void startAPMode() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_Config", "12345678");

    Serial.println("🔗 Conéctate a 'ESP32_Config' y accede a:");
    Serial.println(WiFi.softAPIP());
    APmode = true;

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
    if (APmode){
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ WiFi desconectado. Intentando reconectar...");
        reconnectWiFi();
        }
    }
