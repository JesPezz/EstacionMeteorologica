# Changelog

All notable changes to this project are documented in this file.

## Unreleased - 2026-09-06

- **Cambiado:** Cooldown del envío a **ThingSpeak** en Node-RED (`nodered_flow.json`, nodo «Limitador por Dispositivo») de **20 s a 5 min (300000 ms)** para no agotar la cuota gratuita (3M mensajes/año) antes de su renovación (2027-02-06). Con 4 dispositivos el consumo proyectado baja de ~12.000 a **~1.150 msgs/día** (~176K hasta la renovación, frente a ~539K disponibles).
  - Se despliega editando `~/.node-red/flows.json` en el servidor (192.168.1.107) y reiniciando `nodered.service`; respaldo en `flows.json.bak`.
- **Agregado:** Automatización para recuperar resolución tras la renovación: script idempotente `/usr/local/bin/thingspeak_bajar_a_60s.py` + cron `0 3 6 2 *` que el 2027-02-06 baja el cooldown a **60 s** (cubre el año con margen con 4 dispositivos: ~2,1M/3M).
- **Documentado:** Límites de la cuenta gratuita de ThingSpeak (FAQ oficial): **3M mensajes/año**, **4 canales** máximo, intervalo mínimo de actualización de **15 s**. Un 5º dispositivo requiere plan de pago (Home: 33M msgs/año y 10 canales).
- **Nota histórica (por qué no se agotaba antes):** hasta 2025-11 el ESP32 publicaba por MQTT cada 60 s; `src/main.cpp` pasó a 3 s (commit `34f215a`) y se añadió el modo multi-dispositivo con limitador de 20 s por dispositivo (commit `a7d9952`), disparando el consumo a ~11-12K msgs/día en el ciclo actual.

## v5.2.4-MQTT - 2026-09-10

- **Corregido:** Timeouts de keepalive en MQTT: los ESP32 conectaban al broker pero Mosquitto los expulsaba con `has exceeded timeout, disconnecting` (se quedaban mudos por el *modem sleep* del WiFi, que retrasaba los PINGREQ de la tarea síncrona de espMqttClient).
  - `src/MQTTManager.cpp`: `WiFi.setSleep(false)` añadido en `setupMQTT()` para desactivar el ahorro de energía del modem Wi-Fi y mantener el flujo de PINGREQ dentro del keepalive de 15 s.
  - Elimina las falsas alertas «🚨 Broker MQTT inaccesible (WiFi OK)» del watchdog de broker.
- `src/config.cpp`: versión sincronizada a `v5.2.4-MQTT` (precaución OTA: el módulo compara `version` con el `tag_name` del release).

## v5.2.3-MQTT - 2026-08-21

- **Agregado:** Watchdog de broker MQTT en `src/main.cpp` que detecta la caída del broker (Raspberry/Mosquitto) aunque el WiFi siga operativo — reconfiguración de red, cambio de IP del broker o Mosquitto detenido:
  - Si hay WiFi pero MQTT no logra mantener la conexión durante 15 s, se registra en el log, se notifica por Telegram una única vez por evento y se activa el backlog.
  - Diferencia explícitamente "caída del broker" de "caída del WiFi", cerrando el hueco donde los datos se perdían sin respaldo.
- **Cambiado:** Intervalo de respaldo offline mantenido en **1 hora** (`OFFLINE_SAVE_INTERVAL_MS = 3600000`) en `src/main.cpp` para no saturar la memoria SPIFFS; la primera escritura al entrar en el modo offline es **inmediata**, evitando perder datos al inicio de una caída del broker.
- **Corregido:** `publishSensorData()` en `src/MQTTManager.cpp` ahora respalda a backlog inmediatamente cuando `mqttClient.connected()` es `false` (broker inaccesible aunque el WiFi esté OK), en lugar de descartar los datos durante la ventana previa a la desconexión TCP.
- `src/config.cpp`: versión sincronizada a `v5.2.3-MQTT` (precaución OTA: el módulo compara `version` con el `tag_name` del release).

## v5.2.2-MQTT - 2026-08-10

