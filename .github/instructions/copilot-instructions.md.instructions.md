# Reglas de Release y Control de Versiones

1. **Sincronización Obligatoria de Versión en Código:**
   - NUNCA generes un Release en GitHub ni compiles un binario de producción (`firmware.bin`) sin antes buscar y actualizar la variable de versión en el código fuente (por ejemplo: `const char* version`, `APP_VERSION`, `#define VERSION`).
   - El valor de la variable en el código DEBE coincidir carácter por carácter con el Tag del Release (ej. `v4.3.2-MQTT`).
   - Esto es crítico para prevenir bucles infinitos en el sistema de actualización OTA del microcontrolador.

2. **Validación Previa a Compilaciones de Release:**
   - Antes de ejecutar `pio run` o `gh release create`, realiza una búsqueda global en el espacio de trabajo de la versión anterior y reemplázala por la nueva versión objetivo en todos los archivos de configuración y cabecera.

   3. **Siempre usa el idioma español:**

   # Reglas de Versionado e Incremento Automático

1. **Incremento Automático de Versión:**
   - Antes de realizar una compilación para producción (`pio run`), crear un Pull Request (PR) o publicar un Release en GitHub, debes inspeccionar la versión actual registrada en el código y en los Tags del repositorio.
   - Incrementa automáticamente el número consecutivo de versión (patch/subversión).
     - *Ejemplo:* Si la versión actual es `v4.3.3-MQTT`, cámbiala a `v4.3.4-MQTT`.
     - Conserva siempre el formato exacto, prefijos y sufijos existentes (como `-MQTT`).

2. **Excepción por Versión Personalizada:**
   - Si el usuario especifica explícitamente una versión distinta en su solicitud (por ejemplo: *"haz el release v5.0.0"* o *"usa la v4.4.0-MQTT"*), omite el incremento automático y utiliza exactamente la versión indicada por el usuario.

3. **Sincronización en Código:**
   - Recuerda actualizar la variable global de versión en el código fuente (ej. `const char* version`, `APP_VERSION`, `#define VERSION`) para que coincida exactamente con la nueva versión calculada antes de compilar y generar el binario `.bin`.

   # Reglas de Documentación Post-Release

1. **Actualización Obligatoria de `CHANGELOG.md`:**
   - Después de cada release o PR importante, agrega una nueva sección al inicio de `CHANGELOG.md` con la versión generada y la fecha actual (ejemplo: `## [v4.3.4-MQTT] - YYYY-MM-DD`).
   - Clasifica y detalla brevemente los cambios realizados usando el estándar:
     - **Agregado (Added):** Nuevas funcionalidades o endpoints.
     - **Cambiado (Changed):** Modificaciones en lógica existente o flujo UI.
     - **Corregido (Fixed):** Corrección de bugs o parches.

2. **Actualización de `README.md`:**
   - Si la nueva versión incluye características visibles (nuevos campos de configuración, comportamientos de autenticación, endpoints o estados de interfaz), actualiza las secciones correspondientes de `README.md`.
   - Asegúrate de que la portada del repositorio refleje siempre el estado de la versión más reciente.