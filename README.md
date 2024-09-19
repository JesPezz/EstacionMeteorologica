Estación Meteorológica ThingSpeak v1.2
Este proyecto es una estación meteorológica basada en ESP32 que envía datos a ThingSpeak y los gestiona mediante Google Apps Script. Los datos se recogen desde sensores en varias ubicaciones y se integran en una hoja de cálculo de Google Sheets para su análisis.

Características del Proyecto
Hardware: Utiliza ESP32 para la recolección de datos de temperatura y otros parámetros con el sensor BME680.
Software: ThingSpeak para la visualización de datos, Google Apps Script para la gestión de estos datos, y un servidor que facilita la automatización.
Integración: El proyecto se integra con Google Sheets a través de un script personalizado, donde se almacenan los datos recibidos de los sensores.
Múltiples ubicaciones: se pueden recoger los datos desde multiples esp32
Diagrama de Conexión del ESP32 con el BME680
A continuación, se describe la conexión entre el ESP32 y el sensor BME680 usando el protocolo I2C:

ESP32 Pin	BME680 Pin
3.3V	VIN
GND	GND
GPIO 21	SDA
GPIO 22	SCL
Este diagrama muestra las conexiones necesarias para que el ESP32 pueda comunicarse con el BME680 y recoger datos ambientales.

Instalación
1. Clonar el repositorio
Clona este repositorio en tu máquina local:

bash
Copiar código
git clone https://github.com/tu-usuario/EstacionThingSpeakv1.2.git

2. Configurar los ESP32
Conectar los sensores BME680 al ESP32 siguiendo el diagrama de conexión anterior.
Configurar el código del ESP32 para que envíe los datos a ThingSpeak.
Subir el código correspondiente a cada ESP32 dependiendo de su ubicación.

3. Configurar Google Apps Script
Copiar el script que se encuentra en el directorio ScriptGoogleSheet.
Reemplazar el sheetId con el ID de tu Google Sheet.
Implementar y ejecutar el script en Google Apps Script para que reciba los datos.

4. Configurar ThingSpeak
Crear una cuenta en ThingSpeak y configurar los canales para recibir los datos de cada ESP32.
Asociar las claves de API de ThingSpeak en el código del ESP32.
Uso
Una vez configurado, los datos serán enviados automáticamente desde los ESP32 a ThingSpeak, y luego el script de Google Apps Script los almacenará en la hoja de cálculo de Google Sheets.

Instrucciones para la Configuración de Hojas de Cálculo y Canales ThingSpeak

1. Crear una Hoja de Cálculo (Google Sheet)
Accede a Google Sheets y crea una nueva hoja de cálculo.
Renómbrala como desees (por ejemplo, "Registro de Datos Meteorológicos").
Una vez creada, copia el ID de la hoja de cálculo desde la URL. El ID es la cadena larga entre /d/ y /edit en la URL de la hoja.
Ejemplo de URL: https://docs.google.com/spreadsheets/d/1A2B3C4D5E6F7G8H9I/edit
ID: 1A2B3C4D5E6F7G8H9I
Inserta este ID en el script del ESP32 donde corresponda, reemplazando el marcador de posición.

2. Configuración de un Canal en ThingSpeak
Accede a ThingSpeak y regístrate o inicia sesión en tu cuenta.
Crea un nuevo canal y completa los detalles según lo que vayas a monitorear (por ejemplo, temperatura, humedad, etc.).
Una vez creado el canal, ve a la configuración del canal y copia tanto el ID del canal como la API Key.
El ID del canal es un número que identifica tu canal.
La API Key es una cadena alfanumérica que necesitarás para enviar datos al canal.
3. Configuración en el Código
ID de la Sheet: Inserta el ID de la hoja de cálculo en el código para que los datos se registren correctamente.
ID del Canal y API Key de ThingSpeak: Asegúrate de introducir estos datos en el código para cada ubicación configurada.

4. Registro de Datos por Ubicación
Por cada ubicación que configures en el código, se creará una hoja dentro de la misma Google Sheet. Cada hoja registrará y analizará los datos específicos de esa ubicación.

Frecuencia de Registro: Los datos se registran cada hora de manera automática.

Instrucciones para Implementar el Script de Google Apps Script

1. Crear un Proyecto en Google Apps Script
Accede a Google Apps Script y crea un nuevo proyecto.

Copia y pega el script proporcionado que gestionará los datos de los sensores.

En el script, localiza la sección donde se debe insertar la ID de la hoja de cálculo y reemplaza el marcador de posición con la ID que obtuviste anteriormente.

Ejemplo:
javascript
Copiar código
var sheetId = '1A2B3C4D5E6F7G8H9I'; // Inserta aquí el ID de tu hoja de cálculo
var sheet = SpreadsheetApp.openById(sheetId);
Guarda el proyecto y despliega la implementación.

2. Implementar el Script
Haz clic en "Deploy" (Desplegar) y selecciona "Manage deployments" (Gestionar implementaciones).

Crea una nueva implementación y selecciona "Web app".

En la sección de "Execute the app as" (Ejecutar la app como), selecciona "Me" (tú mismo).

En "Who has access" (Quién tiene acceso), selecciona "Anyone, even anonymous" (Cualquiera, incluso anónimos).

Copia la URL de la implementación después de realizar el despliegue.

Ejemplo de URL: https://script.google.com/macros/s/1A2B3C4D5E6F7G8H9I1A2B3C4D5E6F7G8H9I1A2B3C4D5E6F7G8H9I/exec
3. Agregar la URL de Implementación al Código del ESP32
Abre el código de tu ESP32 y localiza la parte donde se hace la solicitud para enviar datos.

Inserta la URL de implementación que obtuviste en el paso anterior.

Ejemplo:

const char* scriptURL = "https://script.google.com/macros/s/1A2B3C4D5E6F7G8H9I1A2B3C4D5E6F7G8H9I1A2B3C4D5E6F7G8H9I/exec";

4. Verificación de Datos
Una vez que todo esté configurado, el ESP32 comenzará a enviar datos a la hoja de cálculo a través del script de Google Apps Script. Puedes verificar los registros en la hoja de cálculo y en ThingSpeak para asegurarte de que los datos están siendo recibidos correctamente.

Contribuir
Si deseas contribuir a este proyecto:

Haz un fork del repositorio.
Crea una nueva rama (git checkout -b feature/nueva-funcionalidad).
Realiza tus cambios y haz commit (git commit -am 'Añadir nueva funcionalidad').
Haz push de la rama (git push origin feature/nueva-funcionalidad).
Envía un pull request.
Licencia
Este proyecto está bajo la licencia MIT. Puedes ver más detalles en el archivo LICENSE.

