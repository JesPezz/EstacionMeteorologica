#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <bsec.h>

// Declaraciones de funciones
void setupBsecSensor();
void checkClockSync();
void readSensorData();
void sendDataToServices();

#endif