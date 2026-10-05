/**
 * @file spi_bus_manager.h
 * @brief Gestor centralizado del bus SPI maestro unificado (SCLK: 18, MISO: 19, MOSI: 23).
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#ifndef SPI_BUS_MANAGER_H
#define SPI_BUS_MANAGER_H

#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el bus maestro SPI maestro unificado para MAX6675 y MicroSD.
 * @return ESP_OK si la inicialización fue exitosa o ya existía el bus.
 */
esp_err_t spi_bus_manager_init(void);

/**
 * @brief Retorna el identificador del host SPI unificado.
 */
spi_host_device_t spi_bus_manager_get_host(void);

#ifdef __cplusplus
}
#endif

#endif // SPI_BUS_MANAGER_H
