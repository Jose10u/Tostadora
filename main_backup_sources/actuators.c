/**
 * @file actuators.c
 * @brief Implementación del driver modular de actuadores con Kick-Start y Soft-Start.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include "actuators.h"
#include "config_pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "ACTUADORES";

static uint8_t s_blower_duty = 0;
static uint8_t s_motor_duty  = 0;
static uint8_t s_ptc_duty    = 0;

esp_err_t actuators_init(void)
{
    ESP_LOGI(TAG, "Configurando hardware de potencia (Modo Restricción 10V / 7A)...");

    // 1. Pines de habilitación del BTS7960 en nivel alto
    gpio_config_t en_cfg = {
        .pin_bit_mask = (1ULL << PIN_MOTOR_R_EN) | (1ULL << PIN_MOTOR_L_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&en_cfg);
    gpio_set_level(PIN_MOTOR_R_EN, 1);
    gpio_set_level(PIN_MOTOR_L_EN, 1);

    // 2. Temporizador y canal para Blower (1 kHz, 8 bits)
    ledc_timer_config_t timer_blower_cfg = {
        .speed_mode       = PWM_MODE,
        .timer_num        = TIMER_BLOWER,
        .duty_resolution  = PWM_DUTY_RES,
        .freq_hz          = FREQ_BLOWER_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_blower_cfg);

    ledc_channel_config_t ch_blower_cfg = {
        .speed_mode = PWM_MODE,
        .channel    = CH_BLOWER,
        .timer_sel  = TIMER_BLOWER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = PIN_BLOWER,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&ch_blower_cfg);

    // 3. Temporizador y canales para Motor BTS7960 (1 kHz, 8 bits)
    ledc_timer_config_t timer_motor_cfg = {
        .speed_mode       = PWM_MODE,
        .timer_num        = TIMER_MOTOR,
        .duty_resolution  = PWM_DUTY_RES,
        .freq_hz          = FREQ_MOTOR_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_motor_cfg);

    ledc_channel_config_t ch_rpwm_cfg = {
        .speed_mode = PWM_MODE,
        .channel    = CH_MOTOR_RPWM,
        .timer_sel  = TIMER_MOTOR,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = PIN_MOTOR_RPWM,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&ch_rpwm_cfg);

    ledc_channel_config_t ch_lpwm_cfg = {
        .speed_mode = PWM_MODE,
        .channel    = CH_MOTOR_LPWM,
        .timer_sel  = TIMER_MOTOR,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = PIN_MOTOR_LPWM,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&ch_lpwm_cfg);

    // 4. Temporizador y canal para PTC (1 kHz, 8 bits)
    ledc_timer_config_t timer_ptc_cfg = {
        .speed_mode       = PWM_MODE,
        .timer_num        = TIMER_PTC,
        .duty_resolution  = PWM_DUTY_RES,
        .freq_hz          = FREQ_PTC_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_ptc_cfg);

    ledc_channel_config_t ch_ptc_cfg = {
        .speed_mode = PWM_MODE,
        .channel    = CH_PTC,
        .timer_sel  = TIMER_PTC,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = PIN_PTC,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&ch_ptc_cfg);

    all_actuators_stop();
    return ESP_OK;
}

void blower_set_power(uint8_t duty)
{
    if (duty > DUTY_LIMIT_10V) {
        duty = DUTY_LIMIT_10V;
    }

    if (duty > 0 && s_blower_duty == 0) {
        ESP_LOGI(TAG, "[BLOWER] Kick-start aplicado (70%% / 180 duty) para vencer inercia...");
        ledc_set_duty(PWM_MODE, CH_BLOWER, 180);
        ledc_update_duty(PWM_MODE, CH_BLOWER);
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    ledc_set_duty(PWM_MODE, CH_BLOWER, duty);
    ledc_update_duty(PWM_MODE, CH_BLOWER);
    s_blower_duty = duty;

    // Regla de interbloqueo aerodinámico: Si el soplador se apaga, apagar el PTC inmediatamente
    if (duty == 0 && s_ptc_duty > 0) {
        ESP_LOGW(TAG, "¡INTERBLOQUEO AERODINÁMICO! Soplador en 0: Cortando inmediatamente resistencia PTC.");
        ledc_set_duty(PWM_MODE, CH_PTC, 0);
        ledc_update_duty(PWM_MODE, CH_PTC);
        s_ptc_duty = 0;
    }
}

void motor_set_speed(uint8_t duty)
{
    gpio_set_level(PIN_MOTOR_R_EN, 1);
    gpio_set_level(PIN_MOTOR_L_EN, 1);

    if (duty == 0) {
        ledc_set_duty(PWM_MODE, CH_MOTOR_RPWM, 0);
        ledc_update_duty(PWM_MODE, CH_MOTOR_RPWM);

        ledc_set_duty(PWM_MODE, CH_MOTOR_LPWM, 0);
        ledc_update_duty(PWM_MODE, CH_MOTOR_LPWM);

        s_motor_duty = 0;
        return;
    }

    // Rotación antihoraria permanente a par pleno: RPWM = 0, LPWM modulando
    ledc_set_duty(PWM_MODE, CH_MOTOR_RPWM, 0);
    ledc_update_duty(PWM_MODE, CH_MOTOR_RPWM);

    ledc_set_duty(PWM_MODE, CH_MOTOR_LPWM, duty);
    ledc_update_duty(PWM_MODE, CH_MOTOR_LPWM);

    s_motor_duty = duty;
}

bool ptc_set_power(uint8_t target_duty)
{
    if (target_duty > 0) {
        // Validación de interbloqueo: el Blower DEBE estar encendido
        if (s_blower_duty == 0) {
            ESP_LOGE(TAG, "[INTERBLOQUEO] Comando de calentamiento rechazado: El soplador está apagado.");
            ledc_set_duty(PWM_MODE, CH_PTC, 0);
            ledc_update_duty(PWM_MODE, CH_PTC);
            s_ptc_duty = 0;
            return false;
        }

        // Límite de potencia seguro para evitar sobrecorrientes en frío
        if (target_duty > 128) {
            target_duty = 128;
        }

        // Rampa Soft-Start progresiva si arranca desde frío (0%)
        if (s_ptc_duty == 0) {
            ESP_LOGI(TAG, "[PTC] Aplicando Soft-Start térmico progresivo para evitar picos >7A...");
            uint8_t step = target_duty / 4;
            if (step < 10) step = 10;
            for (uint8_t ramp = step; ramp < target_duty; ramp += step) {
                ledc_set_duty(PWM_MODE, CH_PTC, ramp);
                ledc_update_duty(PWM_MODE, CH_PTC);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }

    ledc_set_duty(PWM_MODE, CH_PTC, target_duty);
    ledc_update_duty(PWM_MODE, CH_PTC);
    s_ptc_duty = target_duty;
    return true;
}

void all_actuators_stop(void)
{
    ptc_set_power(0);
    motor_set_speed(0);
    blower_set_power(0);
    ESP_LOGI(TAG, "Todos los actuadores detenidos con éxito.");
}

void actuators_get_duties(uint8_t *blower_duty, uint8_t *motor_duty, uint8_t *ptc_duty)
{
    if (blower_duty) *blower_duty = s_blower_duty;
    if (motor_duty)  *motor_duty  = s_motor_duty;
    if (ptc_duty)    *ptc_duty    = s_ptc_duty;
}
