#include "GoogleSheetManager.h"
#include "config.h"
#include <HTTPClient.h>
#include <bsec.h>
#include "BME_Sensor.h"
#include "led.h"

std::vector<String> storedReadings;
Preferences preferences;

// Implementa googlesheet, saveAndSendData, etc. (copiar código original)

void googlesheet(void)
{
  // Verificar si el WiFi está conectado
  if (WiFi.status() == WL_CONNECTED) {
    // Crear cliente HTTP
    HTTPClient http;

    if (!config.googleSheetURL.startsWith("http://") && !config.googleSheetURL.startsWith("https://")) {
      config.googleSheetURL = "https://" + config.googleSheetURL;  // Asegurar protocolo correcto
  }
    Serial.print("🔍 googleSheetURL actual: ");
    Serial.println(config.googleSheetURL);
  
    String url = config.googleSheetURL;
    url += "?location=" + config.location;  
    url += "&iaq=" + String(iaqSensor.iaq);
    url += "&iaqAccuracy=" + String(iaqSensor.iaqAccuracy);
    url += "&staticIaq=" + String(iaqSensor.staticIaq);
    url += "&co2Equivalent=" + String(iaqSensor.co2Equivalent);
    url += "&breathVocEquivalent=" + String(iaqSensor.breathVocEquivalent);
    url += "&rawTemperature=" + String(iaqSensor.rawTemperature);
    url += "&pressure=" + String(iaqSensor.pressure / 100);
    url += "&rawHumidity=" + String(iaqSensor.rawHumidity);
    url += "&gasResistance=" + String(iaqSensor.gasResistance);
    url += "&stabStatus=" + String(iaqSensor.stabStatus);
    url += "&runInStatus=" + String(iaqSensor.runInStatus);
    url += "&temperature=" + String(iaqSensor.temperature);
    url += "&humidity=" + String(iaqSensor.humidity);
    url += "&gasPercentage=" + String(iaqSensor.gasPercentage);
    
    Serial.print("🌍 URL final: ");
    Serial.println(url);

    // Realizar la solicitud HTTP
    http.begin(url);
    http.setTimeout(5000);
    int httpResponseCode = http.GET();

    // Si hay una redirección (código 302)
    if (httpResponseCode == 302) {
      Serial.println("Redirección detectada");
      // Obtener la nueva URL desde el encabezado de redirección
      String newUrl = http.getLocation();
      Serial.print("Nueva URL: ");
      Serial.println(newUrl);

      if (newUrl.length() > 0 && (newUrl.startsWith("http://") || newUrl.startsWith("https://"))) {
        http.end();
        http.begin(newUrl);
        httpResponseCode = http.GET();
    } else {
        Serial.println("⚠️ URL de redirección inválida.");
    }


      // Realizar la solicitud a la nueva URL
      http.end();  // Finalizar la conexión anterior
      http.begin(newUrl);  // Comenzar con la nueva URL
      httpResponseCode = http.GET();  // Realizar la nueva solicitud
    }

   if (httpResponseCode > 0) {
      Serial.print("Datos enviados al Google Sheet. Código de respuesta HTTP: ");
      Serial.println(httpResponseCode);
      ;

      // Leer la respuesta del servidor (si la hay)
      String response = http.getString();
      Serial.println("Respuesta del servidor:");
      Serial.println(response);

      // Limpiar datos almacenados si la conexión fue exitosa
      preferences.clear();
      ledSuccess();

    } else {
      Serial.print("Error al enviar datos. Código de respuesta HTTP: ");
      Serial.println(httpResponseCode);
      errLeds();
      saveAndSendData();
    }

    // Terminar la conexión HTTP
    http.end();
  } else {
    Serial.println("Error: No WiFi connection");
    // Llamar a la función para guardar los datos en caso de fallo de conexión WiFi
    saveAndSendData(); 
  }
}

// Función para guardar y enviar datos
void saveAndSendData() {
    // Obtén el tiempo actual en segundos desde el arranque del ESP32
    unsigned long currentTime = millis() / 1000;
    
    // Verifica si ha pasado una hora desde la última sincronización
    if (currentTime - lastSyncTime >= 3600) {
      // Realiza una lectura del sensor BME680 y almacena los datos en un formato de cadena
     
      String reading = String(iaqSensor.iaq);
      reading += "&iaqAccuracy=" + String(iaqSensor.iaqAccuracy);
      reading += "&staticIaq=" + String(iaqSensor.staticIaq);
      reading += "&co2Equivalent=" + String(iaqSensor.co2Equivalent);
      reading += "&breathVocEquivalent=" + String(iaqSensor.breathVocEquivalent);
      reading += "&rawTemperature=" + String(iaqSensor.rawTemperature);
      reading += "&pressure=" + String(iaqSensor.pressure);
      reading += "&rawHumidity=" + String(iaqSensor.rawHumidity);
      reading += "&gasResistance=" + String(iaqSensor.gasResistance);
      reading += "&stabStatus=" + String(iaqSensor.stabStatus);
      reading += "&runInStatus=" + String(iaqSensor.runInStatus);
      reading += "&temperature=" + String(iaqSensor.temperature);
      reading += "&humidity=" + String(iaqSensor.humidity);
      reading += "&gasPercentage=" + String(iaqSensor.gasPercentage);    
  
         storedReadings.push_back(reading);
      // Imprime `reading` en el puerto serial
      Serial.println(reading);
      Serial. println("datos guardados");
      // Actualiza el tiempo de la última sincronización
      lastSyncTime = currentTime;
    }
    
    // Verifica si se ha reconectado el WiFi
    if (WiFi.status() == WL_CONNECTED && !storedReadings.empty()) {
      // Envía las lecturas almacenadas a la hoja de Google Sheets
      for (const String &reading : storedReadings) {
        sendAllReadingsToGoogleSheet();
      }
      
      // Borra todas las lecturas almacenadas después de enviarlas
      storedReadings.clear();
    }
  }

  // Función para enviar una lectura a la hoja de Google Sheets
void sendReadingToGoogleSheet(const String &reading) {
    // Send sensor data to Google Sheets
      HTTPClient http;
      String url = config.googleSheetURL; 
      url += "?iaq=" + reading; // Agregar la cadena `reading` como un parámetro en la URL
  
      // Imprime `reading` en el puerto serial
      Serial.println(reading);
      http.begin(url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        Serial.println("Data sent to Google Sheets");
        preferences.clear(); // Clear stored data in case of successful connection
        ledSuccess();
      } 
    }
  
  
    void sendAllReadingsToGoogleSheet() {
      // Verifica si hay datos para enviar
      if (!storedReadings.empty()) {
    // Envía todas las lecturas almacenadas a la hoja de Google Sheets
    for (const String &reading : storedReadings) {
      sendReadingToGoogleSheet(reading);
    }
    
    // Borra todas las lecturas almacenadas después de enviarlas
    storedReadings.clear();
    Serial.println("Todas las lecturas almacenadas enviadas y borradas");
  }
    }