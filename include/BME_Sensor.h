#ifndef BME_SENSOR_H
#define BME_SENSOR_H

#include <Wire.h>
#include <bsec.h>

extern Bsec iaqSensor; // Declaración externa

void checkIaqSensorStatus();
void loadState();
void updateState();

#endif