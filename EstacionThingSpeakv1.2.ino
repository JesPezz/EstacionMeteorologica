#include "bsec.h"
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "time.h"
#include <EEPROM.h>
#include <vector> // Agrega la biblioteca vector para almacenar lecturas
#include <ThingSpeak.h> // Agrega la biblioteca ThingSpeak

// Definir las credenciales de ThingSpeak
char ssid[] = "ssid";
char password[] = "pass";
const char* serverName = "url";
unsigned long channelID = id";
const char *writeAPIKey = "key";

const unsigned long CHANNEL_UPDATE_INTERVAL = 60 * 1000; // Intervalo de actualización del canal en milisegundos (en este caso, cada 60 segundos)
unsigned long lastChannelUpdate = 0;

// Define la duración de un mes en segundos (aproximadamente)
const unsigned long MONTH_IN_SECONDS = 30 * 24 * 60 * 60;

// Variable para rastrear el tiempo de la última sincronización
unsigned long lastSyncTime = 0;

#define STATE_SAVE_PERIOD	UINT32_C(360 * 60 * 1000) // 360 minutes - 4 times a day

// Helper functions declarations
void checkIaqSensorStatus(void);
void errLeds(void);
void loadState(void);
void updateState(void);
void googlesheet(void);
bool reconnectWiFi(void);
void saveAndSendData();
void sendReadingToGoogleSheet(const String &reading);
void sendAllReadingsToGoogleSheet();
void connectToWiFi();
void checkWiFiConnection();

// Declaración de la variable storedReadings
std::vector<String> storedReadings;

// Create an object of the class Bsec
Bsec iaqSensor;
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;
String output;
Preferences preferences;

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
bool prevMinuteZero = false; // Variable para rastrear el estado anterior de los minutos


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

void connectToWiFi() {
  // Inicia la conexión WiFi
  WiFi.begin(ssid, password);
  
  // Espera hasta que esté conectado
  Serial.print("Conectando a WiFi ");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) { 
    delay(1000);
    Serial.print(".");
    retries++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conectado a WiFi.");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Error: No se pudo conectar a WiFi después de varios intentos.");
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Intentando reconectar...");
    connectToWiFi(); // Llama a la función para intentar reconectar
  }
}


void setup() {
  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1);
  /* Initializes the Serial communication */
  Serial.begin(115200);

  // Intentar conectarse al WiFi en la configuración inicial
  connectToWiFi();

  delay(1000);
  // Define el nombre de tu código como una cadena de texto
  String nombreCodigo = "EstacionThingSpeakV1.1";
  String ubicacion = "Exterior";
  Serial.println("Nombre del código: " + nombreCodigo);
  Serial.println("Ubicacion: " + ubicacion);
  pinMode(LED_BUILTIN, OUTPUT);
  iaqSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  Serial.println(output);
  checkIaqSensorStatus();

  loadState();
  Serial.println("loadState() se ha cargado.");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a ");
   Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");

  // Connect to WiFi and handle reconnection
  if (reconnectWiFi()) {
    output = "Connected to WiFi: " + WiFi.SSID();
    Serial.println(output);
  }

  // Synchronize the ESP32's internal clock once a month
  syncClock();

  bsec_virtual_sensor_t sensorList[13] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_GAS_PERCENTAGE
  };

  iaqSensor.updateSubscription(sensorList, 13, BSEC_SAMPLE_RATE_LP);
  checkIaqSensorStatus();

  // Print the header
  output = "Timestamp [ms], IAQ, IAQ accuracy, Static IAQ, CO2 equivalent, breath VOC equivalent, raw temp[°C], pressure [hPa], raw relative humidity [%], gas [Ohm], Stab Status, run in status, comp temp[°C], comp humidity [%], gas percentage";
  Serial.println(output);

  // Iniciar tarea FreeRTOS para enviar datos a ThingSpeak
  xTaskCreatePinnedToCore(
    taskSendToThingSpeak,   // Función que ejecutará la tarea
    "SendToThingSpeak",     // Nombre de la tarea
    4096,                   // Tamaño de la pila de la tarea
    NULL,                   // Parámetros de la tarea
    1,                      // Prioridad de la tarea
    NULL,                   // Manejador de la tarea (no utilizado)
    0                       // Núcleo en el que se ejecutará la tarea (núcleo 1)
  );

}

