#include "GoogleSheetManager.h"
#include "config.h"
#include <HTTPClient.h>
#include <bsec.h>
#include "BME_Sensor.h"
#include "led.h"
#include "SensorManager.h"
#include "TimeManager.h"
#include "notifications.h"
#include "WiFiManager.h"
#include <Preferences.h>

String extractRedirectUrl(const String &htmlResponse);
struct tm timeinfo;
String formattedTime;
const int maxStoredReadings = 72;
Preferences preferences;
String GoogleSheetManager::url = "";

String safeString(float value) {
    return isnan(value) ? "0.00" : String(value, 2);
}

void googlesheet(void) {
    if (WiFi.status() == WL_CONNECTED) {
        // Verifica si hay URLs guardadas en la memoria
        preferences.begin("sensorData", false); // Abre el espacio de almacenamiento
        bool hasStoredUrls = false;
    
        // Itera sobre las claves en preferences
        int index = 0;
        while (index < maxStoredReadings) { // Límite máximo de URLs
            String key = "url" + String(index);
            String storedUrl = preferences.getString(key.c_str(), ""); // Usa un valor predeterminado vacío
    
            // Si encuentra una URL guardada, imprímela en el serial
        if (storedUrl.length() > 0) {
            sendTelegramMessage("ℹ️ Se encontraron URLs guardadas en la memoria.", config);
            Serial.print("🔗 URL almacenada [");
            Serial.print(key);
            Serial.print("]: ");
            Serial.println(storedUrl);
            hasStoredUrls = true;
        } else {
            break; // Si no hay más URLs, termina el bucle
        }
        
            index++;
        }
        preferences.end(); // Cierra el espacio de almacenamiento
    
        // Si hay URLs guardadas, envíalas primero
        if (hasStoredUrls) {
            Serial.println("📦 Se encontraron URLs guardadas en la memoria. Enviándolas primero...");
            sendAllReadingsToGoogleSheet();
        }
    
        // Ahora envía los datos actuales
        HTTPClient http;

    if (!config.googleSheetURL.startsWith("http://") && !config.googleSheetURL.startsWith("https://")) {
      config.googleSheetURL = "https://" + config.googleSheetURL;
    }

    Serial.print("🔍 googleSheetURL actual: ");
    Serial.println(config.googleSheetURL);
    Serial.println();

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
    Serial.println();

    http.begin(GoogleSheetManager::url);
    http.setTimeout(10000); // Aumenta el tiempo de espera

    // Bloque de reintentos
    int retryCount = 3; // Número de reintentos
    bool dataSent = false; // Bandera para evitar doble envío
    int httpResponseCode = -1; // Inicializa con un valor de error

    while (retryCount > 0 && !dataSent) {
        httpResponseCode = http.GET();
        if (httpResponseCode == HTTP_CODE_OK) { // Código 200: Éxito
            dataSent = true; // Marca los datos como enviados
            break; // Éxito, sal del bucle
        } else if (httpResponseCode == 302) { // Código 302: Redirección
            Serial.println("Redirección detectada");
            Serial.println();
            String newUrl = http.getLocation();
            Serial.print("Nueva URL: ");
            Serial.println(newUrl);
            Serial.println();

                    // Si no se encuentra la cabecera Location, intenta extraer la URL de la respuesta HTML
            if (newUrl.length() == 0) {
             String htmlResponse = http.getString(); // Obtén la respuesta HTML
                newUrl = extractRedirectUrl(htmlResponse); // Extrae la URL de redirección del HTML
                Serial.print("URL de redirección extraída del HTML: ");
                Serial.println(newUrl);
                Serial.println();
            }

            if (newUrl.length() > 0 && (newUrl.startsWith("http://") || newUrl.startsWith("https://"))) {
                http.end(); // Cierra la conexión anterior
                http.begin(newUrl); // Abre una nueva conexión con la URL redirigida

                // Realiza la solicitud a la nueva URL
                httpResponseCode = http.GET();
                if (httpResponseCode == HTTP_CODE_OK) {
                    dataSent = true; // Marca los datos como enviados
                    break; // Éxito, sal del bucle
                }
            } else {
                Serial.println("⚠️ URL de redirección inválida.");
                Serial.println();
            }
        } else {
            Serial.println("Error en la solicitud HTTP. Código: " + String(httpResponseCode));
            Serial.println();
        }

        retryCount--;
        Serial.println("Reintentando... Intentos restantes: " + String(retryCount));
        Serial.println();
        delay(1000); // Espera 1 segundo antes de reintentar
    }

    if (dataSent) {
        Serial.print("Datos enviados al Google Sheet. Código de respuesta HTTP: ");
        Serial.println(httpResponseCode);
        Serial.println();
        String response = http.getString();
        Serial.println("Respuesta del servidor:");
        Serial.println(response);
        Serial.println();
        ledSuccess();
    } else {
        Serial.print("Error al enviar datos. Código de respuesta HTTP: ");
        Serial.println(httpResponseCode);
        Serial.println();
        errLeds();
        saveAndSendData(); // Guarda los datos para enviarlos más tarde
    }

    http.end();
  } else {
    Serial.println("Error: No WiFi connection");
    Serial.println();
    saveAndSendData();
  }
}

