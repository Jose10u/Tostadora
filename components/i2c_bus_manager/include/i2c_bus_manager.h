/**
 * @file i2c_bus_manager.h
 * @brief Gestor centralizado del bus I2C maestro unificado (SDA: GPIO 21, SCL: GPIO 22).
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#ifndef I2C_BUS_MANAGER_H
#define I2C_BUS_MANAGER_H

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el bus maestro I2C compartido si aún no ha sido creado.
 * @return ESP_OK si el bus se inicializó correctamente o ya existía.
 */
esp_err_t i2c_bus_manager_init(void);

/**
 * @brief Obtiene el manejador del bus maestro I2C para registrar periféricos.
 * @return Manejador del bus i2c_master_bus_handle_t.
 */
i2c_master_bus_handle_t i2c_bus_manager_get_handle(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_BUS_MANAGER_H
