/**
 * @file sensor_ina226.h
 * @brief Driver I2C para el monitor de potencia, tensión y corriente INA226.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#ifndef SENSOR_INA226_H
#define SENSOR_INA226_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sondea e inicializa el monitor INA226 en la dirección I2C 0x40 si está conectado.
 * @return ESP_OK si el sensor fue detectado y configurado.
 */
esp_err_t sensor_ina226_init(void);

/**
 * @brief Lee las medidas de tensión de bus, corriente por shunt y potencia calculada.
 * @param[out] out_volts Tensión en voltios (V).
 * @param[out] out_amps Corriente en amperios (A).
 * @param[out] out_watts Potencia en vatios (W).
 * @return ESP_OK si la lectura I2C fue exitosa.
 */
esp_err_t sensor_ina226_read(float *out_volts, float *out_amps, float *out_watts);

/**
 * @brief Retorna si el dispositivo INA226 está disponible en el bus I2C.
 */
bool sensor_ina226_is_available(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_INA226_H
