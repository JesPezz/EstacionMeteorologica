#include "WiFiManager.h"
#include "config.h"
#include <WiFi.h>
#include "notifications.h"
#include <Preferences.h>
#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>

bool APmode = false;
unsigned long lastConnectionAttempt = 0;
const int maxConnectionAttempts = 3;  // 5 intentos (antes: 1)
const int connectionAttemptDelay = 10000; // 10 segundos entre intentos
const int apModeTimeout = 180000; // 3 minutos (para salir del modo AP)


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
    Serial.println("🔍 Escaneando redes WiFi (async seguro)...");
    networks.clear();

    // Proteger contra escaneos durante OTA o si el stack WiFi está inestable
    if (otaInProgress) {
        Serial.println("⚠️ OTA en progreso, escaneo cancelado.");
        return;
    }

    // Iniciar escaneo asíncrono (no bloqueante)
    int scanStarted = WiFi.scanNetworks(true, false);
    if (scanStarted < 0) {
        // Si no se pudo iniciar el escaneo, intentar modo síncrono como fallback
        Serial.println("⚠️ No se pudo iniciar escaneo asíncrono, intentando escaneo síncrono...");
        int numNetworks = WiFi.scanNetworks();
        if (numNetworks <= 0) {
            Serial.println("❌ Error al escanear redes WiFi (síncrono)");
            writeLog("❌ Error al escanear redes WiFi (síncrono)");
            return;
        }
        for (int i = 0; i < numNetworks; ++i) {
            WiFiNetwork network;
            strlcpy(network.ssid, WiFi.SSID(i).c_str(), sizeof(network.ssid));
            network.rssi = WiFi.RSSI(i);
            network.encryptionType = WiFi.encryptionType(i);
            networks.push_back(network);
            Serial.printf("  %d: %s (%d dBm)\n", i + 1, network.ssid, network.rssi);
        }
        WiFi.scanDelete();
        return;
    }

    // Esperar hasta que el escaneo async finalice (timeout para evitar bloqueo)
    unsigned long start = millis();
    int numNetworks = -2;
    while (millis() - start < 10000) { // 10s timeout
        numNetworks = WiFi.scanComplete();
        if (numNetworks != -2) break; // -2 => still in progress
        delay(200);
    }

    if (numNetworks <= 0) {
        Serial.println("❌ No se encontraron redes o error en escaneo asíncrono.");
        writeLog("❌ No se encontraron redes o error en escaneo asíncrono.");
        WiFi.scanDelete();
        return;
    }

    Serial.println("✅ Redes encontradas:");
    for (int i = 0; i < numNetworks; ++i) {
        WiFiNetwork network;
        strlcpy(network.ssid, WiFi.SSID(i).c_str(), sizeof(network.ssid));
        network.rssi = WiFi.RSSI(i);
        network.encryptionType = WiFi.encryptionType(i);
        networks.push_back(network);
        Serial.printf("  %d: %s (%d dBm)\n", i + 1, network.ssid, network.rssi);
    }

    WiFi.scanDelete(); // Liberar memoria del escaneo
}

bool WiFiManager::saveNetwork(const WiFiNetwork& network) {

    // Use config.savedNetworks as source of truth
    // Load current config (best-effort)
    if (!loadConfig()) {
        Serial.println("⚠️ No se pudo cargar config.json antes de guardar la red");
    }

    // Update existing entry or add
    bool found = false;
    for (auto& net : config.savedNetworks) {
        if (strcmp(net.ssid, network.ssid) == 0) {
            strlcpy(net.password, network.password, sizeof(net.password));
            found = true;
            Serial.printf("✅ Red '%s' actualizada en config.json\n", network.ssid);
            break;
        }
    }

    if (!found) {
        config.savedNetworks.push_back(network);
        Serial.printf("✅ Red '%s' añadida en config.json\n", network.ssid);
    }

    if (!saveConfig(config)) {
        Serial.println("❌ Error al guardar config.json con la nueva red");
        writeLog("❌ Error al guardar config.json con la nueva red");
        return false;
    }

    return true;
}


