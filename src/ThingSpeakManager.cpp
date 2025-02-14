#include "ThingSpeakManager.h"
#include "config.h"
#include <HTTPClient.h>
#include "BME_Sensor.h"

// Implementa taskSendToThingSpeak (copiar código original)



void taskSendToThingSpeak(void *pvParameters) {
    (void) pvParameters; // Se suprime la advertencia de parámetro no utilizado
  
    for (;;) {
      // Verificar si ha pasado el intervalo de actualización del canal
      if (millis() - lastChannelUpdate >= CHANNEL_UPDATE_INTERVAL) {
        // Construir la URL completa con los datos
        String url = "https://api.thingspeak.com/update?api_key=";
        url += writeAPIKey;
        url += "&field1=";
        url += String(iaqSensor.temperature);
        url += "&field2=";
        url += String(iaqSensor.humidity);
        url += "&field3=";
        url += String(iaqSensor.pressure / 100);
        url += "&field4=";
        url += String(iaqSensor.iaq);
  
        // Enviar una solicitud HTTP GET a la URL
        HTTPClient http;
        http.begin(url);
        int httpResponseCode = http.GET();
        
        // Verificar el código de respuesta HTTP
        if (httpResponseCode > 0) {
          Serial.print("Datos enviados a ThingSpeak. Código de respuesta HTTP: ");
          Serial.println(httpResponseCode);
        } else {
          Serial.print("Error al enviar datos a ThingSpeak. Código de respuesta HTTP: ");
          Serial.println(httpResponseCode);
        }
  
        // Cerrar la conexión HTTP
        http.end();
        
        // Actualizar el tiempo de la última actualización del canal
        lastChannelUpdate = millis();
      }
  
      // Esperar un tiempo antes de la próxima actualización del canal
      vTaskDelay(pdMS_TO_TICKS(14000)); // Esperar 14 segundos
    }
  }