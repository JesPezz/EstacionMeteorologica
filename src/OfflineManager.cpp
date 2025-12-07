#include "OfflineManager.h"
#include "config.h"       // Para acceder al nombre del tópico y configuración
#include "SPIFFS.h"
#include "MQTTManager.h"  // Para usar mqttClient.publish()

const char* BACKLOG_FILE = "/backlog.txt";

void saveToBacklog(JsonDocument& doc) {
    // 1. Abrir archivo en modo "Append" (añadir al final)
    File file = SPIFFS.open(BACKLOG_FILE, FILE_APPEND);
    if (!file) {
        Serial.println("❌ Error crítico: No se pudo abrir/crear el archivo de backlog");
        return;
    }

    // 2. Marcar el dato como "offline" para que sepas en Node-RED/Excel que fue recuperado
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
    // Si no existe el archivo, no hacemos nada
    if (!SPIFFS.exists(BACKLOG_FILE)) return;

    // Solo procesar si hay conexión MQTT real
    if (!mqttClient.connected()) return;

    Serial.println("📂 Encontrado archivo de respaldo. Iniciando sincronización...");

    // 1. Abrimos el archivo para lectura
    File file = SPIFFS.open(BACKLOG_FILE, FILE_READ);
    if (!file) return;

    // Renombramos el archivo original para que no se sigan acumulando datos mientras leemos
    // (Una técnica de seguridad simple: procesamos el 'old', y si llegan nuevos se crea uno nuevo)
    // Pero por simplicidad, leeremos todo y luego borraremos.

    while (file.available()) {
        // Leer línea por línea
        String jsonLine = file.readStringUntil('\n');
        jsonLine.trim(); // Quitar espacios extra

        if (jsonLine.length() > 0) {
            // Enviamos tal cual a MQTT
            // Usamos QoS 1 para asegurar que llegue
            uint16_t packetId = mqttClient.publish(config.mqttTopic.c_str(), 1, false, jsonLine.c_str());
            
            Serial.print("📤 Reenviando dato histórico: ");
            // Imprimimos solo los primeros 50 caracteres para no saturar el monitor
            Serial.println(jsonLine.substring(0, 50) + "..."); 
            
            // Pequeña pausa para no saturar el buffer de salida del ESP32
            delay(500); 
            yield();
        }
    }
    
    file.close();
    
    // Una vez enviado todo, borramos el archivo para no duplicar
    SPIFFS.remove(BACKLOG_FILE);
    Serial.println("✅ Backlog sincronizado y vaciado.");
}