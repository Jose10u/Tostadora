#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"

static const char *TAG = "SERVO_TEST";

// Configuración de Hardware
#define SERVO_GPIO_PIN             18
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000 // 1 MHz -> 1 tick = 1 us
#define SERVO_TIMEBASE_PERIOD        20000   // 20000 ticks = 20 ms (50 Hz)

// Rangos de ancho de pulso en microsegundos (500us a 2500us corresponden a 0° - 180°)
#define SERVO_MIN_PULSEWIDTH_US      500
#define SERVO_MAX_PULSEWIDTH_US      2500

/**
 * @brief Convierte un ángulo de 0 a 180 grados al ancho de pulso en microsegundos.
 */
static inline uint32_t angle_to_compare(int angle)
{
    return (angle * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / 180) + SERVO_MIN_PULSEWIDTH_US;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando MCPWM para control de servo en GPIO %d...", SERVO_GPIO_PIN);

    // 1. Configurar base de tiempo del MCPWM
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // 2. Crear operador
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0,
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    // 3. Crear comparador
    mcpwm_cmpr_handle_t comparator = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    // 4. Crear generador asociado al GPIO
    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = SERVO_GPIO_PIN,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    // Configurar acciones del generador
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    // 5. Habilitar e iniciar el timer
    ESP_LOGI(TAG, "Iniciando timer de MCPWM...");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    // Bucle principal de barrido (0° a 180° y retorno)
    int angle = 0;
    int step = 2;

    while (1) {
        // Actualizar el valor de comparación para cambiar el ancho de pulso
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, angle_to_compare(angle)));

        angle += step;
        if (angle <= 0 || angle >= 180) {
            step = -step; // Invertir dirección
        }

        vTaskDelay(pdMS_TO_TICKS(30)); // Pequeña retardo para control de velocidad
    }
}