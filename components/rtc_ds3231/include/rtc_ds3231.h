/**
 * @file rtc_ds3231.h
 * @brief Driver I2C para el reloj en tiempo real de alta precisión RTC DS3231.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#ifndef RTC_DS3231_H
#define RTC_DS3231_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sondea e inicializa el dispositivo RTC DS3231 en la dirección 0x68.
 * @return ESP_OK si el RTC fue detectado en el bus I2C.
 */
esp_err_t rtc_ds3231_init(void);

/**
 * @brief Lee la fecha, hora y temperatura interna del RTC DS3231.
 * @param[out] out_datetime Cadena formateada "YYYY/MM/DD HH:MM:SS" (mínimo 24 bytes).
 * @param[out] out_time Cadena formateada "HH:MM:SS" (mínimo 12 bytes).
 * @param[out] out_temp Puntero opcional para retornar la temperatura interna en °C.
 * @return ESP_OK si la lectura I2C fue exitosa.
 */
esp_err_t rtc_ds3231_read(char *out_datetime, char *out_time, float *out_temp);

/**
 * @brief Retorna si el reloj RTC DS3231 está disponible y operativo.
 */
bool rtc_ds3231_is_available(void);

#ifdef __cplusplus
}
#endif

#endif // RTC_DS3231_H
