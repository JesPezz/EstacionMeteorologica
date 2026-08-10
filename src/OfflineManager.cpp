#include "OfflineManager.h"
#include "config.h"       // Para acceder al nombre del tópico y configuración
#include "SPIFFS.h"
#include "MQTTManager.h"  // Para usar mqttClient.publish()

const char* BACKLOG_FILE = "/backlog.txt";
const char* BACKLOG_PROC_FILE = "/backlog_proc.txt";
const char* BACKLOG_REST_FILE = "/backlog_rest.txt";

void saveToBacklog(JsonDocument& doc) {
    // 1. Abrir archivo en modo "Append" (añadir al final)
    File file = SPIFFS.open(BACKLOG_FILE, FILE_APPEND);
    if (!file) {
        Serial.println("❌ Error crítico: No se pudo abrir/crear el archivo de backlog");
        return;
    }

    // 2. Marcar los datos como "offline" para que sepas en Node-RED/Excel que fue recuperado
    doc["offline_flag"] = true; 

    // 3. Serializar el JSON directamente al archivo y agregar salto de línea
    if (serializeJson(doc, file) == 0) {
        Serial.println("❌ Error escribiendo en backlog");
    } else {
        file.println(); // IMPORTANTE: Salto de línea para separar cada registro JSON
        Serial.println("💾 Dato guardado en memoria interna (Backlog) correctamente.");
    }
    
    file.close();
}

void processBacklog() {
    // Solo procesar si hay conexión MQTT real
    if (!mqttClient.connected()) return;

    // 🔄 Separar escritura de lectura: si el activo /backlog.txt existe y no hay uno en
    // procesamiento, renombrarlo a /backlog_proc.txt para que no se acumulen nuevos
    // registros mientras se leen los históricos.
    if (!SPIFFS.exists(BACKLOG_PROC_FILE) && SPIFFS.exists(BACKLOG_FILE)) {
        if (SPIFFS.rename(BACKLOG_FILE, BACKLOG_PROC_FILE)) {
            Serial.printf("📂 Backlog movido a %s para procesamiento.\n", BACKLOG_PROC_FILE);
        } else {
            Serial.println("❌ Error al renombrar backlog.txt -> backlog_proc.txt");
            return;
        }
    }

    if (!SPIFFS.exists(BACKLOG_PROC_FILE)) return;

    File file = SPIFFS.open(BACKLOG_PROC_FILE, FILE_READ);
    if (!file) return;

    Serial.printf("📤 Iniciando sincronización de %s...\n", BACKLOG_PROC_FILE);

    // Búfer estático para evitar fragmentación de Heap (sin String dinámicos por registro)
    static char lineBuffer[512];
    size_t len = 0;
    int sentCount = 0;
    bool hasMore = false;

    // JsonDocument estático reusable para validar cada registro antes de publicarlo.
    static JsonDocument validateDoc;

    while (file.available()) {
        // Leer línea por línea sobre búfer estático
        len = file.readBytesUntil('\n', lineBuffer, sizeof(lineBuffer) - 1);
        lineBuffer[len] = '\0';

        // Limpiar \r y espacios finales
        while (len > 0 && (lineBuffer[len - 1] == '\r' || lineBuffer[len - 1] == ' ')) {
            lineBuffer[--len] = '\0';
        }

        if (len > 0) {
            // 🔍 Validar que el registro sea un JSON real con los campos esperados.
            // Descartar líneas corruptas/basura (p.ej. "{}" residuales de sesiones v5.0.x):
            // publicar un {} al broker contaminaría Node-RED/InfluxDB con datos vacíos.
            validateDoc.clear();
            DeserializationError vErr = deserializeJson(validateDoc, lineBuffer);
            bool valid = (vErr == DeserializationError::Ok) &&
                         validateDoc["location"].is<const char*>() &&
                         validateDoc["fechaHora"].is<const char*>() &&
                         validateDoc["temperature"].is<float>();
            if (!valid) {
                Serial.printf("⚠️ Descartando registro corrupto de backlog: %s\n", lineBuffer);
                writeLog("⚠️ Descartando registro corrupto de backlog");
                continue;
            }

            if (sentCount < MAX_BATCH) {
                mqttClient.publish(config.mqttTopic.c_str(), 1, false, lineBuffer);
                sentCount++;

                Serial.printf("📤 Reenviando dato histórico (%d/%d): %s\n", sentCount, MAX_BATCH, lineBuffer);

                // Pequeña pausa para no saturar la cola en RAM del cliente MQTT
                delay(100);
                yield();
            } else {
                // Alcanzamos el límite del lote: conservar esta línea y detener la lectura.
                // El puntero de lectura queda tras esta línea; lineBuffer+len la retienen.
                hasMore = true;
                break;
            }
        }
    }

    // Si quedan registros sin enviar, conservarlos para la próxima llamada
    if (hasMore) {
        // Guardar el resto del archivo (ya posicionado tras la línea leída) en un temporal
        File rest = SPIFFS.open(BACKLOG_REST_FILE, FILE_WRITE);
        if (rest) {
            if (len > 0) { // La última línea leída (no enviada) debe conservarse
                rest.println(lineBuffer);
            }
            while (file.available()) {
                rest.write(file.read());
            }
            rest.close();
        }
        file.close();

        SPIFFS.remove(BACKLOG_PROC_FILE);
        SPIFFS.rename(BACKLOG_REST_FILE, BACKLOG_PROC_FILE);
        hasMore = false;

        Serial.printf("⚠️ Lote limitado a %d registros. Continuará en la próxima llamada.\n", MAX_BATCH);
    } else {
        file.close();

        // Todos los registros fueron enviados: eliminar el archivo de procesamiento
        SPIFFS.remove(BACKLOG_PROC_FILE);
        Serial.println("✅ Backlog sincronizado y vaciado.");
    }
}