#ifndef LUX_CONTROL_H
#define LUX_CONTROL_H

#include <stdbool.h>

typedef enum {
    MODO_AUTO = 0,
    MODO_MANUAL
} modo_t;

// Configura GPIOs, ADC y calibracion. Llamar una sola vez en app_main().
void lux_control_init(void);

// Crea la tarea de FreeRTOS que lee el sensor, controla LEDs/relevo
// y revisa el boton de cambio de modo. Llamar una sola vez despues
// de lux_control_init().
void lux_control_start_task(void);

// Getters seguros (usan mutex internamente) para que otros modulos
// (ej. el servidor web) puedan leer el estado actual sin condiciones de carrera.
modo_t lux_control_get_modo(void);
float  lux_control_get_lux(void);
bool   lux_control_hay_luz_suficiente(void);

#endif // LUX_CONTROL_H
