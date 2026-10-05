/**
 * @file sensor_hx711.c
 * @brief Implementación del driver de pesaje con ADC HX711 y tarea FreeRTOS.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include "sensor_hx711.h"
#include "config_pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "HX711";

static bool s_hx711_available = false;
static float s_cal_factor     = HX711_CAL_FACTOR_DEFAULT;
static int32_t s_tare_offset  = 0;
static float s_filtered_weight_g = 0.0f;
static portMUX_TYPE s_hx711_mux = portMUX_INITIALIZER_UNLOCKED;

static inline bool hx711_is_ready(void)
{
    return (gpio_get_level(PIN_HX711_DT) == 0);
}

static esp_err_t hx711_read_raw(int32_t *out_raw)
{
    if (out_raw == NULL) return ESP_ERR_INVALID_ARG;

    // Espera no bloqueante hasta que DT baje a nivel bajo (datos listos)
    uint32_t elapsed = 0;
    while (gpio_get_level(PIN_HX711_DT) != 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
        if (elapsed >= 150) {
            return ESP_ERR_TIMEOUT;
        }
    }

    int32_t value = 0;

    portENTER_CRITICAL(&s_hx711_mux);
    for (int i = 0; i < 24; i++) {
        gpio_set_level(PIN_HX711_SCK, 1);
        esp_rom_delay_us(1);
        value = (value << 1) | gpio_get_level(PIN_HX711_DT);
        gpio_set_level(PIN_HX711_SCK, 0);
        esp_rom_delay_us(1);
    }

    // Pulso 25: Ajusta ganancia de 128 (Canal A) para la siguiente conversión
    gpio_set_level(PIN_HX711_SCK, 1);
    esp_rom_delay_us(1);
    gpio_set_level(PIN_HX711_SCK, 0);
    esp_rom_delay_us(1);
    portEXIT_CRITICAL(&s_hx711_mux);

    // Extensión de signo de 24 a 32 bits en complemento a 2
    if (value & 0x800000) {
        value |= 0xFF000000;
    }

    *out_raw = value;
    return ESP_OK;
}

static esp_err_t hx711_read_average(uint8_t samples, int32_t *out_avg)
{
    if (samples == 0 || out_avg == NULL) return ESP_ERR_INVALID_ARG;

    int64_t sum = 0;
    uint8_t valid_samples = 0;

    for (uint8_t i = 0; i < samples; i++) {
        int32_t raw = 0;
        if (hx711_read_raw(&raw) == ESP_OK) {
            sum += raw;
            valid_samples++;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (valid_samples == 0) return ESP_ERR_TIMEOUT;
    *out_avg = (int32_t)(sum / valid_samples);
    return ESP_OK;
}

esp_err_t sensor_hx711_tare(uint8_t samples)
{
    if (samples < 1) samples = 10;
    int32_t avg = 0;
    esp_err_t ret = hx711_read_average(samples, &avg);
    if (ret == ESP_OK) {
        s_tare_offset = avg;
        s_filtered_weight_g = 0.0f;
        ESP_LOGI(TAG, "Tara calibrada exitosamente. Offset bruto: %ld cuentas", (long)s_tare_offset);
    }
    return ret;
}

esp_err_t sensor_hx711_read_weight(uint8_t samples, float *out_weight_g)
{
    if (out_weight_g == NULL) return ESP_ERR_INVALID_ARG;

    int32_t raw_avg = 0;
    esp_err_t ret = hx711_read_average(samples, &raw_avg);
    if (ret != ESP_OK) return ret;

    int32_t net = raw_avg - s_tare_offset;
    float weight = (float)net / s_cal_factor;

    // Eliminar pequeñas fluctuaciones negativas residuales
    if (weight < 0.0f && weight > -0.5f) {
        weight = 0.0f;
    }

    *out_weight_g = weight;
    return ESP_OK;
}

float sensor_hx711_get_weight(void)
{
    return s_filtered_weight_g;
}

bool sensor_hx711_is_available(void)
{
    return s_hx711_available;
}

static void hx711_freertos_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Tarea FreeRTOS de pesaje continuo HX711 iniciada (~10 Hz)...");
    while (1) {
        if (s_hx711_available) {
            float w = 0.0f;
            if (sensor_hx711_read_weight(1, &w) == ESP_OK) {
                // Filtro paso bajo de primer orden para suavizado térmico y vibratorio
                s_filtered_weight_g = (s_filtered_weight_g * 0.7f) + (w * 0.3f);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 100 ms (~10 lecturas/segundo)
    }
}

esp_err_t sensor_hx711_init(void)
{
    ESP_LOGI(TAG, "Configurando pines HX711 (DT: GPIO %d, SCK: GPIO %d)...",
             PIN_HX711_DT, PIN_HX711_SCK);

    // GPIO 34: Pin de solo entrada (GPI)
    gpio_config_t conf_dt = {
        .pin_bit_mask = (1ULL << PIN_HX711_DT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&conf_dt);
    if (ret != ESP_OK) return ret;

    // GPIO 2: Pin de salida de reloj
    gpio_config_t conf_sck = {
        .pin_bit_mask = (1ULL << PIN_HX711_SCK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&conf_sck);
    if (ret != ESP_OK) return ret;

    gpio_set_level(PIN_HX711_SCK, 0);

    // Sondeo de presencia inicial
    uint32_t wait_ms = 0;
    while (gpio_get_level(PIN_HX711_DT) != 0 && wait_ms < 200) {
        vTaskDelay(pdMS_TO_TICKS(10));
        wait_ms += 10;
    }

    if (gpio_get_level(PIN_HX711_DT) == 0) {
        s_hx711_available = true;
        ESP_LOGI(TAG, ">>> CELDA DE CARGA HX711 DETECTADA EXITOSAMENTE <<<");
        sensor_hx711_tare(10);
    } else {
        s_hx711_available = false;
        ESP_LOGW(TAG, "[HX711] Módulo no detectado (DT en alto o no conectado).");
    }

    // Crear tarea FreeRTOS dedicada al pesaje
    xTaskCreate(
        hx711_freertos_task,
        "hx711_task",
        4096,
        NULL,
        4,
        NULL
    );

    return ESP_OK;
}
