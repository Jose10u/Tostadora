/*
 * lux_control.c
 *
 * Encapsula toda la logica que ya tenias funcionando:
 *  - Lectura del sensor LDR (ADC) y calculo de lux
 *  - Control de LEDs (verde/rojo) y relevo segun nivel de luz
 *  - Lectura del boton para alternar AUTO / MANUAL
 *
 * A diferencia del original, el estado (modo, lux, si hay luz
 * suficiente) se guarda en una estructura protegida con un mutex,
 * para que el servidor web pueda leerlo desde otra tarea sin
 * condiciones de carrera.
 */

#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "lux_control.h"

static const char *TAG = "LUX_CONTROL";

/* ---------- Pines (igual que tu codigo original) ---------- */
#define BUTTON_GPIO         GPIO_NUM_4
#define LED_G_GPIO          GPIO_NUM_18
#define LED_R_GPIO          GPIO_NUM_19
#define RELAY_GPIO          GPIO_NUM_21
#define DEBOUNCE_MS         200

/* ESP32 clasico: ADC1_CHAN6 = GPIO34 (LDR), ADC1_CHAN7 = GPIO35 (potenciometro) */
#define ADC1_CHAN_LDR       ADC_CHANNEL_6
#define ADC1_CHAN_POT       ADC_CHANNEL_7
#define ADC_ATTEN           ADC_ATTEN_DB_12

#define V_REF               3.3f
#define R_FIXED             10000.0f
#define UMBRAL_LUZ_TRABAJO  300.0f   // Lux minimos considerados "mucha luz"

/* ---------- Estado compartido protegido por mutex ---------- */
typedef struct {
    modo_t modo;
    float  lux;
    bool   hay_luz_suficiente;
} estado_t;

static estado_t s_estado = {
    .modo = MODO_AUTO,
    .lux = 0.0f,
    .hay_luz_suficiente = false,
};

static SemaphoreHandle_t s_mutex = NULL;

/* ---------- ADC ---------- */
static adc_oneshot_unit_handle_t s_adc1_handle;
static adc_cali_handle_t s_cali_ldr = NULL;
static adc_cali_handle_t s_cali_pot = NULL;
static bool s_cali_ldr_ok = false;
static bool s_cali_pot_ok = false;

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel,
                                  adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cfg = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cfg, &handle);
        calibrated = (ret == ESP_OK);
    }
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_line_fitting_config_t cfg = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cfg, &handle);
        calibrated = (ret == ESP_OK);
    }
#endif
    *out_handle = handle;
    return calibrated;
}

static float mv_to_lux(int mv)
{
    if (mv <= 0) return 0.0f;
    float v_out = (float)mv / 1000.0f;
    if (v_out >= V_REF) v_out = V_REF - 0.001f;
    float r_ldr = R_FIXED * ((V_REF - v_out) / v_out);
    return 500.0f * powf(10000.0f / r_ldr, 1.4f);
}

/* ---------- GPIO ---------- */
static void gpio_setup(void)
{
    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_conf);

    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << LED_G_GPIO) | (1ULL << LED_R_GPIO) | (1ULL << RELAY_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_conf);

    gpio_set_level(LED_G_GPIO, 0);
    gpio_set_level(LED_R_GPIO, 0);
    gpio_set_level(RELAY_GPIO, 0);
}

static void revisar_boton(void)
{
    static bool anterior = true;
    static int64_t ultimo_cambio_ms = 0;

    bool actual = gpio_get_level(BUTTON_GPIO);
    int64_t ahora_ms = esp_timer_get_time() / 1000;

    if (anterior == true && actual == false && (ahora_ms - ultimo_cambio_ms) > DEBOUNCE_MS) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_estado.modo = (s_estado.modo == MODO_AUTO) ? MODO_MANUAL : MODO_AUTO;
        modo_t nuevo = s_estado.modo;
        xSemaphoreGive(s_mutex);

        ultimo_cambio_ms = ahora_ms;
        ESP_LOGI(TAG, ">>> CAMBIO DE MODO: %s <<<", nuevo == MODO_AUTO ? "AUTOMATICO" : "MANUAL");
    }
    anterior = actual;
}