- **Cambiado:** Migración de la capa MQTT de `AsyncMqttClient` (con su pila `AsyncTCP-esphome`) a **`espMqttClient`** (v1.7.3, síncrono sobre `WiFiClient`), eliminando la fuga de memoria de fondo que persistía incluso con QoS 0 (~230 B/s ≈ ~700 B por publicación cada 3 s, acumulables a ~220 KB/hora con QoS 1).
  - `lib/espMqttClient/`: librería vendida en el repositorio (patrón del proyecto), sin dependencias (`library.json`) para evitar traer `ESP32Async/AsyncTCP` v3.x, API-incompatible con `AsyncTCP-esphome`.
  - `AsyncTCP-esphome@^2.1.3` se mantiene únicamente para `ESPAsyncWebServer-esphome` (servidor web); el camino MQTT ya no usa AsyncTCP.
  - `src/MQTTManager.cpp` / `include/MQTTManager.h`: `espMqttClient mqttClient;`, callbacks adaptados (`onConnect(bool)`, `onDisconnect(espMqttClientTypes::DisconnectReason)`, `onMessage(...)` con `const uint8_t*`); `resetMQTTClient()` ya no usa *placement new*: `disconnect(true)` + `clearQueue()` + `setupMQTT()` + `connectToMqtt()`.
  - `src/MQTTCommands.cpp` / `include/MQTTCommands.h`: `setupMQTTWill`, `subscribeMQTTCommands` y `handleMqttMessage` adaptados a la nueva API; publicaciones y Will con `const uint8_t*` + longitud.
  - `src/main.cpp` y `src/OfflineManager.cpp`: comentarios de integración actualizados (sin cambios funcionales).
- **Corregido:** El bucle de procesamiento del backlog en `src/main.cpp` (envío por lotes con `MAX_BATCH=5`) se disparaba con demasiada frecuencia al reanudar la conexión MQTT.
  - Se reduce la carga procesando el backlog por lotes cada `OFFLINE_SAVE_INTERVAL_MS` (1 hora) en lugar de hacerlo continuamente, preservando memoria durante la recuperación.
- `src/config.cpp`: versión sincronizada a `v5.2.2-MQTT` (precaución OTA: el módulo compara `version` con el `tag_name` del release).

## v5.2.1-MQTT - 2026-08-10

- **Corregido:** Fuga de memoria en la capa de red MQTT que agotaba el heap tras horas de operación y dejaba el dispositivo sin red (WiFi no podía reinicializar: `esp_wifi_init` → `ESP_ERR_NO_MEM`).
  - `src/MQTTManager.cpp`: las publicaciones de telemetría en tiempo real (`publishSensorData()`) pasan de QoS 1 a **QoS 0**. Cada mensaje QoS 1 generaba un PUBACK entrante del broker que el flujo RX de `AsyncTCP-esphome` retenía (~184 B por publicación, liberados solo al caer la conexión TCP). Al publicar cada 3 s, la fuga era de ~220 KB/hora. Con QoS 0 desaparecen los PUBACK entrantes; es apropiado para telemetría periódica donde la pérdida puntual de un dato es tolerable.
  - `src/main.cpp`: añadido **watchdog de heap** como red de seguridad: si el heap libre baja de 45 KB, se registra en `/error.log` y el dispositivo se reinicia para recuperar memoria antes del punto de no retorno.

## v5.2-MQTT - 2026-08-10

- **Corregido:** `processBacklog()` publicaba en el broker cualquier línea del archivo de respaldo, incluyendo registros corruptos/basura (p.ej. `{}` residuales de sesiones v5.0.x), contaminando MQTT, Node-RED e InfluxDB con datos vacíos.
  - `src/OfflineManager.cpp`: cada registro se valida con `deserializeJson()` antes de publicarlo, exigiendo los campos `location`, `fechaHora` y `temperature` (todo registro legítimo generado por `populateSensorJson()` los incluye).
  - Los registros inválidos se descartan (no cuentan en el lote `MAX_BATCH`), se registran en `/error.log` y quedan eliminados del archivo, evitando que se reenvíen en llamadas posteriores.

## v5.1-MQTT - 2026-08-10

