#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <vector>
#include <ArduinoJson.h>

struct WiFiNetwork {
    String ssid;
    String password;
    int rssi;
};

bool reconnectWiFi();
void checkWiFiConnection();
void scanWiFiNetworks();
std::vector<WiFiNetwork> getAvailableNetworks();
bool saveWiFiCredentialsToFile(const String &ssid, const String &password);
void loadWiFiConfig();
bool connectToBestWiFi();
void startAPMode();
void printWiFiConfig();

#endif