/* ---------- Tarea principal (reemplaza tu while(1) de app_main) ---------- */
static void lux_control_task(void *arg)
{
    int adc_raw;
    int mv;

    while (1) {
        revisar_boton();

        modo_t modo_local;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        modo_local = s_estado.modo;
        xSemaphoreGive(s_mutex);

        if (modo_local == MODO_AUTO) {
            if (adc_oneshot_read(s_adc1_handle, ADC1_CHAN_LDR, &adc_raw) == ESP_OK && s_cali_ldr_ok) {
                if (adc_cali_raw_to_voltage(s_cali_ldr, adc_raw, &mv) == ESP_OK) {
                    float lux = mv_to_lux(mv);
                    bool ok_luz = (lux >= UMBRAL_LUZ_TRABAJO);

                    gpio_set_level(LED_G_GPIO, ok_luz ? 1 : 0);
                    gpio_set_level(LED_R_GPIO, ok_luz ? 0 : 1);
                    gpio_set_level(RELAY_GPIO, ok_luz ? 0 : 1);

                    xSemaphoreTake(s_mutex, portMAX_DELAY);
                    s_estado.lux = lux;
                    s_estado.hay_luz_suficiente = ok_luz;
                    xSemaphoreGive(s_mutex);

                    //ESP_LOGI(TAG, "[AUTO] Lux: %.2f -> %s", lux, ok_luz ? "Mucha luz" : "Poca luz");
                }
            }
        } else {
    // MODO_MANUAL: controlar LEDs y relevo con el potenciometro
    if (adc_oneshot_read(s_adc1_handle, ADC1_CHAN_POT, &adc_raw) == ESP_OK && s_cali_pot_ok) {
        if (adc_cali_raw_to_voltage(s_cali_pot, adc_raw, &mv) == ESP_OK) {
            float volts = (float)mv / 1000.0f;
            bool ok_luz = (volts > 2.5f);

            gpio_set_level(LED_G_GPIO, ok_luz ? 1 : 0);
            gpio_set_level(LED_R_GPIO, ok_luz ? 0 : 1);
            gpio_set_level(RELAY_GPIO, ok_luz ? 0 : 1);   // <-- ahora sigue el mismo criterio que en AUTO

            xSemaphoreTake(s_mutex, portMAX_DELAY);
            s_estado.lux = volts * 100.0f;
            s_estado.hay_luz_suficiente = ok_luz;
            xSemaphoreGive(s_mutex);
        }
    }
}

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ---------- API publica ---------- */
void lux_control_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    gpio_setup();

    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, ADC1_CHAN_LDR, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, ADC1_CHAN_POT, &chan_cfg));

    s_cali_ldr_ok = adc_calibration_init(ADC_UNIT_1, ADC1_CHAN_LDR, ADC_ATTEN, &s_cali_ldr);
    s_cali_pot_ok = adc_calibration_init(ADC_UNIT_1, ADC1_CHAN_POT, ADC_ATTEN, &s_cali_pot);

    ESP_LOGI(TAG, "lux_control inicializado. Modo por defecto: AUTO");
}

void lux_control_start_task(void)
{
    xTaskCreate(lux_control_task, "lux_control_task", 4096, NULL, 5, NULL);
}

modo_t lux_control_get_modo(void)
{
    modo_t m;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    m = s_estado.modo;
    xSemaphoreGive(s_mutex);
    return m;
}

float lux_control_get_lux(void)
{
    float l;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    l = s_estado.lux;
    xSemaphoreGive(s_mutex);
    return l;
}

bool lux_control_hay_luz_suficiente(void)
{
    bool b;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    b = s_estado.hay_luz_suficiente;
    xSemaphoreGive(s_mutex);
    return b;
}
