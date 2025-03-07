#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <bsec.h>

void setupBsecSensor();
void checkClockSync();
void readSensorData();

#endif