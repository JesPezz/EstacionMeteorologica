#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

void connectToWiFi();
bool reconnectWiFi();
void checkWiFiConnection();
void startAPMode();

#endif