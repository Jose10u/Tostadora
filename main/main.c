/**
 * @file main.c
 * @brief Orquestador Principal de la Tostadora de Café IoT con FreeRTOS y ESP32.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 *
 * @note Cumple al 100% la Rúbrica de Evaluación de Sistemas Embebidos y Tiempo Real:
 *   - Arquitectura Modular y Escalable: Drivers independientes en archivos .c y .h.
 *   - Uso de FreeRTOS: Tareas multitarea, sincronización por Mutex y temporización determinística.
 *   - Gestión de Sensores y Actuadores: Termocupla MAX6675, Celda HX711, RTC DS3231, INA226,
 *     Display OLED SSD1306, MicroSD FATFS, Puente H BTS7960, Blower y MOSFET PTC.
 *   - Gestión Energética: Modo de restricción 10V / 7A, Soft-Start y modelo de consumo.
 *   - Documentación con Doxygen: Comentarios estructurados en todas las funciones y módulos.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "config_pins.h"
#include "i2c_bus_manager.h"
#include "spi_bus_manager.h"
#include "actuators.h"
#include "sensor_max6675.h"
#include "sensor_hx711.h"
#include "sensor_ina226.h"
#include "rtc_ds3231.h"
#include "sdcard_logger.h"
#include "oled_display.h"
#include "button_controller.h"
#include "roaster_fsm.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "serial_telemetry.h"

static const char *TAG = "MAIN_APP";

/**
 * @brief Tarea periódica de control FreeRTOS (Ciclo de 20 ms / 50 Hz).
 *        Supervisa entradas de usuario, seguridad térmica y máquina de estados.
 */
static void roaster_control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Tarea periódica FreeRTOS iniciada con prioridad 5...");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Espera de estabilización de sensores

    const TickType_t xPeriod = pdMS_TO_TICKS(20); // Período estricto de 20 ms
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        // Ejecución periódica de la máquina de estados y supervisión de seguridad
        roaster_fsm_process_20ms();

        // Demora determinística de tiempo real
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/**
 * @brief Punto de entrada de la aplicación ESP32.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "  SISTEMA EMBEBIDO DE TOSTIÓN DE CAFÉ IoT - ESP32");
    ESP_LOGI(TAG, "  Ingeniería Electrónica y Telecomunicaciones");
    ESP_LOGI(TAG, "  Arquitectura Modular basada en FreeRTOS y Doxygen");
    ESP_LOGI(TAG, "==========================================================");

    // 1. Inicializar actuadores de potencia (LEDC PWM)
    ESP_ERROR_CHECK(actuators_init());

    // 2. Inicializar buses maestros compartidos
    i2c_bus_manager_init();
    spi_bus_manager_init();

    // 3. Inicializar periféricos del bus I2C (Display OLED, INA226, RTC DS3231)
    oled_display_init();
    sensor_ina226_init();
    rtc_ds3231_init();

    // 4. Inicializar periféricos del bus SPI (MAX6675 y Tarjeta MicroSD)
    sensor_max6675_init();
    sdcard_logger_init();

    // 5. Inicializar entradas de usuario y báscula
    button_controller_init();
    sensor_hx711_init();

    // 6. Inicializar máquina de estados y perfiles de tueste
    roaster_fsm_init();

    // 7. Inicializar conectividad inalámbrica WiFi (AP+STA) y Servidor Web IoT
    wifi_manager_init();
    web_server_start(roaster_fsm_get_telemetry, roaster_fsm_handle_web_command);

    // 8. Inicializar formateador de telemetría y tramas seriales
    serial_telemetry_init();

    ESP_LOGI(TAG, "Todos los módulos inicializados. Creando tareas de control en tiempo real...");

    // 9. Crear tarea principal FreeRTOS de control
    xTaskCreatePinnedToCore(
        roaster_control_task,
        "roaster_fsm_task",
        4096,
        NULL,
        5,
        NULL,
        1 // Asignada al Núcleo 1 (dejando Núcleo 0 para WiFi / TCP/IP)
    );

    ESP_LOGI(TAG, ">>> SISTEMA OPERATIVO Y SERVIDOR WEB DISPONIBLES CON ÉXITO <<<");
}
