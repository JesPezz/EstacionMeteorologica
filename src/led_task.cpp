#include "led_task.h"

QueueHandle_t ledQueue;

void ledTask(void *parameter) {
    LedState currentState;
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    for (;;) {
        // Espera un comando de la cola (bloquea esta tarea, no el sistema)
        if (xQueueReceive(ledQueue, &currentState, portMAX_DELAY)) {
            switch (currentState) {
                case LED_SUCCESS:
                    digitalWrite(LED_BUILTIN, HIGH);
                    vTaskDelay(1000 / portTICK_PERIOD_MS); // Encendido continuo durante 1000 ms
                    digitalWrite(LED_BUILTIN, LOW);
                    break;

                case LED_PROGRESS:
                    for (int i = 0; i < 5; i++) {
                        digitalWrite(LED_BUILTIN, HIGH);
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        digitalWrite(LED_BUILTIN, LOW);
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                    }
                    break;

                case LED_ERROR:
                    for (int i = 0; i < 3; i++) {
                        digitalWrite(LED_BUILTIN, HIGH);
                        vTaskDelay(400 / portTICK_PERIOD_MS);
                        digitalWrite(LED_BUILTIN, LOW);
                        vTaskDelay(400 / portTICK_PERIOD_MS);
                    }
                    break;
                
                default:
                    digitalWrite(LED_BUILTIN, LOW);
                    break;
            }
        }
    }
}

void setupLedTask() {
    ledQueue = xQueueCreate(5, sizeof(LedState)); // Cola de 5 comandos
    xTaskCreate(ledTask, "LedTask", 2048, NULL, 1, NULL);
}

void signalLed(LedState state) {
    if (ledQueue != NULL) {
        xQueueSend(ledQueue, &state, 0);
    }
}