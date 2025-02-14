#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>


void startWebServer();
bool isAuthenticated(AsyncWebServerRequest *request);

#endif