- **Corregido:** El Auto-Rollback OTA se disparaba también durante caídas de WiFi en tiempo de ejecución: al agotar los reintentos de conexión, `connectToBestWiFi()` marcaba la imagen como inválida (`esp_ota_mark_app_invalid_rollback_and_reboot()`) y el dispositivo arrancaba con la versión anterior en lugar de ejecutar la lógica de backlog.
  - `src/WiFiManager.cpp`: `connectToBestWiFi()` ya no ejecuta rollback ni reinicio al agotar los reintentos; devuelve `false` y se activa el modo AP, manteniendo la lógica offline/backlog.
  - `src/main.cpp`: el rollback de OTA ahora solo se evalúa al arrancar para imágenes recién instaladas, comprobando el estado `ESP_OTA_IMG_PENDING_VERIFY` de la partición en ejecución mediante `esp_ota_get_state_partition()`:
    - Si la imagen nueva arranca con WiFi conectado → `esp_ota_mark_app_valid_cancel_rollback()` (se confirma la actualización).
    - Si la imagen nueva arranca sin WiFi → rollback a la partición anterior (fail-safe OTA real).
    - Si la imagen ya fue confirmada (no `PENDING_VERIFY`), **nunca** se hace rollback, solo modo AP/backlog.

## v5.0.11-MQTT - 2026-08-08

- **Agregado:** Comandos MQTT duales para operar la estación de forma remota (individual + broadcast), con respuestas JSON por MQTT.
  - Nuevos `include/MQTTCommands.h` y `src/MQTTCommands.cpp`:
    - `getLocationTag()` normaliza la ubicación (`config.location`) a minúsculas y guiones bajos para usar como tag en tópicos.
    - Suscripción dual en `subscribeMQTTCommands()`: `estacion/<ubicacion>/comando` (individual) y `estacion/all/comando` (broadcast).
    - `handleMqttMessage()` reconstruye payloads fragmentados y despacha al manejador.
    - Comandos soportados: `ip`, `estado`, `clima`, `metricas`, `reiniciar` (reinicio no bloqueante tras 1s) y `actualizar` (programa la OTA con jitter aleatorio de 1–10 s en broadcast; 1 s en individual).
    - `publishReport()` envía la respuesta JSON en `estacion/reporte/<location>`.
  - LWT (Last Will) configurado en `setupMQTTWill()`: al caer el dispositivo publica `{"ubicacion":..., "status":"offline"}` con retain en el tópico de reporte, de modo que cualquier suscriptor (p.ej. Node-RED) detecta la desconexión al instante.
  - Integración en `src/MQTTManager.cpp`: `onMqttConnect()` suscribe los comandos y `onMessage` registrado; `setupMQTT()` configura el Will.
  - `src/main.cpp`: `processPendingMqttActions()` en el `loop()` procesa reinicios/OTAs programados sin bloquear.
  - Consolidado con el flujo Node-RED (agregación de respuestas en ventana de 3 s y alertas por Telegram).

## v5.0.10-MQTT - 2026-08-08

- **Corregido:** Panic/Crash al descargar el log desde la interfaz web (`/downloadLog` y `/logview`).
  - El acceso concurrente a SPIFFS desde la tarea del loop (`writeLog()` cada ~30 s, más SSE/exención) mientras el servidor AsyncWebServer leía el mismo `/error.log` podía corromper la FS interna y provocar el panic.
  - Añadido `spiffsMutex` global (`SemaphoreHandle_t`) en `src/config.cpp` que serializa todo acceso a SPIFFS.
  - `writeLog()`, `handleDownloadLog()` y `/logview` ahora toman el mutex antes de abrir/leer/escribir y lo liberan al terminar.
  - Invés de `request->send(SPIFFS, ...)` (streaming asíncrono que reabre el archivo fuera del mutex), `/downloadLog` lee el archivo completo →String bajo el mutex y lo envía como respuesta en memoria (el log está limitado a `MAX_LOG_SIZE=10 KB`).

## v5.0.9-MQTT - 2026-08-08

