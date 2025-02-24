# Estación Meteorológica ThingSpeak (v2.8)

```markdown
Este proyecto es una **estación meteorológica basada en ESP32** que envía datos a **ThingSpeak** y los gestiona mediante **Google Apps Script**. Los datos se integran en una hoja de cálculo de **Google Sheets** para su análisis y visualización en la interfaz web integrada.
```

## 🌟 Novedades en la versión 2.8

```diff
- Mejora en la configuración de OTA: `updateOta` ahora se configura en **horas** en la interfaz web y se almacena correctamente en `config.json`.
- Correcciones en la interfaz web: Se optimizó la carga y envío de configuraciones, eliminando dependencias innecesarias.
- Mejor manejo de configuraciones en el ESP32: Se asegura la conversión entre **milisegundos** y **horas** al manejar `updateOta`.
- Mayor estabilidad en el servidor web ESP32: Correcciones en `/config` y `/getConfig` para mejorar la comunicación con la interfaz web.
```

## ⚙️ Características del Proyecto

```yaml
hardware:
  - ESP32
  - Sensor BME680 (temperatura, humedad, presión, calidad del aire)
software:
  - ThingSpeak (visualización de datos)
  - Google Apps Script (gestión de datos)
  - Interfaz web embebida (configuración)
  - Múltiples ubicaciones soportadas
```

## 📊 Conexión del ESP32 con el BME680

```bash
ESP32 Pin | BME680 Pin
--------- | ----------
3.3V      | VIN
GND       | GND
GPIO 21   | SDA
GPIO 22   | SCL
```

## 🛠 Instalación

### 1. Clonar el repositorio
```bash
git clone https://github.com/JesPezz/EstacionMeteorologica.git
```

### 2. Configuración del Canal en ThingSpeak

```yaml
1. Crea un canal en ThingSpeak.
2. Obtén el **ID del canal** y la **API Key**.
```

### 3. Configuración en el ESP32

```json
{
  "ssid": "TuRedWiFi",
  "password": "TuContraseña",
  "googleSheetURL": "https://script.google.com/...",
  "thingSpeakAPIKey": "TU_API_KEY",
  "channelID": 123456,
  "location": "PlantaAlta",
  "updateOta": 6,
  "telegramToken": "TU_TELEGRAM_TOKEN",
  "chatId": "TU_CHAT_ID"
}
```

## 🔄 Actualización OTA

```diff
- Se puede actualizar el firmware desde la interfaz web.
- Ahora `updateOta` se maneja en **horas**, permitiendo mejor configuración.
- El sistema verifica nuevas versiones automáticamente.
```

## 📈 Configuración de Google Apps Script

```yaml
1. Crea un nuevo proyecto en Google Apps Script.
2. Copia y pega el script para gestionar datos en Google Sheets.
3. Despliega como **Web App** y copia la URL en la configuración del ESP32.
```

## 💡 Contribuir

```bash
git checkout -b feature/nueva-funcionalidad
git commit -m "Añadir nueva funcionalidad"
git push origin feature/nueva-funcionalidad
```

## ✨ Licencia

```plaintext
Este proyecto está bajo la licencia MIT.
```

