/**
 * @file sdcard_logger.h
 * @brief Driver y sistema de archivos FATFS sobre SPI para registro en MicroSD.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#ifndef SDCARD_LOGGER_H
#define SDCARD_LOGGER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Monta el sistema de archivos FATFS en la tarjeta MicroSD sobre el bus SPI compartido.
 * @return ESP_OK si la tarjeta fue montada con éxito.
 */
esp_err_t sdcard_logger_init(void);

/**
 * @brief Registra un renglón de telemetría completa en formato CSV en /sdcard/tostion.csv.
 */
esp_err_t sdcard_logger_record(int elapsed_sec, const char *datetime, const char *state_name,
                              const char *mode_name, float temp_c, float weight_g,
                              int motor_pct, int blower_pct, int ptc_pct,
                              float volts, float amps, float watts);

/**
 * @brief Retorna si la tarjeta MicroSD está montada y disponible para escritura.
 */
bool sdcard_logger_is_mounted(void);

/**
 * @brief Retorna el total de renglones grabados durante la sesión.
 */
uint32_t sdcard_logger_get_count(void);

#ifdef __cplusplus
}
#endif

#endif // SDCARD_LOGGER_H