- **Corregido:** Panic de stack (`Stack canary watchpoint triggered (loopTask)`) durante la OTA al arrancar, que entraba en bucle infinito (OTA → crash → reboot). El algoritmo de descarga anidaba hasta 4 objetos `WiFiClientSecure`+`HTTPClient`+JSON en el stack del loopTask.
  - `checkForUpdates()` ahora **retorna** la URL de descarga (`String`) y cierra su HTTP antes de liberar el stack, eliminando las funciones redundantes `getFirmwareURL()` y `getFinalURL()`.
  - `downloadAndUpdate(const String &firmwareURL)` recibe la URL directamente y gestiona el redirect 302 de GitHub con `setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)`.
  - El búfer de descarga se movió al heap (`malloc(2048)` en lugar de stack) para no consumir stack del loop `loopTask`.
  - Stack del loop aumentado de 16384 a 32768 bytes (`platformio.ini`).

## v5.0.8-MQTT - 2026-08-08

- **Corregido:** El MQTT ya no queda incapaz de reconectarse tras una caída/reconexión WiFi. El `AsyncClient` TCP interno de `AsyncMqttClient` queda con estado corrupto después de una desconexión WiFi y `connect()` repetido (guard de 15s) reintentaba infinitamente con motivo `TCP_DISCONNECTED`.
  - Nueva función `resetMQTTClient()` en `src/MQTTManager.cpp` que **reconstruye el objeto** `AsyncMqttClient` con placement-new (método recomendado para la librería), pone `lastMqttRetry = 0` para permitir reconexión inmediata y vuelve a configurar credenciales/servidor.
  - En `src/main.cpp` `loop()` se detecta la **transición de WiFi de off→online**: al reconectar, se llama `resetMQTTClient()` exactamente una vez.
  - `setupMQTT()` ahora solo crea `mqttReconnectTimer` si aún no existe (evita fugas de timers al reconstruir repetidamente).

## v5.0.7-MQTT - 2026-08-08

- **Corregido:** La descarga OTA ya no se cuelga ni falla permanentemente si la conexión TLS con el CDN de GitHub se corta a mitad de descarga (error `(-76)` en `ssl_client.cpp`).
  - `downloadAndUpdate()` en `src/ota_update.cpp` ahora reintenta hasta **3 veces** la descarga completa (nueva conexión HTTP+TLS por intento).
  - Se añadió **timeout de estancamiento** (`STALL_TIMEOUT_MS = 30000`): si no llegan datos durante 30s se aborta el intento y se reintenta en lugar de quedarse en `while (written < contentLength)` indefinidamente.
  - Búfer de lectura ampliado de `1024` a `2048` bytes para reducir número de llamadas de red.
  - Se hace `Update.abort()` al fallar cada intento para dejar la partición OTA lista para el siguiente, y solo se notifica por Telegram el fallo total (3 intentos agotados).

## v5.0.6-MQTT - 2026-08-08

- **Corregido:** Ráfaga de logs al conectar/reconectar MQTT. Se eliminaron los mensajes de depuración del cabecera de `connectToMqtt()` (`********` y `📡 Intentando conectar a MQTT...`) que se imprimían en cada vuelta del `loop()` (CASO B) a pesar del guard de 15 s. `connectToMqtt()` queda silencioso salvo cuando de verdad ejecuta `mqttClient.connect()`.
- **Corregido:** `abort()` (`bad_alloc` → `std::terminate`) durante la descarga OTA. El crash provenía del poll del frontend (`/esp_status` cada 10s y SSE `/events` cada 5s) que con el heap drenado por la OTA lanzaba una excepción C++ no capturada en `request->send`. Ahora:
  - `handleESPStatus()` en `src/web_server.cpp` responde `202/503 {"ota":true}` sin asignar `String`/JSON cuando `otaInProgress`.
  - `sendSSEData()` y el callback del `sseTimer` devuelven temprano si `otaInProgress` (no construyen `getSensorJson()`).

## v5.0.5-MQTT - 2026-08-08

