#ifndef VOLTAGE_MONITOR_H
#define VOLTAGE_MONITOR_H

#include <Arduino.h>

// Inicializa el monitor de voltaje (configura pines si es necesario)
void initVoltageMonitor();

// Debe ser llamado periódicamente desde loop(); realiza la lectura y escribe logs si detecta fallo.
void checkVoltage();

// Getters para exponer estado al frontend
float getMeasuredVoltage();
String getBatteryStatus(); // "absent", "undervoltage", "ok"

#endif // VOLTAGE_MONITOR_H