// Función para guardar y enviar datos


void saveAndSendData() {
  // Lee los datos del sensor antes de guardarlos
  readSensorData();

  // Obtiene la fecha y hora actual
  String fechaHora = getFormattedDateTime(); // Función que obtiene la fecha y hora actual

  // Construye la URL con los valores actuales del sensor y la fecha/hora
  String url = config.googleSheetURL;
  url += "?location=" + config.location;  
  url += "&fechaHora=" + fechaHora; // Agrega la fecha y hora a la URL
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

  // Verifica si la URL se construyó correctamente
  if (url.length() > 0) {
    // Guarda la URL en preferences (memoria no volátil)
    preferences.begin("sensorData", false); // Abre el espacio de almacenamiento

    // Busca la siguiente clave disponible (por ejemplo, url0, url1, etc.)
    int index = 0;
    while (preferences.getString(("url" + String(index)).c_str(), "").length() > 0) {
      index++;
    }

    // Si se alcanza el límite, elimina la lectura más antigua
    if (index >= maxStoredReadings) {
      preferences.remove("url0"); // Elimina la lectura más antigua
      // Desplaza las claves restantes
      for (int i = 1; i < maxStoredReadings; i++) {
        String key = "url" + String(i);
        String value = preferences.getString(key.c_str(), "");
        preferences.putString(("url" + String(i - 1)).c_str(), value);
      }
      index = maxStoredReadings - 1;
    }

    // Guarda la nueva lectura
    String key = "url" + String(index);
    preferences.putString(key.c_str(), url.c_str()); // Guarda la URL construida localmente
    preferences.end(); // Cierra el espacio de almacenamiento

    Serial.println("Datos guardados en memoria no volátil: " + url);
    Serial.println();
  } else {
    Serial.println("Error: No se pudo construir la URL.");
    Serial.println();
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

      // Verifica que la URL esté bien formada
      if (storedUrl.startsWith("http://") || storedUrl.startsWith("https://")) {
        Serial.print("Enviando URL almacenada: ");
        Serial.println(storedUrl);
        Serial.println();

      // Envía la URL a Google Sheets
      GoogleSheetManager::url = storedUrl;
      sendReadingToGoogleSheet();

      // Borra la URL enviada
      preferences.remove(key.c_str());
    } else {
        Serial.println("⚠️ URL almacenada inválida: " + storedUrl);
        Serial.println();
        preferences.remove(key.c_str()); // Elimina la URL inválida
    }

    index++;
}

preferences.end(); // Cierra el espacio de almacenamiento
Serial.println("Todas las lecturas almacenadas en memoria no volátil enviadas y borradas.");
Serial.println();
}



