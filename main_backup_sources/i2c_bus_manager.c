/**
 * @file i2c_bus_manager.c
 * @brief Implementación del gestor de bus I2C maestro unificado.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include "i2c_bus_manager.h"
#include "config_pins.h"
#include "esp_log.h"

static const char *TAG = "I2C_BUS";
static i2c_master_bus_handle_t s_i2c_bus_handle = NULL;

esp_err_t i2c_bus_manager_init(void)
{
    if (s_i2c_bus_handle != NULL) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Inicializando bus maestro I2C compartido (SDA: GPIO %d, SCL: GPIO %d, 100 kHz)...",
             PIN_I2C_SDA, PIN_I2C_SCL);

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al crear bus maestro I2C: %s", esp_err_to_name(ret));
        s_i2c_bus_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "Bus maestro I2C inicializado exitosamente.");
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_bus_manager_get_handle(void)
{
    return s_i2c_bus_handle;
}