void loop() {

  // Verificar la conexión WiFi constantemente
  checkWiFiConnection();

  // Obtén el tiempo actual en segundos desde el arranque del ESP32
  unsigned long currentTime = millis() / 1000;

  // Verifica si ha pasado un mes desde la última sincronización
  if (currentTime - lastSyncTime >= MONTH_IN_SECONDS) {
    // Llama a syncClock() para sincronizar el reloj nuevamente
    syncClock();
    
    // Actualiza el tiempo de la última sincronización
    lastSyncTime = currentTime;
  }
  unsigned long time_trigger = millis();
  if (iaqSensor.run()) { // If new data is available
    // digitalWrite(LED_BUILTIN, LOW);
    output = String(time_trigger);
    output += ", " + String(iaqSensor.iaq);
    output += ", " + String(iaqSensor.iaqAccuracy);
    output += ", " + String(iaqSensor.staticIaq);
    output += ", " + String(iaqSensor.co2Equivalent);
    output += ", " + String(iaqSensor.breathVocEquivalent);
    output += ", " + String(iaqSensor.rawTemperature);
    output += ", " + String(iaqSensor.pressure);
    output += ", " + String(iaqSensor.rawHumidity);
    output += ", " + String(iaqSensor.gasResistance);
    output += ", " + String(iaqSensor.stabStatus);
    output += ", " + String(iaqSensor.runInStatus);
    output += ", " + String(iaqSensor.temperature);
    output += ", " + String(iaqSensor.humidity);
    output += ", " + String(iaqSensor.gasPercentage);
    Serial.println(output);
    // digitalWrite(LED_BUILTIN, HIGH);

  // Verifica si los minutos han cambiado desde el ciclo anterior
    bool currentMinuteZero = isHourOnTheDot();
    
    // Si los minutos han cambiado de cero a uno, ejecuta googlesheet()
    if (!prevMinuteZero && currentMinuteZero) {
      googlesheet();
    }

    // Actualiza el estado de los minutos previos
    prevMinuteZero = currentMinuteZero;

    updateState();
  } else {
    checkIaqSensorStatus();
  }
 // Verificar y manejar la reconexión WiFi si es necesario
  if (WiFi.status() != WL_CONNECTED) {
    if (reconnectWiFi()) {
      output = "Connected to WiFi: " + WiFi.SSID();
      Serial.println(output);
    }
      saveAndSendData();
  }
}
// Helper function definitions

bool reconnectWiFi(void)
{
  // Intenta reconectar WiFi si no está conectado
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Conexión WiFi perdida. Intentando reconectar...");
    
    // Intenta reconectar al WiFi
    WiFi.begin(ssid, password);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 10) {
      delay(500);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi reconectado.");
      return true; // Reconexión exitosa
    } else {
      Serial.println("\nFallo la reconexión WiFi.");
      return false; // Reconexión fallida
    }
  }
  
  return true; // WiFi ya estaba conectado
}

void checkIaqSensorStatus(void)
{
  if (iaqSensor.bsecStatus != BSEC_OK) {
    if (iaqSensor.bsecStatus < BSEC_OK) {
      output = "BSEC error code : " + String(iaqSensor.bsecStatus);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BSEC warning code : " + String(iaqSensor.bsecStatus);
      Serial.println(output);
    }
  }

  if (iaqSensor.bme68xStatus != BME68X_OK) {
    if (iaqSensor.bme68xStatus < BME68X_OK) {
      output = "BME68X error code : " + String(iaqSensor.bme68xStatus);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BME68X warning code : " + String(iaqSensor.bme68xStatus);
      Serial.println(output);
    }
  }
}

void errLeds(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}

void loadState(void)
{
  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
    // Existing state in EEPROM
    Serial.println("Reading state from EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
      bsecState[i] = EEPROM.read(i + 1);
      Serial.print(bsecState[i], HEX);
    }

    iaqSensor.setState(bsecState);
    checkIaqSensorStatus();
  } else {
    // Erase the EEPROM with zeroes
    Serial.println("Erasing EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
      EEPROM.write(i, 0);

    EEPROM.commit();
  }
}

void updateState(void)
{
  bool update = false;
  if (stateUpdateCounter == 0) {
    /* First state update when IAQ accuracy is >= 3 */
    if (iaqSensor.iaqAccuracy >= 3 || iaqSensor.iaqAccuracy == 1 || iaqSensor.iaqAccuracy == 2) {
      update = true;
      stateUpdateCounter++;
    }
  } else {
    /* Update every STATE_SAVE_PERIOD minutes */
    if ((stateUpdateCounter * STATE_SAVE_PERIOD) < millis()) {
      update = true;
      stateUpdateCounter++;
    }
  }

  if (update) {
    iaqSensor.getState(bsecState);
    checkIaqSensorStatus();

    Serial.println("Writing state to EEPROM");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE ; i++) {
      EEPROM.write(i + 1, bsecState[i]);
      Serial.print(bsecState[i], HEX);
    }

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    EEPROM.commit();
  }
}

void googlesheet(void)
{
  
  // Check if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    // Send sensor data to Google Sheets
    HTTPClient http;
    String url = serverName;
    url += "?iaq=" + String(iaqSensor.iaq);
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
    
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Data sent to Google Sheets");
      preferences.clear(); // Clear stored data in case of successful connection
      digitalWrite(LED_BUILTIN, HIGH); // Enciende el LED si la conexión fue exitosa
      delay(1000);
      digitalWrite(LED_BUILTIN, LOW);
    } 
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
    String url = serverName; // `serverName` contiene la URL base de tu hoja de Google Sheets
    url += "?iaq=" + reading; // Agregar la cadena `reading` como un parámetro en la URL

    // Imprime `reading` en el puerto serial
    Serial.println(reading);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Data sent to Google Sheets");
      preferences.clear(); // Clear stored data in case of successful connection
      digitalWrite(LED_BUILTIN, HIGH); // Enciende el LED si la conexión fue exitosa
      delay(1000);
      digitalWrite(LED_BUILTIN, LOW);
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