# Reglas de Compilación, Release y Versionado

### 1. Política de Compilación (`pio run`)
- **NO ejecutes `pio run`** tras realizar cambios menores, refactorizaciones o edición de código durante el desarrollo cotidiano.
- Únicamente ejecuta `pio run` cuando el usuario solicite explícitamente **crear un Release**, **generar el binario de producción** o probar la compilación.
- No hay infraestructura de tests automatizados (`test/` solo contiene el README por defecto de PlatformIO). La verificación se realiza exclusivamente compilando.

---

### 2. Arquitectura, Toolchain y Gotchas de Código
- **Entorno PlatformIO:** ESP32 Arduino framework, un único entorno: `esp32doit-devkit-v1`.
  - Comando de compilación: `pio run -e esp32doit-devkit-v1`
  - Binario generado: `.pio/build/esp32doit-devkit-v1/firmware.bin`
- **Web UI & SPIFFS:** 
  - La interfaz web vive en `Data/index.html` y se incrusta en el binario mediante `board_build.embed_txtfiles`.
  - Modificar el frontend requiere compilar el firmware **y** generar/subir la imagen SPIFFS (`pio run -t buildfs` / `pio run -t uploadfs`).
  - El sistema de ficheros es **SPIFFS** (no LittleFS). Archivos clave: `/config.json` (migra automáticamente desde `/wifi.json` en arranque) y `/error.log`.
- **Librerías y Dependencias:**
  - `lib_archive = no` en `platformio.ini` es **OBLIGATORIO** para que la librería BSEC de Bosch compile correctamente.
  - Librerías privadas/vendidas en `lib/` (BSEC-Arduino-library, ESP-Mail-Client-master).
- **ArduinoJson v7:** Usar estrictamente `JsonDocument` (nunca `DynamicJsonDocument`, ya que fue eliminado en la versión 7).
- **Includes duplicados:** `main.cpp` contiene includes repetidos (ej. `web_server.h`, `led_task.h`, `VoltageMonitor.h`) que compilan correctamente: no los elimines ni refactorices a menos que sea estrictamente necesario.

---

### 3. Flujo Integrado de Release (Paso a Paso)
Cuando el usuario pida realizar un release (ej. *"haz el release"* o *"publica la nueva versión"*), ejecuta **estrictamente** esta secuencia:

1. **Obtener la versión actual:**
   - Consulta el último tag/release del repositorio (usando `gh release list` o `git tag`) y la variable de versión en el código fuente.
   - **Ubicación exacta de la versión:** `src/config.cpp` (en la línea `const char* version = "...";`). No existe `#define VERSION` en este proyecto.

2. **Determinar la nueva versión:**
   - **Modo Automático:** Si el usuario NO especificó una versión, incrementa el consecutivo del número parche/subversión manteniendo el formato exacto, prefijos y sufijos (ejemplo: de `v4.3.4-MQTT` a `v4.3.5-MQTT`).
   - **Modo Personalizado:** Si el usuario indicó una versión explícita (ej. *"haz la v5.0.0"*), usa exactamente esa versión.

3. **Sincronizar el Código Fuente y Docs:**
   - Reemplaza el valor de `version` en `src/config.cpp` por la nueva versión calculada.
   - *Precaución OTA:* El módulo OTA (`ota_update.cpp`) compara la versión local (`version`) con el `tag_name` del release en GitHub. Si difieren, el microcontrolador entrará en bucle de actualización. La sincronización en código es obligatoria antes de compilar.
   - Actualiza también el título y badges en `README.md` si aplica.

4. **Compilar el Binario:**
   - Ejecuta `pio run` para generar el firmware actualizado con la versión correcta embebida.

5. **Documentar y Publicar:**
   - Agrega la entrada correspondiente en `CHANGELOG.md` con la fecha actual y detalla las secciones (**Agregado**, **Cambiado**, **Corregido**).
   - Publica el Release en GitHub (`gh release create`) adjuntando el binario recién compilado (`.pio/build/esp32doit-devkit-v1/firmware.bin`).

---

### 4. Idioma
- Toda la comunicación, documentación, mensajes de commit y notas de release deben redactarse en **español**.

# Reglas Anti-Bucle y Control de Errores de Compilación

1. **Límite de Reintentos en Compilación (`pio run`):**
   - Si una compilación con `pio run` falla **2 veces consecutivas**, DETÉN todo el proceso de edición automática inmediatamente.
   - NO intentes parchar archivos repetidamente sin detenerte a analizar.
   - Presenta el error del compilador/linker de forma resumida al usuario y solicita confirmación o guía antes de continuar.

2. **Refactorización Completa de Firmas (Headers `.h` e Implementación `.cpp`):**
   - Si cambias el tipo de un parámetro o retorno en una función (ejemplo: cambiar `DynamicJsonDocument&` to `JsonDocument&`), debes actualizar OBLIGATORIAMENTE y en un solo paso:
     1. La declaración en el archivo de cabecera (`.h`).
     2. La definición en el archivo de implementación (`.cpp`).
     3. Todas las llamadas a dicha función en otros archivos.
   - NO ejecutes `pio run` hasta haber verificado visualmente la coincidencia exacta de tipos entre `.h` y `.cpp`.

3. **Diagnóstico de Errores de Linker (`undefined reference`):**
   - If the error is `undefined reference to...`, NUNCA edites llamadas en archivos consumidores (`main.cpp`, `MQTTManager.cpp`).
   - Revisa de inmediato la firma exacta en el `.h` vs la firma en el `.cpp` del módulo origen.

---

## Notificaciones
- Al finalizar una tarea exitosamente (después de realizar el commit y/o push), ejecuta el siguiente comando en la shell para notificar por Telegram:
  ```bash
  curl -s -X POST "https://api.telegram.org/bot${TELEGRAM_BOT_TOKEN}/sendMessage" -d "chat_id=${TELEGRAM_CHAT_ID}" -d "text=✅ OpenCode finalizó la tarea en EstacionMeteorologica"
  ```

---

## Referencia a otras instrucciones
- El archivo `.github/instructions/copilot-instructions.md.instructions.md` está **obsoleto** y contiene conflictos de merge sin resolver. Ignóralo por completo; este `AGENTS.md` es la única fuente de verdad para agentes en este repositorio.
