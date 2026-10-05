/**
 * @file sdcard_logger.c
 * @brief Implementación del logger en tarjeta MicroSD usando VFS FATFS sobre SPI.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include <stdio.h>
#include <string.h>
#include "sdcard_logger.h"
#include "spi_bus_manager.h"
#include "config_pins.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"

static const char *TAG = "SD_LOGGER";
static sdmmc_card_t *s_sd_card = NULL;
static bool s_sd_mounted = false;
static uint32_t s_records_count = 0;

esp_err_t sdcard_logger_init(void)
{
    if (spi_bus_manager_init() != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo asegurar bus SPI para MicroSD.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Montando sistema de archivos FATFS en MicroSD (CS: GPIO %d)...", PIN_SD_CS);

    esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = 4,
        .allocation_unit_size   = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = spi_bus_manager_get_host();

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_SD_CS;
    slot_config.host_id = spi_bus_manager_get_host();

    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_cfg, &s_sd_card);
    if (ret != ESP_OK) {
        s_sd_mounted = false;
        s_sd_card = NULL;
        ESP_LOGW(TAG, "[SD] Tarjeta no detectada (%s). Continuando sin datalogger.", esp_err_to_name(ret));
        return ret;
    }

    s_sd_mounted = true;
    ESP_LOGI(TAG, ">>> TARJETA MICROSD MONTADA EXITOSAMENTE EN %s <<<", SD_MOUNT_POINT);

    // Inicializar cabecera si el archivo es nuevo
    FILE *f = fopen(SD_MOUNT_POINT "/tostion.csv", "a");
    if (f != NULL) {
        long pos = ftell(f);
        if (pos == 0) {
            fprintf(f, "Tiempo_s,Fecha_Hora,Estado,Modo,Temp_C,Peso_g,Motor_Pct,Blower_Pct,PTC_Pct,Volt_V,Corr_A,Pot_W\n");
        }
        fprintf(f, "# --- SESION INICIADA ---\n");
        fclose(f);
    }

    return ESP_OK;
}

esp_err_t sdcard_logger_record(int elapsed_sec, const char *datetime, const char *state_name,
                              const char *mode_name, float temp_c, float weight_g,
                              int motor_pct, int blower_pct, int ptc_pct,
                              float volts, float amps, float watts)
{
    if (!s_sd_mounted) return ESP_ERR_INVALID_STATE;

    FILE *f = fopen(SD_MOUNT_POINT "/tostion.csv", "a");
    if (f == NULL) return ESP_FAIL;

    fprintf(f, "%d,%s,%s,%s,%.2f,%.2f,%d,%d,%d,%.2f,%.2f,%.2f\n",
            elapsed_sec,
            datetime ? datetime : "----/--/-- --:--:--",
            state_name ? state_name : "UNKNOWN",
            mode_name ? mode_name : "MODE",
            temp_c,
            weight_g,
            motor_pct,
            blower_pct,
            ptc_pct,
            volts,
            amps,
            watts);

    fclose(f);
    s_records_count++;
    return ESP_OK;
}

bool sdcard_logger_is_mounted(void)
{
    return s_sd_mounted;
}

uint32_t sdcard_logger_get_count(void)
{
    return s_records_count;
}
