# 📖 Documentación del Proyecto y Manual de Usuario

## 📌 Introducción
Este documento cubre la **documentación técnica** del proyecto **Estación Meteorológica ThingSpeak**, así como el **manual de usuario** para su correcta configuración y uso.

---

# 🛠 Documentación Técnica

## 1️⃣ **Descripción General**
Este sistema permite recopilar datos meteorológicos mediante un ESP32 y el sensor BME680, enviando la información a **ThingSpeak** y almacenándola en **Google Sheets**. Además, cuenta con una **interfaz web** embebida para configuración remota y soporte para **actualización OTA**.

## 2️⃣ **Arquitectura del Proyecto**
```yaml
hardware:
  - ESP32
  - Sensor BME680 (Temperatura, Humedad, Presión, Calidad del Aire)
  - Alimentación 3.3V / 5V
software:
  - Arduino Framework
  - ThingSpeak API
  - Google Apps Script
  - Servidor Web embebido en ESP32
```

## 3️⃣ **Instalación y Configuración**
### 3.1 Clonar el Repositorio
```bash
git clone https://github.com/JesPezz/EstacionMeteorologica.git
```
### 3.2 Configuración del ESP32
- **Cargar firmware con PlatformIO / Arduino IDE**.
- **Editar `config.json` o configurar desde la interfaz web**.
- **Conectar ESP32 a la red WiFi**.

### 3.3 Configuración de ThingSpeak
- Crear canal en [ThingSpeak](https://thingspeak.com/).
- Obtener API Key y Channel ID.
- Configurar en `config.json` o en la web.

### 3.4 Configuración de Google Apps Script
1. Crear un nuevo proyecto en [Google Apps Script](https://script.google.com/).
2. Copiar y pegar el script para gestión de datos en Google Sheets.
3. Implementar como **Web App** y obtener la URL.
4. Configurar la URL en `config.json` o interfaz web.

---

# 📖 Manual de Usuario

## 1️⃣ **Acceso a la Interfaz Web**
- Conectar el ESP32 a la red WiFi.
- Obtener la IP desde el Monitor Serie o la app `Fing`.
- Acceder desde el navegador: `http://<IP_DEL_ESP32>`.

## 2️⃣ **Uso de la Interfaz Web**
La interfaz web permite gestionar la configuración y actualizar el firmware del ESP32. A continuación, se presentan los pasos para su uso:

### 🔹 **Estado del ESP32**
![Interfaz Web - Estado ESP32](https://github.com/JesPezz/EstacionMeteorologica/blob/main/docs/img/2025-02-23.png)
- **Muestra la dirección IP del ESP32**.
- **Estado de la conexión WiFi**.
- **Uso de CPU y memoria libre**.

### 🔹 **Configuración del ESP32**
![Interfaz Web - Configuración](https://github.com/JesPezz/EstacionMeteorologica/blob/main/docs/img/2025-02-23%20(1).png)
1. Ingresar **SSID y contraseña** del WiFi.
2. Configurar **ThingSpeak API Key** y **Channel ID**.
3. Ingresar la **ubicación del sensor**.
4. Configurar el **intervalo de actualización OTA (horas)**.
5. Guardar los cambios y reiniciar.

### 🔹 **Actualización OTA**
![Interfaz Web - Actualización OTA](https://github.com/JesPezz/EstacionMeteorologica/blob/main/docs/img/2025-02-23%20(2).png)
- Seleccionar un archivo de firmware `.bin`.
- Hacer clic en `Subir Firmware`.
- Esperar la actualización y reinicio automático.

### 🔹 **Reinicio del ESP32**
- Presionar el botón `🔄 Reiniciar ESP32` para forzar un reinicio.

### 🔹 **Notificaciones por Telegram**
- Obtener un bot de Telegram y la API Token.
- Configurar el **Chat ID** en la interfaz web.
- Recibir notificaciones de cambios y alertas.

---

## ⚡ **Solución de Problemas**
| Problema | Solución |
|----------|---------|
| No conecta a WiFi | Verificar SSID y contraseña en `config.json` |
| No carga la interfaz | Revisar la IP asignada por el router |
| Datos no llegan a ThingSpeak | Verificar API Key y conexión a Internet |
| Fallo en actualización OTA | Probar con otro firmware `.bin` |

---

## 📜 **Licencia**
Este proyecto está bajo la licencia **MIT**. Si deseas contribuir, realiza un **pull request** en el repositorio oficial.

🚀 **Desarrollado por JesPezz**



