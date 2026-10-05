/**
 * @file actuators.h
 * @brief Driver modular de actuadores de potencia: Blower BG0903, Motor BTS7960 y Calentador PTC.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 *
 * @note Implementa interbloqueo de seguridad térmico-aerodinámico y rampa Soft-Start.
 */

#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa los canales PWM LEDC y pines de habilitación de los tres actuadores.
 * @return ESP_OK si la configuración hardware fue exitosa.
 */
esp_err_t actuators_init(void);

/**
 * @brief Comanda el soplador centrífugo BG0903 con pulso de arranque (Kick-Start).
 * @param duty Ciclo de trabajo (0..255). Se restringe automáticamente al techo DUTY_LIMIT_10V (212).
 */
void blower_set_power(uint8_t duty);

/**
 * @brief Comanda el motorreductor de tambor BTS7960 en sentido antihorario a potencia plena.
 * @param duty Ciclo de trabajo (0..255).
 */
void motor_set_speed(uint8_t duty);

/**
 * @brief Comanda la resistencia PTC aplicando rampa Soft-Start e interbloqueo con el blower.
 * @param target_duty Ciclo de trabajo objetivo (0..255). Se restringe a un máximo seguro de 128 (50%).
 * @return true si la potencia fue aceptada, false si fue rechazada por interbloqueo (soplador apagado).
 */
bool ptc_set_power(uint8_t target_duty);

/**
 * @brief Detiene inmediatamente todos los actuadores de potencia (PTC, Motor y Blower a 0).
 */
void all_actuators_stop(void);

/**
 * @brief Obtiene los ciclos de trabajo actuales (0..255) de cada actuador.
 * @param[out] blower_duty Puntero a la variable para almacenar el duty del soplador.
 * @param[out] motor_duty Puntero a la variable para almacenar el duty del motor.
 * @param[out] ptc_duty Puntero a la variable para almacenar el duty del calefactor PTC.
 */
void actuators_get_duties(uint8_t *blower_duty, uint8_t *motor_duty, uint8_t *ptc_duty);

#ifdef __cplusplus
}
#endif

#endif // ACTUATORS_H
