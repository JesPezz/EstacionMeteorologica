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
unsigned long lastConnectionAttempt = 0;
const int maxConnectionAttempts = 5;  // 5 intentos (antes: 1)
const int connectionAttemptDelay = 10000; // 10 segundos entre intentos
const int apModeTimeout = 120000; // 2 minutos (para salir del modo AP)

// 🔹 Función mejorada para conectar a WiFi con múltiples intentos
bool connectWithRetries(const char* ssid, const char* password) {
    for (int i = 0; i < maxConnectionAttempts; i++) {
        Serial.printf("📡 Intento %d/%d para conectar a %s...\n", i + 1, maxConnectionAttempts, ssid);
        WiFi.begin(ssid, password);
        
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startTime < connectionAttemptDelay)) {
            delay(500);
            Serial.print(".");
            yield();
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n✅ ¡Conectado a %s!\nIP: %s\n", ssid, WiFi.localIP().toString().c_str());
            return true;
        }
    }
    return false;
}

void WiFiManager::scanNetworks(std::vector<WiFiNetwork>& networks) {
    if(otaInProgress) {
        Serial.println("⚠️ OTA en progreso, no se puede escanear redes.");
        return;
    }
    Serial.println();
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

    // Crear archivo si no existe
    if (!SPIFFS.exists("/wifi.json")) {
        File file = SPIFFS.open("/wifi.json", FILE_WRITE);
        if (!file) {
            Serial.println("❌ Error al crear wifi.json inicial");
            return false;
        }
        file.print("[]"); // Array JSON vacío
        file.close();
        Serial.println("✅ Creado wifi.json inicial");
    }
    
    // 1. Leer redes existentes
    std::vector<WiFiNetwork> existingNetworks;
    if (!loadSavedNetworks(existingNetworks)) {
        Serial.println("❌ Error al cargar redes existentes");
        return false;
    }

    if (!loadSavedNetworks(existingNetworks)) {
        existingNetworks.clear(); // Limpiar si hay error
        Serial.println("⚠️ Usando lista vacía (fallo al cargar)");
    }

    // 2. Buscar y actualizar red existente (si existe)
    bool found = false;
    for (auto& net : existingNetworks) {
        if (strcmp(net.ssid, network.ssid) == 0) {
            strlcpy(net.password, network.password, sizeof(net.password));
            found = true;
            Serial.printf("✅ Red '%s' actualizada\n", network.ssid);
            break;
        }
    }

    // 3. Si no existe, añadirla
    if (!found) {
        existingNetworks.push_back(network);
        Serial.printf("✅ Red '%s' añadida\n", network.ssid);
    }

    // 4. Guardar todo el array actualizado
    File file = SPIFFS.open("/wifi.json", "w");
    if (!file) {
        Serial.println("❌ Error al abrir wifi.json para escritura");
        return false;
    }

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& net : existingNetworks) {
        JsonObject obj = arr.add<JsonObject>();
        obj["ssid"] = net.ssid;
        obj["password"] = net.password;
    }

    if (serializeJson(doc, file) == 0) {
        Serial.println("❌ Error al serializar JSON");
        file.close();
        return false;
    }

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

// 🔹 Función mejorada para seleccionar y conectar a la mejor red
bool connectToBestWiFi() {
    std::vector<WiFiNetwork> savedNetworks;
    WiFiManager::loadSavedNetworks(savedNetworks);

    if (savedNetworks.empty()) {
        Serial.println("⚠️ No hay redes WiFi guardadas");
        return false;
    }

    // 1. Escanear redes disponibles y emparejarlas con las guardadas
    std::vector<WiFiNetwork> availableNetworks;
    WiFiManager::scanNetworks(availableNetworks);

    // 2. Ordenar las redes guardadas por mejor señal (RSSI)
    std::vector<WiFiNetwork> prioritizedNetworks;
    for (const auto& saved : savedNetworks) {
        for (const auto& available : availableNetworks) {
            if (strcmp(saved.ssid, available.ssid) == 0) {
                WiFiNetwork prioritized = saved;
                prioritized.rssi = available.rssi; // Actualizar RSSI del escaneo
                prioritizedNetworks.push_back(prioritized);
                break;
            }
        }
    }

    // 3. Ordenar de mayor a menor RSSI
    std::sort(prioritizedNetworks.begin(), prioritizedNetworks.end(), 
        [](const WiFiNetwork& a, const WiFiNetwork& b) { return a.rssi > b.rssi; });

    // 4. Intentar conexión con cada red guardada (en orden de prioridad)
    for (const auto& net : prioritizedNetworks) {
        Serial.printf("\n🔁 Intentando conectar a %s (%d dBm)...\n", net.ssid, net.rssi);
        if (connectWithRetries(net.ssid, net.password)) {
            return true; // ¡Éxito!
        }
    }

    // 5. Si todas fallan, retornar false (activará modo AP)
    return false;
}


// 🔹 Función mejorada para iniciar el modo AP
void startAPMode() {
    WiFi.disconnect(true);
    delay(100);

    // 1. Verificar si hay redes guardadas
    std::vector<WiFiNetwork> savedNetworks;
    WiFiManager::loadSavedNetworks(savedNetworks);

    // 2. Si NO hay redes guardadas, activar AP indefinidamente
    if (savedNetworks.empty()) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(webUsername, webPassword);
        APmode = true;
        Serial.println("⚠️ No hay redes WiFi guardadas. Modo AP activado indefinidamente.");
        return; // 🔹 ¡Salir aquí para evitar reintentos!
    }

    // 3. Si hay redes guardadas, intentar conexión
    if (connectToBestWiFi()) {
        APmode = false;
        Serial.println("✅ WiFi conectado. Modo AP desactivado.");
        return;
    }

    // 4. Si las redes guardadas fallan, activar AP temporalmente (con timeout)
    WiFi.mode(WIFI_AP);
    WiFi.softAP(webUsername, webPassword);
    APmode = true;
    lastConnectionAttempt = millis();
    Serial.printf("⚠️ Modo AP activado (timeout: %d minutos). IP: %s\n", apModeTimeout / 60000, WiFi.softAPIP().toString().c_str());
}


// 🔹 Función de reconexión (mantener compatibilidad)
bool reconnectWiFi() {
    return connectToBestWiFi();
}


// 🔹 Función mejorada para verificar WiFi y manejar reconexiones
void checkWiFiConnection() {
    if (WiFi.status() == WL_CONNECTED) return;

    if (APmode) {
        // Si está en modo AP y ha pasado el timeout, intentar reconectar
        if (millis() - lastConnectionAttempt > apModeTimeout) {
            Serial.println("🔄 Timeout de modo AP. Intentando reconectar a WiFi...");
            APmode = false;
            WiFi.softAPdisconnect(true);
            startAPMode(); // Reinicia el proceso
        }
    } else {
        // Si no está en modo AP, reintentar conexión periódicamente
        if (millis() - lastConnectionAttempt > 30000) { // Cada 30 segundos
            Serial.println("🔄 Intentando reconexión WiFi...");
            lastConnectionAttempt = millis();
            startAPMode(); // Llama a la función que maneja los reintentos
        }
    }
}

