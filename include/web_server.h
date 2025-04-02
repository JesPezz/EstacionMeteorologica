#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#pragma once
#include "config.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t wifiMutex;
AsyncWebServerResponse* prepareCORSResponse(AsyncWebServerRequest* request, int code, String contentType);
String getSensorJson();
void startWebServer();
bool isAuthenticated(AsyncWebServerRequest *request);
void restartESP32Task(void *parameter);
void handleSensorData(AsyncWebServerRequest *request);
//void getSensorData(JsonDocument& doc);
void handleWiFiConnect(AsyncWebServerRequest *request);
void sendSSEData(TimerHandle_t xTimer);
void handleSSE(AsyncWebServerRequest *request);
void sendSensorData();
void initSSETimer();
void initSensorMutex();
void listSPIFFS();
//void handleScanRequest(AsyncWebServerRequest *request);
bool isValidURL(String url, const char* domain);
bool parseRequestJSON(AsyncWebServerRequest* request, JsonDocument& doc);
void handleWiFiSave(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleWiFiScanAPI(AsyncWebServerRequest *request);
void wifiScanTask(void *parameter);
void initWiFiScanner();
void logConfig(const char* title, const Config &config);

#endif