- **Agregado:** Temporizador de reintento MQTT no bloqueante en `src/MQTTManager.cpp` — variable global `unsigned long lastMqttRetry = 0;` y constante `MQTT_RETRY_INTERVAL_MS = 15000`. `connectToMqtt()` ahora solo ejecuta `mqttClient.connect()` si `WiFi.status() == WL_CONNECTED`, el cliente no está conectado y han transcurrido al menos **15 segundos** desde el último intento, evitando `churn` en la pila de sockets TCP/IP. Todas las vías de reconexión (el timer de 10s de `mqttReconnectTimer` y el bucle principal) pasan por este guard.
- **Cambiado:** En `src/main.cpp`, el bloque CASO B de reconexión MQTT ahora delega en `connectToMqtt()` (que aplica el intervalo de 15 s), eliminando el temporizador local `lastMqttAttempt` de 10 s.

## v5.0.4-MQTT - 2026-08-08

- **Agregado:** Reducción del intervalo de guardado offline en `src/main.cpp` de 1 hora a **30 segundos** (`OFFLINE_SAVE_INTERVAL_MS = 30000`) para generar rápidamente múltiples registros durante simulaciones de desconexión Wi-Fi.
- **Refactorizado:** `src/OfflineManager.cpp` — `processBacklog()`:
  - Al iniciar la sincronización, renombra `/backlog.txt` a `/backlog_proc.txt` (`SPIFFS.rename`) para separar la escritura de nuevos registros de la lectura de históricos.
  - Lee los registros con un búfer estático `char lineBuffer[512]` usando `file.readBytesUntil('\n')`, eliminando asignaciones dinámicas de `String` (cero fragmentación de Heap).
  - Envío limitado a un máximo de **5 registros por llamada** (`MAX_BATCH = 5`) con `delay(100)` entre publicaciones para no saturar la cola en RAM de `AsyncMqttClient`; los registros no enviados se conservan en `/backlog_rest.txt` y se renombran para la próxima llamada.
  - Elimina `/backlog_proc.txt` al vaciar todo el archivo.

## v5.0.3-MQTT - 2026-08-08

- **Agregado (diagnóstico):** Impresiones temporales de `ESP.getFreeHeap()` ANTES y DESPUÉS de las rutinas de armado/publicación MQTT (`publishSensorData`), de lectura/escritura NVS (`loadState`/`updateState`) y de la tarea de fondo SSE (timer de 5 s) para identificar qué función provoca la bajada de Heap.
- **Corregido:** Cierre estricto de NVS (`Preferences`) en `setupBsecSensor()` (`src/SensorManager.cpp`): la apertura `bsecPrefs.begin("bsec_data", false)` ahora se cierra inmediatamente con `bsecPrefs.end()`, evitando manejadores NVS abiertos que acumulaban memoria.
- **Verificado:** `populateSensorJson()` arma la trama MQTT únicamente con `JsonDocument&` (sin concatenaciones masivas de `String`); `handleDownloadLog` ya sirve `error.log` por stream directo desde SPIFFS (`request->send(SPIFFS, ...)`) sin cargar el archivo en RAM.

## v5.0.2-MQTT - 2026-08-08

- **Corregido:** Crash por `abort()` en Core 1 tras ~7 minutos de ejecución continua, causado por agotamiento de Stack de la tarea `loopTask` combinado con `JsonDocument` locales pesados en el Stack.
- **Agregado:** Monitoreo de memoria Heap en `loop()` (cada 30 s): se registra en el log `ESP.getFreeHeap()` y `ESP.getMinFreeHeap()`, con alerta en Serial cuando quedan menos de 40 KB libres.
- **Optimizado:** `publishSensorData()` en `src/MQTTManager.cpp` ahora usa un `JsonDocument` estático (`static`) y serializa sobre un búfer estático (`char payloadBuffer[1024]`), eliminando el `DynamicJsonDocument` y las asignaciones de `String` en cada publicación MQTT.
- **Configuración:** Aumentado el Stack de la tarea Arduino a 16 KB mediante `-DCONFIG_ARDUINO_LOOP_STACK_SIZE=16384` en `build_flags` de `platformio.ini`.

## v5.0.1-MQTT - 2026-08-08

