#include "TimeManager.h"
#include "config.h"
#include "TimeManager.h"
#include <time.h>       
#include <Arduino.h>    

// Function to synchronize the ESP32's internal clock with NTP server
void syncClock() {
    configTime(0, 0, "pool.ntp.org");
    while (!time(nullptr)) {
      delay(100);
    }
  }

  // Function to get the current time from the ESP32's internal clock
void getCurrentTime(struct tm* timeinfo) {
    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);
  }

  bool isHourOnTheDot() {
    struct tm timeinfo;
    getCurrentTime(&timeinfo); // Obtiene la hora actual
    return timeinfo.tm_min == 0; // Verifica si los minutos son igual a cero
  }
  
  bool isFiveMinutes() {
    struct tm timeinfo;
    getCurrentTime(&timeinfo); // Obtiene la hora actual
    return timeinfo.tm_min % 5 == 0; // Verifica si los minutos son múltiplos de 5
}

bool isTenMinutes() {
  struct tm timeinfo;
  getCurrentTime(&timeinfo); // Obtiene la hora actual
  return timeinfo.tm_min % 10 == 0; // Verifica si los minutos son múltiplos de 10
}

bool isMinuteOnTheDot() {
  struct tm timeinfo;
  getCurrentTime(&timeinfo); // Obtiene la hora actual
  return true; // Se activa cada minuto
}