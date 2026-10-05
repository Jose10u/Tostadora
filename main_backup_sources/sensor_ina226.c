/**
 * @file sensor_ina226.c
 * @brief Implementación del driver I2C para el sensor INA226.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include "sensor_ina226.h"
#include "i2c_bus_manager.h"
#include "config_pins.h"
#include "esp_log.h"

static const char *TAG = "INA226";
static i2c_master_dev_handle_t s_ina226_dev = NULL;
static bool s_ina226_available = false;

esp_err_t sensor_ina226_init(void)
{
    if (i2c_bus_manager_init() != ESP_OK) {
        return ESP_FAIL;
    }

    i2c_master_bus_handle_t bus = i2c_bus_manager_get_handle();
    if (bus == NULL) return ESP_FAIL;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA226_I2C_ADDR,
        .scl_speed_hz = 100000,
    };

    i2c_master_dev_handle_t temp_dev = NULL;
    if (i2c_master_bus_add_device(bus, &dev_cfg, &temp_dev) != ESP_OK) {
        s_ina226_available = false;
        return ESP_FAIL;
    }

    // Leer registro de ID del fabricante (0xFE)
    uint8_t reg_fe = 0xFE;
    uint8_t mfg_id[2] = {0};
    if (i2c_master_transmit_receive(temp_dev, &reg_fe, 1, mfg_id, 2, 50) == ESP_OK) {
        uint16_t id = ((uint16_t)mfg_id[0] << 8) | mfg_id[1];
        if (id == 0x5449) { // Texas Instruments ('TI')
            s_ina226_dev = temp_dev;
            s_ina226_available = true;
            ESP_LOGI(TAG, ">>> MONITOR INA226 DETECTADO EN 0x%02X <<<", INA226_I2C_ADDR);
            return ESP_OK;
        }
    }

    i2c_master_bus_rm_device(temp_dev);
    s_ina226_available = false;
    ESP_LOGI(TAG, "Monitor INA226 no conectado en 0x%02X (se utilizará modelo de estimación matemática de potencia).", INA226_I2C_ADDR);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t sensor_ina226_read(float *out_volts, float *out_amps, float *out_watts)
{
    if (!s_ina226_available || s_ina226_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Registro Bus Voltage: 0x02 (LSB = 1.25 mV)
    uint8_t reg_bus = 0x02;
    uint8_t bus_data[2] = {0};
    if (i2c_master_transmit_receive(s_ina226_dev, &reg_bus, 1, bus_data, 2, 50) != ESP_OK) {
        return ESP_FAIL;
    }
    uint16_t raw_bus = ((uint16_t)bus_data[0] << 8) | bus_data[1];
    float volts = (float)raw_bus * 0.00125f;

    // Registro Shunt Voltage: 0x01 (LSB = 2.5 uV, Resistencia Shunt 0.01 Ohm)
    uint8_t reg_shunt = 0x01;
    uint8_t shunt_data[2] = {0};
    if (i2c_master_transmit_receive(s_ina226_dev, &reg_shunt, 1, shunt_data, 2, 50) != ESP_OK) {
        return ESP_FAIL;
    }
    int16_t raw_shunt = (int16_t)(((uint16_t)shunt_data[0] << 8) | shunt_data[1]);
    float amps = (float)raw_shunt * 0.00025f; // 2.5uV / 0.01 ohm
    if (amps < 0.0f) amps = 0.0f;

    if (out_volts) *out_volts = volts;
    if (out_amps)  *out_amps  = amps;
    if (out_watts) *out_watts = volts * amps;

    return ESP_OK;
}

bool sensor_ina226_is_available(void)
{
    return s_ina226_available;
}
