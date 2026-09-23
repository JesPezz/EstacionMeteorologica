#include "WiFiManager.h"
#include "config.h"
#include <WiFi.h>
#include "notifications.h"
#include <Preferences.h>
#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>
#include "esp_ota_ops.h"
#include <algorithm>

bool APmode = false;
unsigned long lastConnectionAttempt = 0;
const int maxConnectionAttempts = 5;  // 5 intentos
const int connectionAttemptDelay = 15000; // 15 segundos entre intentos
const int apModeTimeout = 180000; // 3 minutos (para salir del modo AP)
const int MIN_RSSI = -70; // dBm mínimo para considerar conexión estable
const int RSSI_MONITOR_INTERVAL_MS = 60000; // Revisar RSSI cada 60s


// 🔹 Función mejorada para conectar a WiFi con múltiples intentos
bool connectWithRetries(const char* ssid, const char* password) {
    WiFi.setSleep(false); // Desactivar modem sleep para evitar desconexiones con repetidores
    for (int i = 0; i < maxConnectionAttempts; i++) {
        Serial.printf("📡 Intento %d/%d para conectar a %s...\n", i + 1, maxConnectionAttempts, ssid);
        WiFi.disconnect(true);
        delay(100);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startTime < connectionAttemptDelay)) {
            delay(500);
            Serial.print(".");
            yield();
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n✅ ¡Conectado a %s!\nIP: %s\nRSSI: %d dBm\n", ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI());
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

    // Use config.savedNetworks as source of truth (cargado UNA VEZ en setup() desde SPIFFS)

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
    // 🔄 Las redes ya están cargadas en RAM desde setup() (config.savedNetworks).
    // NO releer config.json desde SPIFFS durante el loop()/reconexiones.
    for (const auto& wn : config.savedNetworks) {
        outNetworks.push_back(wn);
    }
    return true;
}

void printWiFiNetwork() {
    // 🔄 Las redes ya están en RAM (cargadas en setup()). No releer SPIFFS aquí.
    Serial.println("📜 Configuración de redes WiFi (RAM):");
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

// 🔹 Función mejorada para seleccionar y conectar a la mejor red por RSSI
bool connectToBestWiFi() {
    std::vector<WiFiNetwork> savedNetworks;
    WiFiManager::loadSavedNetworks(savedNetworks);

    if (savedNetworks.empty()) {
        Serial.println("⚠️ No hay redes guardadas para conectar.");
        return false;
    }

    // Escanear redes disponibles para obtener RSSI actual
    std::vector<WiFiNetwork> availableNetworks;
    WiFiManager::scanNetworks(availableNetworks);

    // Emparejar redes guardadas con RSSI actual
    std::vector<WiFiNetwork> prioritizedNetworks;
    for (const auto& saved : savedNetworks) {
        for (const auto& available : availableNetworks) {
            if (strcmp(saved.ssid, available.ssid) == 0) {
                WiFiNetwork prioritized = saved;
                prioritized.rssi = available.rssi;
                prioritizedNetworks.push_back(prioritized);
                break;
            }
        }
        // Si no se encontró en el escaneo, usar la red guardada con RSSI 0
        bool found = false;
        for (const auto& net : availableNetworks) {
            if (strcmp(saved.ssid, net.ssid) == 0) { found = true; break; }
        }
        if (!found) {
            prioritizedNetworks.push_back(saved);
        }
    }

    // Ordenar de mayor a menor RSSI
    std::sort(prioritizedNetworks.begin(), prioritizedNetworks.end(),
        [](const WiFiNetwork& a, const WiFiNetwork& b) { return a.rssi > b.rssi; });

    Serial.println("📶 Redes ordenadas por señal:");
    for (const auto& net : prioritizedNetworks) {
        Serial.printf("  %s: %d dBm\n", net.ssid, net.rssi);
    }

    const int maxGlobalRounds = 3;
    for (int round = 1; round <= maxGlobalRounds; round++) {
        Serial.printf("🔄 Ronda de conexión %d/%d\n", round, maxGlobalRounds);

        for (const auto& network : prioritizedNetworks) {
            // Saltar redes con señal muy débil (menos de -80 dBm)
            if (network.rssi < -85) {
                Serial.printf("⏭️ Saltando %s (RSSI %d dBm demasiado débil)\n", network.ssid, network.rssi);
                continue;
            }

            Serial.printf("🔗 Intentando conectar a: %s (%d dBm)\n", network.ssid, network.rssi);

            if (connectWithRetries(network.ssid, network.password)) {
                // Verificar que la señal sea aceptable
                if (WiFi.RSSI() < MIN_RSSI) {
                    Serial.printf("⚠️ Señal débil (%d dBm < %d dBm). Reconectando...\n", WiFi.RSSI(), MIN_RSSI);
                    WiFi.disconnect(true);
                    continue;
                }
                Serial.println("\n✅ ¡Conexión Exitosa!");
                Serial.print("📡 IP: ");
                Serial.println(WiFi.localIP());
                return true;
            }
        }
    }

    // Último intento: conectar a cualquier red guardada sin filtro RSSI
    Serial.println("⚠️ Último intento sin filtro RSSI...");
    for (const auto& network : savedNetworks) {
        if (connectWithRetries(network.ssid, network.password)) {
            return true;
        }
    }

    Serial.println("❌ No se pudo conectar a ninguna red WiFi. Activando modo AP...");
    writeLog("❌ No se pudo conectar a ninguna red WiFi. Activando modo AP...");
    return false;
}


// 🔹 Función mejorada para iniciar el modo AP
void startAPMode() {
    WiFi.disconnect(true);
    delay(100);
    WiFi.setSleep(false); // Desactivar modem sleep para repetidores

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

    // 2. Monitoreo de RSSI: si WiFi está conectado pero señal débil, reconectar
    if (WiFi.status() == WL_CONNECTED) {
        static unsigned long lastRssiCheck = 0;
        if (millis() - lastRssiCheck >= RSSI_MONITOR_INTERVAL_MS) {
            lastRssiCheck = millis();
            int rssi = WiFi.RSSI();
            if (rssi < MIN_RSSI) {
                Serial.printf("⚠️ Señal WiFi débil: %d dBm < umbral %d dBm. Reconectando...\n", rssi, MIN_RSSI);
                writeLog("⚠️ Señal WiFi débil. Reconectando...");
                WiFi.disconnect(true);
                APmode = false;
                lastConnectionAttempt = millis();
                startAPMode();
                return;
            }
        }
    }

    // 3. Lógica normal para reconexión
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