- **Corregido:** Error de descarga SSL en OTA: el cliente HTTPS (`WiFiClientSecure`) de `downloadAndUpdate()` ahora usa `client.setInsecure()` (omite validación estricta de certificados y ahorra RAM SSL) y aumenta el timeout del socket a 30 segundos (`client.setTimeout(30)`) en `src/ota_update.cpp`.
- **Corregido:** Durante el bucle de descarga del firmware se añaden llamadas periódicas `yield()`/`vTaskDelay(1)` para alimentar el Watchdog y evitar cierres de socket MbedTLS durante la escritura a flash.
- **Optimizado:** Eliminadas las lecturas repetitivas de `config.json` desde SPIFFS durante `loop()` y reintentos de reconexión. `loadSavedNetworks()` y `printWiFiNetwork()` (en `src/WiFiManager.cpp`) ahora usan la configuración y redes Wi-Fi ya cargadas en RAM desde `setup()`; `loadConfig()` solo se ejecuta una vez al arrancar.

## v5.0-MQTT - 2026-08-08

- **Agregado:** Mecanismo de Auto-Rollback OTA (Fail-Safe) para recuperación automática ante fallos tras una actualización:
  - Validación de firmware: Al confirmar la conexión Wi-Fi y el funcionamiento inicial del sistema, `setup()` ejecuta `esp_ota_mark_app_valid_cancel_rollback()` (en `src/main.cpp`) confirmando la imagen como válida y cancelando el rollback pendiente.
  - Recuperación ante fallo: Si la secuencia de conexión Wi-Fi agota todos sus reintentos, `connectToBestWiFi()` (en `src/WiFiManager.cpp`) marca la imagen como inválida con `esp_ota_mark_app_invalid_rollback_and_reboot()` para volver a la partición funcional anterior. Si el arranque no proviene de OTA, se reinicia el sistema normalmente.
  - Requiere el esquema de particiones duales OTA (`otadata` + `ota_0`/`ota_1`) ya presente en `partitions.csv`.

## v4.4.0-MQTT - 2026-08-08

- **Eliminado:** Removido completamente el sistema obsoleto de actualización automática de `index.html` desde GitHub (`checkForIndexUpdate()` y `updateFileFromURL()`), incluyendo las variables globales asociadas (`host`, `url`, `etagFilePath`, `indexURL`). La interfaz web se embebe en el firmware desde el binario (PROGMEM) desde la v4.3.9, por lo que este mecanismo era dead code que consumía recursos y generaba ruido de notificaciones.
- **Optimizado:** El canal Beta (Modo Prueba) ahora consulta la API de GitHub con `?per_page=1` (`/releases?per_page=1`) tanto en `getFirmwareURL()` como en `checkForUpdates()`, reduciendo la respuesta JSON y el tiempo de las llamadas OTA.
- **Cambiado:** El Modo Prueba (canal Beta/Estable) se carga de `Preferences` al inicio de `setup()`, registrando en el log el canal seleccionado para facilitar el diagnóstico de bucles de actualización.

## v4.3.10-MQTT - 2026-08-07

- **Corregido:** Fuga de memoria (`ESP_ERR_NO_MEM` / error 257) durante la reconexión Wi-Fi agregando desconexión limpia (`WiFi.disconnect(true); delay(100);`) antes de cada reintento, límite global de rondas (3 rondas completas), y reinicio controlado (`ESP.restart()`) en caso de fallo crítico para liberar memoria RAM.

## v4.3.9-MQTT - 2026-08-07

- **Agregado/Optimizado:** Embebido nativo del archivo HTML (`data/index.html`) mediante PlatformIO (`board_build.embed_txtfiles = data/index.html`) y servicio directo desde PROGMEM (`send_P`) para unificar firmware e interfaz web en un solo binario.
- **Corregido:** Endpoint `POST /api/test-mode` y función `toggleTestMode` en frontend usando `URLSearchParams` con `application/x-www-form-urlencoded` y optimización de guardado en `Preferences` solo cuando hay cambios reales.

## v4.3.8-MQTT - 2026-08-07

- **Corregido:** El switch del canal OTA (Beta/Estable) no se mantenía activo debido a discrepancias en el parseo del cuerpo de la petición POST en `web_server.cpp` (ahora soporta `application/x-www-form-urlencoded` con `plain=`) y al procesamiento de la respuesta JSON `{"status":"ok", "test_mode": true}` en `Data/index.html`. El switch ahora actualiza correctamente su estado con la confirmación real del ESP32 y revierte en caso de error.

