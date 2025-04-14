#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H
#pragma once
#include <Arduino.h>
#include <vector>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"

class WiFiManager {
public:
    const char* getLastError();
    static void scanNetworks(std::vector<WiFiNetwork>& networks);
    static bool loadSavedNetworks(std::vector<WiFiNetwork>& networks);
    static bool saveNetwork(const WiFiNetwork& network);
};

bool reconnectWiFi();
void checkWiFiConnection();
void scanWiFiNetworks();
std::vector<WiFiNetwork> getAvailableNetworks();
bool connectToBestWiFi();
void startAPMode();
void printWiFiNetwork();

#endif