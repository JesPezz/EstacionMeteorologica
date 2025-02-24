#ifndef BME_SENSOR_H
#define BME_SENSOR_H

#include <Wire.h>
#include <bsec.h>

extern Bsec iaqSensor;

void checkIaqSensorStatus();
void loadState();
void updateState();

#endif