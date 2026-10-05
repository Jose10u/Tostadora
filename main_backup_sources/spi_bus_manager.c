/**
 * @file spi_bus_manager.c
 * @brief Implementación del bus SPI maestro unificado con soporte DMA para MicroSD y MAX6675.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include "spi_bus_manager.h"
#include "config_pins.h"
#include "esp_log.h"

static const char *TAG = "SPI_BUS";
static bool s_spi_initialized = false;

esp_err_t spi_bus_manager_init(void)
{
    if (s_spi_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Configurando bus SPI maestro unificado (SCLK:%d, MISO:%d, MOSI:%d)...",
             PIN_SPI_SCLK, PIN_SPI_MISO, PIN_SPI_MOSI);

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096, // Tamaño requerido para bloques FATFS
    };

    esp_err_t ret = spi_bus_initialize(TOSTADORA_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Fallo al inicializar bus SPI maestro: %s", esp_err_to_name(ret));
        return ret;
    }

    s_spi_initialized = true;
    ESP_LOGI(TAG, "Bus SPI maestro unificado disponible con éxito.");
    return ESP_OK;
}

spi_host_device_t spi_bus_manager_get_host(void)
{
    return TOSTADORA_SPI_HOST;
}
