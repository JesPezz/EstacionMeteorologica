#ifndef LED_TASK_H
#define LED_TASK_H
#include <Arduino.h>

// Comandos para el LED
enum LedState { LED_IDLE, LED_SUCCESS, LED_PROGRESS, LED_ERROR };

void setupLedTask();
void signalLed(LedState state); // Función para llamar desde tu código

#endif