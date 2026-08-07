// En SensorManager.h
#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <bsec.h>
#include <ArduinoJson.h>

extern const uint8_t bsec_config_iaq[]; // Asegurar visibilidad de la config

void setupBsecSensor();
void checkClockSync();
bool readSensorData(); // <--- CAMBIAR void POR bool AQUÍ TAMBIÉN
void populateSensorJson(JsonDocument& doc);
#endif