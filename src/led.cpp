#include "led.h"

// Estados del LED
void ledSuccess() {
    pinMode(LED_BUILTIN, OUTPUT);  // Configurar el pin del LED como salida
    digitalWrite(LED_BUILTIN, HIGH);  // LED encendido fijo
    delay(3000);                     // Mantener encendido 3 segundos
    digitalWrite(LED_BUILTIN, LOW);  // Apagar el LED
}

void ledInProgress() {
    pinMode(LED_BUILTIN, OUTPUT);  // Configurar el pin del LED como salida
    for (int i = 0; i < 5; i++) {  // Parpadear 5 veces rápidamente
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
    }
}

void errLeds() {
    pinMode(LED_BUILTIN, OUTPUT);  // Configurar el pin del LED como salida
    for (int i = 0; i < 3; i++) {  // Parpadear 3 veces lentamente
        digitalWrite(LED_BUILTIN, HIGH);
        delay(1000);
        digitalWrite(LED_BUILTIN, LOW);
        delay(1000);
    }
}