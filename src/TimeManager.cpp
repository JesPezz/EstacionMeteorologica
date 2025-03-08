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

// Cambia el nombre de la función
String urlEncodeTimeManager(const String &value) {
  String encoded = "";
  char c;
  for (int i = 0; i < value.length(); i++) {
      c = value.charAt(i);
      if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
          encoded += c;
      } else if (c == ' ') {
          encoded += "%20";
      } else if (c == ':') {
          encoded += "%3A";
      } else {
          encoded += "%" + String(c, HEX);
      }
  }
  return encoded;
}

String getFormattedDateTime() {
  struct tm timeinfo;
  getCurrentTime(&timeinfo); // Obtiene la fecha y hora actual

  // Formatea la fecha y hora como "YYYY-MM-DD HH:MM:SS"
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  String fechaHora = String(buffer);

  // Usa la función renombrada para codificar la fecha y hora
  return urlEncodeTimeManager(fechaHora);
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