## v4.3.7-MQTT - 2026-08-07

- **Corregido:** Fallo crítico de renderizado en la interfaz web donde se mostraba código JavaScript como texto plano en pantalla; se configuró la ruta raíz `/` y `/index.html` en `web_server.cpp` para forzar el Content-Type `"text/html"` y servir con respaldo embebido en PROGMEM (`send_P`) cuando SPIFFS no esté inicializado.

## v4.3.6-MQTT - 2026-08-07

- **Agregado:** Funcionalidad de "Modo Prueba / Canal Beta" para actualizaciones OTA con persistencia en `Preferences`, endpoints API (`GET/POST /api/test-mode`), interruptor en la interfaz web y soporte dinámico para pre-releases en GitHub Releases (`/releases`).

## v4.3.5-MQTT - 2026-08-07

- **Corregido:** Fallos en la lógica de respaldo y restauración (`/api/backup` y `/api/restore`), agregando autenticación requerida y resolviendo colisiones de rutas.
- **Corregido:** Error `ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION` en descargas (`/api/backup` y `/downloadLog`) usando el manejo nativo `isDownload=true` de `beginResponse`.
- **Cambiado:** Limpieza del repositorio excluyendo archivos sensibles/auxiliares (`wifi.json`, `.github/instructions`) del control de versiones mediante `.gitignore`.
- Corregido borrado accidental de claves en `/config.json` durante migración desde `/wifi.json`.
- Valores por defecto seguros para `updateOta` y `vbatPin`.
- Prevención de bucles OTA y conflictos ADC2/WiFi.



Released: https://github.com/JesPezz/EstacionMeteorologica/releases/tag/v4.3.4.1-MQTT

Summary
- Bumped firmware version to `v4.3.4.1-MQTT` and published updated firmware binary.
- Refactored LED control module to use FreeRTOS task-based signaling via `led_task.cpp` and removed obsolete `led.cpp`.
- Replaced legacy LED helper calls with `signalLed(...)` and migrated include references to `led_task.h`.
- Preserved original LED timing semantics for success, progress, and error states.

Notes
- Binary firmware attached to the release: .pio/build/esp32doit-devkit-v1/firmware.bin
- Devices must be flashed with the attached firmware to prevent OTA loops and ensure the version tag matches the code.

## v4.3.4-MQTT - 2026-08-06

Released: https://github.com/JesPezz/EstacionMeteorologica/releases/tag/v4.3.2-MQTT

Summary
- Re-enabled live SSE updates from the ESP32 frontend with `EventSource('/events')` and reliable event listeners.
- Ensured `/sensor_data` always returns `battery_voltage` and `battery_status` for correct battery UI rendering.
- Added automatic dashboard startup on page load and resilient JSON parse handling in the frontend.
- Updated Wi-Fi scan endpoint to use asynchronous scanning and avoid blocking HTTP request handlers.

Notes
- Binary firmware attached to the release: .pio/build/esp32doit-devkit-v1/firmware.bin
- Devices must be flashed with the attached firmware for live sensor updates and battery state support.
- Related PR: https://github.com/JesPezz/EstacionMeteorologica/pull/5 (draft)

## v4.3.1-MQTT - 2026-08-05

Released: https://github.com/JesPezz/EstacionMeteorologica/releases/tag/v4.3.1-MQTT

Summary
- Embeded critical CSS into the frontend (data/index.html) so the UI renders correctly offline and no longer depends on external CDN for layout.
- Hardened frontend JavaScript JSON parsing and SSE handling to avoid the UI freezing when device responses are malformed.
- Fixed /events registration in the web server so AsyncEventSource keeps connections open (prevents SSE disconnects).
- Added battery monitoring fields (battery_voltage, battery_status) to the sensor API and displayed them in the UI.
- Improved logging for resets and voltage diagnostics.
- Reduced redundant NVS/spiffs writes to extend flash life.

Notes
- Binary firmware attached to the release: .pio/build/esp32doit-devkit-v1/firmware.bin
- Devices must be flashed with the attached firmware for server-side (web_server.cpp) fixes to take effect.
- Related PR: https://github.com/JesPezz/EstacionMeteorologica/pull/5 (draft)

