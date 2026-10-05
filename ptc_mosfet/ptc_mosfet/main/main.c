/*
 * PRUEBA 8: Resistencia PTC controlada por MOSFET low-side (30A+)
 * -----------------------------------------------------------------
 * Un módulo MOSFET "low-side" conmuta el lado de GND del circuito de
 * potencia: la PTC va entre +12V y el DRAIN del MOSFET, y el SOURCE
 * del MOSFET va al GND de la fuente de 12V. El ESP32 solo maneja la
 * señal de la compuerta (GATE) a través del pin de señal del módulo.
 *
 * CONEXIÓN:
 *   - Pin de señal (SIG/PWM) del módulo MOSFET -> GPIO 32 del ESP32
 *   - GND del módulo MOSFET  -> GND del ESP32 (referencia común obligatoria)
 *   - + de la PTC -> +12V de la fuente principal
 *   - - de la PTC -> DRAIN del MOSFET
 *   - SOURCE del MOSFET -> GND de la fuente de 12V
 *
 * IMPORTANTE (seguridad eléctrica/térmica):
 *   - Nunca dejes la PTC energizada sin supervisión durante las pruebas.
 *   - Empieza siempre con duty bajo y ve subiendo.
 *   - Verifica que el disipador del MOSFET esté puesto si vas a sostener
 *     corriente alta por tiempos largos.
 *   - Si tu módulo es de un solo canal on/off (relé/SSR) en vez de MOSFET
 *     con PWM real, usa solo duty = 0 o duty = 255 (encendido/apagado),
 *     no valores intermedios.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "PTC_MOSFET";

#define PIN_PTC         GPIO_NUM_32
#define LEDC_TIMER      LEDC_TIMER_3
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_4
#define LEDC_DUTY_RES   LEDC_TIMER_8_BIT   // valores de 0 a 255
#define LEDC_FREQUENCY  1000                // 1 kHz, suficiente para resistivo

// Límite de duty para las pruebas iniciales (evita 100% de entrada)
#define DUTY_MAXIMO_PRUEBA 180  // ~70% de potencia máxima

void app_main(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz         = LEDC_FREQUENCY,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = PIN_PTC,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ledc_channel);

    ESP_LOGI(TAG, "=== Prueba PTC (MOSFET low-side) ===");
    ESP_LOGI(TAG, "Duty maximo de prueba limitado a %d/255", DUTY_MAXIMO_PRUEBA);

    while (1) {
        ESP_LOGI(TAG, "Subiendo potencia poco a poco...");
        for (int duty = 0; duty <= DUTY_MAXIMO_PRUEBA; duty += 20) {
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            ESP_LOGI(TAG, "Potencia PTC: %d %%", (duty * 100) / 255);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        ESP_LOGI(TAG, "Manteniendo potencia %d segundos...", 10);
        vTaskDelay(pdMS_TO_TICKS(10000));

        ESP_LOGI(TAG, "Apagando PTC...");
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
