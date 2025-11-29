# 📡 Estación Meteorológica ThingSpeak

Este proyecto es una **estación meteorológica basada en ESP32** que utiliza el sensor **BME680** para medir:
- 🌡 **Temperatura**
- 💧 **Humedad**
- 🌬 **Presión**
- 🏭 **Calidad del aire**

Los datos se envían a **ThingSpeak** para su monitoreo en tiempo real y se gestionan mediante **Google Apps Script**, integrándose en **Google Sheets** para análisis y visualización.

---

## 🌟 Novedades en la versión 2.8

✅ **Mejoras en OTA:** `updateOta` ahora se configura en **horas** desde la interfaz web y se almacena correctamente en `config.json`.
✅ **Correcciones en la interfaz web:** Optimización en la carga y envío de configuraciones, eliminando dependencias innecesarias.
✅ **Manejo mejorado de configuraciones en ESP32:** Se asegura la conversión entre **milisegundos** y **horas** al manejar `updateOta`.
✅ **Mayor estabilidad del servidor web ESP32:** Mejoras en `/config` y `/getConfig` para optimizar la comunicación con la interfaz web.

---

## ⚙️ Características del Proyecto

### 🖥 **Hardware**
- ESP32
- Sensor BME680 (temperatura, humedad, presión, calidad del aire)

### 🛠 **Software**
- 📊 **ThingSpeak** (visualización de datos)
- 📜 **Google Apps Script** (gestión de datos)
- 🌍 **Interfaz web embebida** (configuración)
- 📍 **Soporte para múltiples ubicaciones**

---

## 📊 Conexión del ESP32 con el BME680

```bash
ESP32 Pin | BME680 Pin
--------- | ----------
3.3V      | VIN
GND       | GND y SDO
GPIO 21   | SDA
GPIO 22   | SCL
```

---

## 🛠 Instalación

### 1️⃣ Clonar el repositorio
```bash
git clone https://github.com/JesPezz/EstacionMeteorologica.git
```

### 2️⃣ Configurar el Canal en ThingSpeak
1. Crea un canal en ThingSpeak.
2. Obtén el **ID del canal** y la **API Key**.

### 3️⃣ Configurar el ESP32
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

---

## 🔄 Actualización OTA

🚀 **El firmware ahora se puede actualizar desde la interfaz web.**
🔄 `updateOta` se maneja en **horas** para una configuración más flexible.
✅ El sistema verifica automáticamente si hay nuevas versiones.

---

## 📈 Configuración de Google Apps Script

1. Crea un nuevo proyecto en **Google Apps Script**.
2. Copia y pega el script para gestionar datos en **Google Sheets**.
3. Despliega como **Web App** y copia la URL en la configuración del ESP32.

---

## 💡 Contribuir

Si deseas mejorar este proyecto, sigue estos pasos:
```bash
git checkout -b feature/nueva-funcionalidad
git commit -m "Añadir nueva funcionalidad"
git push origin feature/nueva-funcionalidad
```

¡Toda contribución es bienvenida! 🚀

