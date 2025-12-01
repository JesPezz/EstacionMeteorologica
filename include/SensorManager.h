#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H
#include <Arduino.h>
#include <bsec.h>

extern const uint8_t bsec_config_iaq[];
void setupBsecSensor();
void checkClockSync();
void readSensorData();

#endif