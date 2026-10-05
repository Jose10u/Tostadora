/**
 * @file rtc_ds3231.c
 * @brief Implementación del driver I2C para el RTC DS3231.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include <stdio.h>
#include <string.h>
#include "rtc_ds3231.h"
#include "i2c_bus_manager.h"
#include "config_pins.h"
#include "esp_log.h"

static const char *TAG = "DS3231";
static i2c_master_dev_handle_t s_ds3231_dev = NULL;
static bool s_ds3231_available = false;

static inline uint8_t bcd_to_dec(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

esp_err_t rtc_ds3231_init(void)
{
    if (i2c_bus_manager_init() != ESP_OK) {
        return ESP_FAIL;
    }

    i2c_master_bus_handle_t bus = i2c_bus_manager_get_handle();
    if (bus == NULL) return ESP_FAIL;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_I2C_ADDR,
        .scl_speed_hz = 100000,
    };

    i2c_master_dev_handle_t temp_dev = NULL;
    if (i2c_master_bus_add_device(bus, &dev_cfg, &temp_dev) != ESP_OK) {
        s_ds3231_available = false;
        return ESP_FAIL;
    }

    // Comprobación de lectura del registro 0x00 (segundos)
    uint8_t reg = 0x00;
    uint8_t sec_byte = 0;
    if (i2c_master_transmit_receive(temp_dev, &reg, 1, &sec_byte, 1, 50) == ESP_OK) {
        s_ds3231_dev = temp_dev;
        s_ds3231_available = true;
        ESP_LOGI(TAG, ">>> RELOJ EN TIEMPO REAL RTC DS3231 DETECTADO EN 0x%02X <<<", DS3231_I2C_ADDR);
        return ESP_OK;
    }

    i2c_master_bus_rm_device(temp_dev);
    s_ds3231_available = false;
    ESP_LOGW(TAG, "[DS3231] No respondió en dirección 0x%02X.", DS3231_I2C_ADDR);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t rtc_ds3231_read(char *out_datetime, char *out_time, float *out_temp)
{
    if (!s_ds3231_available || s_ds3231_dev == NULL) {
        if (out_datetime) snprintf(out_datetime, 24, "----/--/-- --:--:--");
        if (out_time)     snprintf(out_time, 12, "--:--:--");
        if (out_temp)     *out_temp = 0.0f;
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t reg_start = 0x00;
    uint8_t rtc_buf[7];
    esp_err_t ret = i2c_master_transmit_receive(s_ds3231_dev, &reg_start, 1, rtc_buf, sizeof(rtc_buf), 50);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t sec   = bcd_to_dec(rtc_buf[0] & 0x7F);
    uint8_t min   = bcd_to_dec(rtc_buf[1] & 0x7F);
    uint8_t hour  = bcd_to_dec(rtc_buf[2] & 0x3F);
    uint8_t date  = bcd_to_dec(rtc_buf[4] & 0x3F);
    uint8_t month = bcd_to_dec(rtc_buf[5] & 0x1F);
    uint16_t year = 2000 + bcd_to_dec(rtc_buf[6]);

    if (out_datetime) {
        snprintf(out_datetime, 24, "%04d/%02d/%02d %02d:%02d:%02d", year, month, date, hour, min, sec);
    }
    if (out_time) {
        snprintf(out_time, 12, "%02d:%02d:%02d", hour, min, sec);
    }

    if (out_temp) {
        uint8_t reg_temp = 0x11;
        uint8_t temp_buf[2] = {0};
        if (i2c_master_transmit_receive(s_ds3231_dev, &reg_temp, 1, temp_buf, 2, 50) == ESP_OK) {
            int8_t t_msb = (int8_t)temp_buf[0];
            uint8_t t_lsb = temp_buf[1] >> 6;
            *out_temp = (float)t_msb + ((float)t_lsb * 0.25f);
        } else {
            *out_temp = 0.0f;
        }
    }

    return ESP_OK;
}

bool rtc_ds3231_is_available(void)
{
    return s_ds3231_available;
}