bool WiFiManager::loadSavedNetworks(std::vector<WiFiNetwork>& outNetworks) {
    outNetworks.clear();
    // Ensure config is loaded
    if (!loadConfig()) {
        Serial.println("⚠️ No se pudo cargar config.json para leer redes guardadas");
    }

    for (const auto& wn : config.savedNetworks) {
        outNetworks.push_back(wn);
    }
    return true;
}

void printWiFiNetwork() {
    if (!loadConfig()) {
        Serial.println("⚠️ No se pudo cargar config.json para imprimir redes");
        return;
    }
    Serial.println("📜 Configuración de redes WiFi (config.json):");
    for (const auto& wn : config.savedNetworks) {
        Serial.printf(" - SSID: %s\n", wn.ssid);
    }
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

bool connectToBestWiFi() {
    std::vector<WiFiNetwork> savedNetworks;
    WiFiManager::loadSavedNetworks(savedNetworks);

    if (savedNetworks.empty()) {
        Serial.println("⚠️ No hay redes guardadas para conectar.");
        return false;
    }

    Serial.println("🔄 Iniciando secuencia de conexión (Modo Directo - Sin Escaneo)...");

    // Iteramos sobre las redes guardadas e intentamos conectar una por una
    for (const auto& network : savedNetworks) {
        Serial.printf("🔗 Intentando conectar a: %s\n", network.ssid);
        
        // ⚡ Desconexión preventiva para limpiar el estado del radio
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(network.ssid, network.password);

        // Esperamos hasta 10 segundos por red
        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
            delay(500);
            Serial.print(".");
            yield(); // Alimentar al perro guardián
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n✅ ¡Conexión Exitosa!");
            Serial.print("📡 IP: ");
            Serial.println(WiFi.localIP());
            return true; // ¡Éxito! Salimos de la función
        } else {
            Serial.println("\n❌ No se pudo conectar. Probando siguiente (si hay)...");
        }
    }

    Serial.println("⚠️ Fallaron todos los intentos de conexión.");
    return false;
}


// 🔹 Función mejorada para seleccionar y conectar a la mejor red
/* bool connectToBestWiFi() {
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
} */


// 🔹 Función mejorada para iniciar el modo AP
void startAPMode() {
    WiFi.disconnect(true);
    delay(100);

    // 1. Verificar si hay redes guardadas
    std::vector<WiFiNetwork> savedNetworks;
    WiFiManager::loadSavedNetworks(savedNetworks);

    // 2. Si NO hay redes guardadas, activar AP indefinidamente SIN reintentos
    if (savedNetworks.empty()) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(webUsername, webPassword);
        APmode = true;
        Serial.println("⚠️ No hay redes WiFi guardadas. Modo AP activado indefinidamente.");
        return; // 🔹 ¡Salir inmediatamente!
    }

    // 3. Si hay redes guardadas, usar lógica normal con timeout
    if (connectToBestWiFi()) {
        APmode = false;
        return;
    }

    // 4. Activar AP temporalmente solo si hay redes guardadas
    WiFi.mode(WIFI_AP);
    WiFi.softAP(webUsername, webPassword);
    APmode = true;
    lastConnectionAttempt = millis();
}


// 🔹 Función de reconexión (mantener compatibilidad)
bool reconnectWiFi() {
    return connectToBestWiFi();
}


// 🔹 Función mejorada para verificar WiFi y manejar reconexiones
void checkWiFiConnection() {
    // 1. Si no hay redes guardadas, NO hacer nada
    std::vector<WiFiNetwork> savedNetworks;
    WiFiManager::loadSavedNetworks(savedNetworks);
    if (savedNetworks.empty()) return;

    // 2. Lógica normal para reconexión
    if (WiFi.status() != WL_CONNECTED && !APmode) {
        startAPMode();
    }
    else if (APmode && (millis() - lastConnectionAttempt > apModeTimeout)) {
        Serial.println("🔄 Intentando reconexión WiFi...");
        APmode = false;
        WiFi.softAPdisconnect(true);
        startAPMode();
    }
}

