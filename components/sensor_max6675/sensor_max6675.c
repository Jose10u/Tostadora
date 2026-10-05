/**
 * @file sensor_max6675.c
 * @brief Implementación del driver SPI para la termocupla MAX6675.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include <string.h>
#include "sensor_max6675.h"
#include "spi_bus_manager.h"
#include "config_pins.h"
#include "esp_log.h"

static const char *TAG = "MAX6675";
static spi_device_handle_t s_max6675_dev = NULL;
static bool s_max6675_available = false;

esp_err_t sensor_max6675_init(void)
{
    if (spi_bus_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo acceder al bus SPI maestro.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Registrando dispositivo MAX6675 (CS: GPIO %d, 1 MHz, Modo 0)...", PIN_MAX6675_CS);

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 1000000,          // 1 MHz
        .mode           = 0,                // Modo 0 (CPOL=0, CPHA=0)
        .spics_io_num   = PIN_MAX6675_CS,   // GPIO 15
        .queue_size     = 1,
        .flags          = SPI_DEVICE_NO_DUMMY,
    };

    esp_err_t ret = spi_bus_add_device(spi_bus_manager_get_host(), &dev_cfg, &s_max6675_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al registrar MAX6675 en el bus SPI: %s", esp_err_to_name(ret));
        s_max6675_available = false;
        return ret;
    }

    s_max6675_available = true;
    ESP_LOGI(TAG, ">>> TERMOCUPLA MAX6675 INICIALIZADA EXITOSAMENTE (CS: GPIO %d) <<<", PIN_MAX6675_CS);
    return ESP_OK;
}

esp_err_t sensor_max6675_read(float *out_temp, bool *out_connected)
{
    if (!s_max6675_available || s_max6675_dev == NULL) {
        if (out_connected) *out_connected = false;
        if (out_temp) *out_temp = 0.0f;
        return ESP_ERR_INVALID_STATE;
    }

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length   = 16;
    t.rxlength = 16;
    t.flags    = SPI_TRANS_USE_RXDATA;

    esp_err_t ret = spi_device_polling_transmit(s_max6675_dev, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error en transacción SPI del MAX6675: %s", esp_err_to_name(ret));
        if (out_connected) *out_connected = false;
        return ret;
    }

    uint16_t raw_data = ((uint16_t)t.rx_data[0] << 8) | t.rx_data[1];

    // Bit 2: 1 = Termocupla desconectada / circuito abierto; 0 = Correcta
    bool is_open = (raw_data & 0x0004) != 0;

    if (is_open) {
        if (out_connected) *out_connected = false;
        if (out_temp) *out_temp = 0.0f;
    } else {
        if (out_connected) *out_connected = true;
        // Bits 14..3: temperatura en escala de 0.25 °C por cuenta
        float temp = (float)(raw_data >> 3) * 0.25f;
        if (out_temp) *out_temp = temp;
    }

    return ESP_OK;
}

bool sensor_max6675_is_available(void)
{
    return s_max6675_available;
}
