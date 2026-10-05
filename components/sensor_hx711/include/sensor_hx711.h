/**
 * @file sensor_hx711.h
 * @brief Driver para la celda de carga de café con módulo conversor ADC de 24 bits HX711.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 *
 * @note Utiliza pines seguros (DT: GPIO 34 de entrada pura, SCK: GPIO 2 de salida).
 *       Implementa filtrado digital paso bajo y tara automática en FreeRTOS.
 */

#ifndef SENSOR_HX711_H
#define SENSOR_HX711_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa los pines GPIO, sondea el transmisor HX711 y lanza la tarea de pesaje FreeRTOS.
 * @return ESP_OK si la configuración hardware fue correcta.
 */
esp_err_t sensor_hx711_init(void);

/**
 * @brief Ejecuta el proceso de tara (puesta a cero) promediando varias muestras.
 * @param samples Número de muestras a promediar (típicamente 10).
 * @return ESP_OK si la tara se completó satisfactoriamente.
 */
esp_err_t sensor_hx711_tare(uint8_t samples);

/**
 * @brief Lee el peso bruto promediado en gramos.
 * @param samples Cantidad de lecturas directas del ADC.
 * @param[out] out_weight_g Puntero donde se almacena el peso calculado.
 * @return ESP_OK si la lectura fue exitosa.
 */
esp_err_t sensor_hx711_read_weight(uint8_t samples, float *out_weight_g);

/**
 * @brief Obtiene el peso filtrado en tiempo real actualizado por la tarea en segundo plano.
 * @return Peso actual en gramos (g).
 */
float sensor_hx711_get_weight(void);

/**
 * @brief Retorna si el módulo HX711 fue detectado y se encuentra en línea.
 */
bool sensor_hx711_is_available(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_HX711_H
