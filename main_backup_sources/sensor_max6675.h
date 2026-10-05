/**
 * @file sensor_max6675.h
 * @brief Driver SPI para el convertidor analógico-digital y termocupla tipo K MAX6675.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#ifndef SENSOR_MAX6675_H
#define SENSOR_MAX6675_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registra e inicializa el dispositivo MAX6675 en el bus SPI compartido.
 * @return ESP_OK si el registro en el bus fue exitoso.
 */
esp_err_t sensor_max6675_init(void);

/**
 * @brief Lee la trama SPI de 16 bits del MAX6675 y calcula la temperatura real.
 * @param[out] out_temp Puntero para retornar la temperatura en grados Celsius (°C).
 * @param[out] out_connected Puntero para retornar si la termocupla está conectada físicamente (bit 2 en 0).
 * @return ESP_OK si la transacción SPI fue exitosa.
 */
esp_err_t sensor_max6675_read(float *out_temp, bool *out_connected);

/**
 * @brief Retorna si el driver MAX6675 fue inicializado correctamente.
 */
bool sensor_max6675_is_available(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_MAX6675_H
