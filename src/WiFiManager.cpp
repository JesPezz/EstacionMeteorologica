#include "WiFiManager.h"
#include "config.h"
#include <WiFi.h>
#include "notifications.h"
#include "GoogleSheetManager.h"
#include <Preferences.h>
#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>

bool APmode = false;


void WiFiManager::scanNetworks(std::vector<WiFiNetwork>& networks) {
    Serial.println("🔍 Escaneando redes WiFi...");
    
    int numNetworks = WiFi.scanNetworks();
    networks.clear();

    if (numNetworks == 0) {
        Serial.println("❌ No se encontraron redes WiFi.");
    } else {
        Serial.println("✅ Redes encontradas:");
        for (int i = 0; i < numNetworks; ++i) {
            WiFiNetwork network;
            strlcpy(network.ssid, WiFi.SSID(i).c_str(), sizeof(network.ssid));
            network.rssi = WiFi.RSSI(i);
            network.encryptionType = WiFi.encryptionType(i);
            networks.push_back(network);

            // 📌 Imprimir en la consola como scanWiFiNetworks()
            Serial.printf("  %d: %s (%d dBm)\n", i + 1, network.ssid, network.rssi);
        }
    }

    WiFi.scanDelete(); // Liberar memoria del escaneo
}

bool WiFiManager::saveNetwork(const WiFiNetwork& network) {
    // Leer redes existentes
    std::vector<WiFiNetwork> existingNetworks;
    loadSavedNetworks(existingNetworks);

    // Evitar duplicados
    for (const auto& net : existingNetworks) {
        if (strcmp(net.ssid, network.ssid) == 0) return false;
    }

    // Añadir nueva red
    existingNetworks.push_back(network);

    // Guardar todo el array
    File file = SPIFFS.open("/wifi.json", "w");
    if (!file) return false;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& net : existingNetworks) {
        JsonObject obj = arr.add<JsonObject>();
        obj["ssid"] = net.ssid;
        obj["password"] = net.password;
    }

    serializeJson(doc, file);
    file.close();
    return true;
}

bool WiFiManager::loadSavedNetworks(std::vector<WiFiNetwork>& networks) {
    networks.clear();
    if (!SPIFFS.exists("/wifi.json")) return false;

    File file = SPIFFS.open("/wifi.json", "r");
    if (!file || file.size() == 0) return false;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    
    if (error || !doc.is<JsonArray>()) {
        file.close();
        return false;
    }

    for (JsonObject obj : doc.as<JsonArray>()) {
        WiFiNetwork net;
        strlcpy(net.ssid, obj["ssid"] | "", sizeof(net.ssid));
        strlcpy(net.password, obj["password"] | "", sizeof(net.password));
        networks.push_back(net);
    }

    file.close();
    return true;
}

void printWiFiNetwork() {
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

std::vector<WiFiNetwork> getAvailableNetworks() {
    std::vector<WiFiNetwork> networks;
    int numNetworks = WiFi.scanNetworks();
    for (int i = 0; i < numNetworks; i++) {
        WiFiNetwork net;
        strlcpy(net.ssid, WiFi.SSID(i).c_str(), sizeof(net.ssid));
        net.password[0] = '\0'; // Empty password
        net.rssi = WiFi.RSSI(i);
        networks.push_back(net);
    }
    return networks;
}

// 🔹 Conectarse a la mejor red disponible
bool connectToBestWiFi() {
    std::vector<WiFiNetwork> savedNetworks;
    WiFiManager::loadSavedNetworks(savedNetworks);

    // Si no hay redes guardadas, no intentar conexión
    if (savedNetworks.empty()) {
        Serial.println("⚠️ No hay redes WiFi guardadas");
        return false; // AP ya está activo desde el setup()
    }

    // Escanear redes disponibles
    std::vector<WiFiNetwork> availableNetworks;
    WiFiManager::scanNetworks(availableNetworks);

    // Buscar la mejor red guardada disponible
    WiFiNetwork bestNetwork;
    int bestRssi = -1000;
    bool found = false;

    for (const auto& saved : savedNetworks) {
        for (const auto& available : availableNetworks) {
            if (strcmp(saved.ssid, available.ssid) == 0 && available.rssi > bestRssi) {
                bestRssi = available.rssi;
                bestNetwork = saved;
                found = true;
            }
        }
    }

    if (found) {
        WiFi.begin(bestNetwork.ssid, bestNetwork.password);
        if (WiFi.waitForConnectResult(10000) == WL_CONNECTED) {
            return true;
        }
    }

    // Intentar conexión
    Serial.printf("\n📡 Intentando conectar a: %s\n", bestNetwork.ssid);
    WiFi.begin(bestNetwork.ssid, bestNetwork.password);

    // Timeout de conexión mejorado
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime < 20000)) {
        delay(500);
        Serial.print(".");
        yield();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n✅ Conectado exitosamente!\nIP: %s\n", WiFi.localIP().toString().c_str());
        WiFi.mode(WIFI_STA);
        return true;
    }

    Serial.println("\n❌ Falló la conexión");
    return false;
}

void startAPMode() {
    WiFi.disconnect(true);
    delay(100);

    // Intenta conectar a la mejor red guardada
    if (connectToBestWiFi()) {
        Serial.println("✅ Conectado a WiFi. No es necesario activar AP.");
        return;  // Sale de la función si la conexión es exitosa
    }

    // Si no se conecta, activa el modo AP
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(webUsername, webPassword);
    APmode = true;

    Serial.println("⚠️ No se pudo conectar a WiFi. Modo AP activo en: " + WiFi.softAPIP().toString());
}

bool reconnectWiFi() {
    if (APmode) return false;

    Serial.println("Intentando reconexión WiFi...");
    WiFi.disconnect();
    delay(100);
    
    unsigned long start = millis();
    while (millis() - start < 30000) { // 30 segundos máximo
        if (connectToBestWiFi()) return true;
        delay(5000);
    }
    return false;
}

void checkWiFiConnection() {
    if (APmode) return;  // Si el AP está activo, no forzar reconexión

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ WiFi desconectado. Esperando un poco antes de reconectar...");

        delay(5000);  // Esperar 5 segundos antes de intentar reconectar

        if (WiFi.status() != WL_CONNECTED) {  // Si sigue desconectado, reconectar
            Serial.println("🔄 Intentando reconectar a WiFi...");
            reconnectWiFi();
        } else {
            Serial.println("✅ Falsa alarma, WiFi sigue conectado.");
        }
    }
}
