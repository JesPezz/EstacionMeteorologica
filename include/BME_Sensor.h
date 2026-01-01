#ifndef BME_SENSOR_H
#define BME_SENSOR_H
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <bsec.h>
#include <Preferences.h>
#include <base64.h>

extern Bsec iaqSensor;
extern Preferences bsecPrefs;

void checkIaqSensorStatus();
void loadState();
void updateState();
void printHexDump(const uint8_t* data, size_t size, uint8_t bytesPerLine);

#endif