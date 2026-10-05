/**
 * @file button_controller.c
 * @brief Implementación del filtro antirrebote y eventos de pulsación corta y larga.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include "button_controller.h"
#include "config_pins.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "BOTONES";

static const gpio_num_t s_btn_pins[BTN_INDEX_COUNT] = {
    PIN_BTN_START_STOP,   // GPIO 13
    PIN_BTN_PAUSE_RESUME, // GPIO 33
    PIN_BTN_MODE          // GPIO 16
};

#define DEBOUNCE_STABLE_TICKS   2   // 2 muestras de 20ms = 40ms antirrebote
#define LONG_PRESS_TICKS        75  // 75 muestras de 20ms = 1.5 segundos

typedef struct {
    uint8_t stable_state;
    uint8_t last_raw;
    uint8_t debounce_count;
    uint16_t pressed_ticks;
    bool was_pressed;
    bool was_long_pressed;
    bool long_handled;
} button_state_t;

static button_state_t s_buttons[BTN_INDEX_COUNT];

esp_err_t button_controller_init(void)
{
    ESP_LOGI(TAG, "Configurando botones pulsadores (GPIO %d, GPIO %d, GPIO %d con Pull-Up)...",
             PIN_BTN_START_STOP, PIN_BTN_PAUSE_RESUME, PIN_BTN_MODE);

    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << PIN_BTN_START_STOP) |
                        (1ULL << PIN_BTN_PAUSE_RESUME) |
                        (1ULL << PIN_BTN_MODE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&btn_cfg);
    if (ret != ESP_OK) return ret;

    for (int i = 0; i < BTN_INDEX_COUNT; i++) {
        s_buttons[i].stable_state      = 1; // Nivel alto en reposo (Pull-up)
        s_buttons[i].last_raw          = 1;
        s_buttons[i].debounce_count    = 0;
        s_buttons[i].pressed_ticks     = 0;
        s_buttons[i].was_pressed       = false;
        s_buttons[i].was_long_pressed  = false;
        s_buttons[i].long_handled      = false;
    }

    return ESP_OK;
}

void button_controller_poll(void)
{
    for (int i = 0; i < BTN_INDEX_COUNT; i++) {
        uint8_t raw = gpio_get_level(s_btn_pins[i]);

        if (raw == s_buttons[i].last_raw) {
            if (s_buttons[i].debounce_count < DEBOUNCE_STABLE_TICKS) {
                s_buttons[i].debounce_count++;
                if (s_buttons[i].debounce_count >= DEBOUNCE_STABLE_TICKS) {
                    if (s_buttons[i].stable_state != raw) {
                        s_buttons[i].stable_state = raw;
                        if (raw == 0) {
                            // Flanco de bajada (presionado)
                            s_buttons[i].pressed_ticks = 0;
                            s_buttons[i].long_handled = false;
                        } else {
                            // Flanco de subida (soltado)
                            if (!s_buttons[i].long_handled) {
                                s_buttons[i].was_pressed = true;
                            }
                        }
                    }
                }
            }
        } else {
            s_buttons[i].last_raw = raw;
            s_buttons[i].debounce_count = 0;
        }

        // Conteo sostenido para detección de pulsación larga
        if (s_buttons[i].stable_state == 0) {
            s_buttons[i].pressed_ticks++;
            if (s_buttons[i].pressed_ticks >= LONG_PRESS_TICKS && !s_buttons[i].long_handled) {
                s_buttons[i].was_long_pressed = true;
                s_buttons[i].long_handled = true;
            }
        }
    }
}

bool button_check_pressed(button_index_t btn)
{
    if (btn >= BTN_INDEX_COUNT) return false;
    if (s_buttons[btn].was_pressed) {
        s_buttons[btn].was_pressed = false;
        return true;
    }
    return false;
}

bool button_check_long_pressed(button_index_t btn)
{
    if (btn >= BTN_INDEX_COUNT) return false;
    if (s_buttons[btn].was_long_pressed) {
        s_buttons[btn].was_long_pressed = false;
        return true;
    }
    return false;
}
