/**
 * @file button_controller.h
 * @brief Controlador y antirrebote para botones pulsadores físicos.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#ifndef BUTTON_CONTROLLER_H
#define BUTTON_CONTROLLER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BTN_INDEX_START_STOP   = 0, ///< Botón 1 (GPIO 13): Iniciar / Detener
    BTN_INDEX_PAUSE_RESUME = 1, ///< Botón 2 (GPIO 33): Pausar / Continuar
    BTN_INDEX_MODE_TARE    = 2, ///< Botón 3 (GPIO 16): Modo (Click) / Tara (Mantener 1.5s)
    BTN_INDEX_COUNT        = 3
} button_index_t;

/**
 * @brief Configura las entradas GPIO de los botones con resistencias Pull-Up internas.
 */
esp_err_t button_controller_init(void);

/**
 * @brief Sondea los niveles de los botones con filtro antirrebote de tiempo real (llamar cada 20 ms).
 */
void button_controller_poll(void);

/**
 * @brief Verifica si hubo una pulsación corta del botón (se auto-consume).
 */
bool button_check_pressed(button_index_t btn);

/**
 * @brief Verifica si hubo una pulsación sostenida (>= 1.5 segundos, se auto-consume).
 */
bool button_check_long_pressed(button_index_t btn);

#ifdef __cplusplus
}
#endif

#endif // BUTTON_CONTROLLER_H
