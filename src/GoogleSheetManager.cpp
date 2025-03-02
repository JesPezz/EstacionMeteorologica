#include "GoogleSheetManager.h"
#include "config.h"
#include <HTTPClient.h>
#include <bsec.h>
#include "BME_Sensor.h"
#include "led.h"

std::vector<String> storedReadings;
Preferences preferences;
String GoogleSheetManager::url = "";

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
  
    GoogleSheetManager::url = config.googleSheetURL;
    GoogleSheetManager::url += "?location=" + config.location;  
    GoogleSheetManager::url += "&iaq=" + String(iaqSensor.iaq);
    GoogleSheetManager::url += "&iaqAccuracy=" + String(iaqSensor.iaqAccuracy);
    GoogleSheetManager::url += "&staticIaq=" + String(iaqSensor.staticIaq);
    GoogleSheetManager::url += "&co2Equivalent=" + String(iaqSensor.co2Equivalent);
    GoogleSheetManager::url += "&breathVocEquivalent=" + String(iaqSensor.breathVocEquivalent);
    GoogleSheetManager::url += "&rawTemperature=" + String(iaqSensor.rawTemperature);
    GoogleSheetManager::url += "&pressure=" + String(iaqSensor.pressure / 100);
    GoogleSheetManager::url += "&rawHumidity=" + String(iaqSensor.rawHumidity);
    GoogleSheetManager::url += "&gasResistance=" + String(iaqSensor.gasResistance);
    GoogleSheetManager::url += "&stabStatus=" + String(iaqSensor.stabStatus);
    GoogleSheetManager::url += "&runInStatus=" + String(iaqSensor.runInStatus);
    GoogleSheetManager::url += "&temperature=" + String(iaqSensor.temperature);
    GoogleSheetManager::url += "&humidity=" + String(iaqSensor.humidity);
    GoogleSheetManager::url += "&gasPercentage=" + String(iaqSensor.gasPercentage);
    
    Serial.print("🌍 URL final: ");
    Serial.println(GoogleSheetManager::url);

    // Realizar la solicitud HTTP
    http.begin(GoogleSheetManager::url);
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
  // Verifica si la URL ya está construida
  if (GoogleSheetManager::url.length() > 0) {
      // Guarda la URL en preferences (memoria no volátil)
      preferences.begin("sensorData", false); // Abre el espacio de almacenamiento

      // Busca la siguiente clave disponible (por ejemplo, url0, url1, etc.)
      int index = 0;
      while (preferences.getString(("url" + String(index)).c_str(), "").length() > 0) {
          index++;
      }

      // Guarda la URL con la clave única
      String key = "url" + String(index);
      preferences.putString(key.c_str(), GoogleSheetManager::url.c_str());
      preferences.end(); // Cierra el espacio de almacenamiento

      Serial.println("Datos guardados en memoria no volátil: " + GoogleSheetManager::url);
  } else {
      Serial.println("Error: No hay datos para guardar.");
  }

  // Si hay conexión WiFi, intenta enviar los datos almacenados
  if (WiFi.status() == WL_CONNECTED) {
      sendAllReadingsToGoogleSheet();
  }
}

void sendAllReadingsToGoogleSheet() {
  preferences.begin("sensorData", false); // Abre el espacio de almacenamiento

  // Itera sobre las claves en preferences
  int index = 0;
  while (true) {
      String key = "url" + String(index);
      String storedUrl = preferences.getString(key.c_str(), "");

      // Si no hay más URLs, termina el bucle
      if (storedUrl.length() == 0) {
          break;
      }

      // Envía la URL a Google Sheets
      GoogleSheetManager::url = storedUrl;
      sendReadingToGoogleSheet();

      // Borra la URL enviada
      preferences.remove(key.c_str());
      index++;
  }

  preferences.end(); // Cierra el espacio de almacenamiento
  Serial.println("Todas las lecturas almacenadas en memoria no volátil enviadas y borradas.");
}

  // Función para enviar una lectura a la hoja de Google Sheets
  void sendReadingToGoogleSheet() {
    if (GoogleSheetManager::url.length() > 0) {
        HTTPClient http;
        http.begin(GoogleSheetManager::url);
        http.setTimeout(5000);
        int httpCode = http.GET();

        // Si hay una redirección (código 302)
        if (httpCode == 302) {
            String newUrl = http.getLocation();
            if (newUrl.length() > 0 && (newUrl.startsWith("http://") || newUrl.startsWith("https://"))) {
                http.end(); // Cierra la conexión anterior
                http.begin(newUrl); // Abre una nueva conexión con la URL redirigida
                httpCode = http.GET(); // Realiza la solicitud a la nueva URL
            }
        }

        if (httpCode > 0) {
            Serial.println("Datos enviados a Google Sheets");
            ledSuccess();
        } else {
            Serial.print("Error al enviar datos. Código de respuesta HTTP: ");
            Serial.println(httpCode);
            errLeds();
        }

        http.end();
    } else {
        Serial.println("Error: No hay datos para enviar.");
    }
}