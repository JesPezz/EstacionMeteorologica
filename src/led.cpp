#include "led.h"

// Estados del LED
void ledSuccess() {
    pinMode(LED_BUILTIN, OUTPUT);  // Configurar el pin del LED como salida
    digitalWrite(LED_BUILTIN, HIGH);  // LED encendido fijo
    delay(1000);                     // Mantener encendido 1 segundo (más rápido)
    digitalWrite(LED_BUILTIN, LOW);  // Apagar el LED
}

void ledInProgress() {
    pinMode(LED_BUILTIN, OUTPUT);  // Configurar el pin del LED como salida
    for (int i = 0; i < 5; i++) {  // Parpadear 5 veces rápidamente
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
    }
}

void errLeds() {
    pinMode(LED_BUILTIN, OUTPUT);  // Configurar el pin del LED como salida
    for (int i = 0; i < 3; i++) {  // Parpadear 3 veces lentamente
        digitalWrite(LED_BUILTIN, HIGH);
        delay(400);
        digitalWrite(LED_BUILTIN, LOW);
        delay(400);
    }
}