void sendReadingToGoogleSheet() {
    if (GoogleSheetManager::url.length() > 0) {
        HTTPClient http;
        http.setTimeout(20000); // Aumentamos el tiempo de espera a 20s

        int retryCount = 3;
        bool dataSent = false;
        int httpResponseCode = -1;
        String currentUrl = GoogleSheetManager::url;

        while (retryCount > 0 && !dataSent) {
            Serial.println("📡 Intentando enviar datos a: " + currentUrl);
            Serial.println();
            http.begin(currentUrl);
            http.addHeader("User-Agent", "ESP32");
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            
            httpResponseCode = http.GET();

            if (httpResponseCode == HTTP_CODE_OK) {
                dataSent = true;
                break;
            } else if (httpResponseCode == HTTP_CODE_FOUND) {
                Serial.println("🔄 Redirección detectada");
                Serial.println();
                String newUrl = http.getLocation();
                
                // Si no se encuentra la cabecera Location, intenta extraer del HTML
                if (newUrl.length() == 0) {
                    Serial.println("⚠️ No se encontró la cabecera Location, extrayendo desde el HTML...");
                    Serial.println();
                    String htmlResponse = http.getString();
                    newUrl = extractRedirectUrl(htmlResponse);
                }
                
                if (newUrl.length() > 0 && (newUrl.startsWith("http://") || newUrl.startsWith("https://"))) {
                    Serial.println("🌍 Nueva URL redirigida: " + newUrl);
                    Serial.println();
                    currentUrl = newUrl;
                    http.end(); // Finaliza la conexión actual antes de hacer una nueva petición
                    Serial.println("⌛ Esperando 3s antes de reintentar la redirección...");
                    delay(3000); // Espera antes de la nueva solicitud
                    continue; // Reintentar con la nueva URL
                } else {
                    Serial.println("⚠️ Error: URL de redirección inválida.");
                    Serial.println();
                }
            } else {
                Serial.println("❌ Error HTTP: " + String(httpResponseCode));
                Serial.println("📡 URL: " + currentUrl);
                Serial.println();
                
                if (httpResponseCode == 400) {
                    Serial.println("⚠️ Datos incorrectos en la URL. Verifica los valores enviados.");
                    Serial.println();
                } else if (httpResponseCode == -2) {
                    Serial.println("⚠️ Error de conexión SSL. Intentando reconectar WiFi...");
                    Serial.println();
                    reconnectWiFi();
                }
            }
            retryCount--;
            Serial.println("🔄 Reintentando... Intentos restantes: " + String(retryCount));
            Serial.println();
            delay(3000);
        }

        if (dataSent) {
            Serial.println("✅ Datos enviados a Google Sheets");
            Serial.println();
            String response = http.getString();
            Serial.println("📄 Respuesta del servidor:");
            Serial.println(response);
            Serial.println();
            ledSuccess();
        } else {
            Serial.print("❌ Fallo al enviar datos. Código HTTP: ");
            Serial.println(httpResponseCode);
            Serial.println();
            errLeds();
        }

        http.end();
    } else {
        Serial.println("⚠️ No hay datos para enviar.");
        Serial.println();
    }
}







String extractRedirectUrl(const String &htmlResponse) {
    // Busca la etiqueta <A HREF="..."> en la respuesta HTML
    int hrefStart = htmlResponse.indexOf("HREF=\"");
    if (hrefStart == -1) {
        return ""; // No se encontró la URL
    }

    hrefStart += 6; // Avanza más allá de "HREF=\""
    int hrefEnd = htmlResponse.indexOf("\"", hrefStart);
    if (hrefEnd == -1) {
        return ""; // No se encontró el final de la URL
    }

    // Extrae la URL
    return htmlResponse.substring(hrefStart, hrefEnd);
}