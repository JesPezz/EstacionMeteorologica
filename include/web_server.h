#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

void startWebServer();
bool isAuthenticated(AsyncWebServerRequest *request);
void restartESP32Task(void *parameter);
void handleSensorData(AsyncWebServerRequest *request);
void getSensorData(JsonDocument& doc);

#endif
