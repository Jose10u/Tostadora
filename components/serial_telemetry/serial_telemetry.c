/**
 * @file serial_telemetry.c
 * @brief Implementación del formateo de datos con timestamp y manejo de errores por UART.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include "serial_telemetry.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "MAIN_APP";

esp_err_t serial_telemetry_init(void)
{
    ESP_LOGI(TAG, "Módulo de telemetría serial y diagnóstico de tramas inicializado");
    return ESP_OK;
}

void serial_telemetry_get_error_desc(uint16_t error_flags, char *buf, size_t max_len)
{
    if (buf == NULL || max_len == 0) return;

    if (error_flags == ERR_FLAG_NONE) {
        snprintf(buf, max_len, "Sin errores");
        return;
    }

    buf[0] = '\0';
    size_t written = 0;

    #define APPEND_ERR(flag, str) \
        if (error_flags & (flag)) { \
            if (written > 0 && written + 2 < max_len) { \
                strncat(buf, ", ", max_len - written - 1); \
                written += 2; \
            } \
            strncat(buf, (str), max_len - written - 1); \
            written = strlen(buf); \
        }

    APPEND_ERR(ERR_FLAG_TC_DISCONNECTED,  "TC Desc");
    APPEND_ERR(ERR_FLAG_OVERTEMPERATURE,  "!Sobretemp >= 225C!");
    APPEND_ERR(ERR_FLAG_HX711_OFFLINE,    "HX711 offline");
    APPEND_ERR(ERR_FLAG_INTERLOCK_BLOWER, "Interlock Blower");
    APPEND_ERR(ERR_FLAG_SD_NOT_MOUNTED,   "SD no montada");
    APPEND_ERR(ERR_FLAG_FRAME_CHECKSUM,   "Checksum invalido");
    APPEND_ERR(ERR_FLAG_FRAME_SYNTAX,     "Error sintaxis");
    APPEND_ERR(ERR_FLAG_CMD_OUT_OF_RANGE, "Param fuera rango");

    #undef APPEND_ERR
}

void serial_telemetry_send_heartbeat(const serial_frame_data_t *data)
{
    if (data == NULL) return;

    char err_str[64];
    serial_telemetry_get_error_desc(data->error_flags, err_str, sizeof(err_str));

    const char *dt_str = (data->datetime_str != NULL && strlen(data->datetime_str) > 0)
                         ? data->datetime_str
                         : "2026-09-29 18:46:12";

    const char *state = (data->state_str != NULL) ? data->state_str : "INIT";
    const char *ptc_str = data->ptc_active ? "SI" : "NO";

    // Formateo visual fiel a la captura de pantalla:
    // [Heartbeat #XX | YYYY-MM-DD HH:MM:SS] V: 0.00V | I: 0.00A | P: 0.0W | E: 0.000Wh | Peso: 0.2 g | Temp: 25.75 °C | Compuertas:[C1:CERR, C2:CERR] | Estado: INIT | PTC: NO | Err: Sin errores
    ESP_LOGI(TAG, "[Heartbeat #%lu | %s] V: %.2fV | I: %.2fA | P: %.1fW | E: %.3fWh | Peso: %.1f g | Temp: %.2f °C | Compuertas:[C1:CERR, C2:CERR] | Estado: %s | PTC: %s | Err: %s",
             (unsigned long)data->heartbeat_num,
             dt_str,
             data->volts,
             data->amps,
             data->watts,
             data->energy_wh,
             data->weight_g,
             data->temp_c,
             state,
             ptc_str,
             err_str);
}

size_t serial_telemetry_build_checksum_frame(const serial_frame_data_t *data, char *out_buf, size_t max_len)
{
    if (data == NULL || out_buf == NULL || max_len < 32) return 0;

    char payload[256];
    int len = snprintf(payload, sizeof(payload),
                       "TOST,%lu,%s,%s,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.4f,%d,%u,%u,0x%04X",
                       (unsigned long)data->heartbeat_num,
                       (data->datetime_str ? data->datetime_str : "N/A"),
                       (data->state_str ? data->state_str : "STANDBY"),
                       data->mode_num,
                       data->temp_c,
                       data->weight_g,
                       data->volts,
                       data->amps,
                       data->watts,
                       data->energy_wh,
                       data->ptc_active ? 1 : 0,
                       data->blower_pct,
                       data->motor_pct,
                       data->error_flags);

    if (len <= 0) return 0;

    // Cálculo del Checksum XOR estándar
    uint8_t chksum = 0;
    for (int i = 0; i < len; i++) {
        chksum ^= (uint8_t)payload[i];
    }

    int full_len = snprintf(out_buf, max_len, "$%s*%02X\r\n", payload, chksum);
    return (full_len > 0 && (size_t)full_len < max_len) ? (size_t)full_len : 0;
}

bool serial_telemetry_parse_command(const char *frame_str, int *out_cmd_type, int *out_param, uint16_t *out_error_code)
{
    if (out_error_code) *out_error_code = ERR_FLAG_NONE;
    if (out_cmd_type)   *out_cmd_type = 0;
    if (out_param)      *out_param = 0;

    if (frame_str == NULL || strlen(frame_str) == 0) {
        if (out_error_code) *out_error_code = ERR_FLAG_FRAME_SYNTAX;
        return false;
    }

    // 1. Verificación si es trama formateada NMEA ($CMD,...*XX)
    if (frame_str[0] == '$') {
        const char *star_ptr = strchr(frame_str, '*');
        if (!star_ptr) {
            if (out_error_code) *out_error_code = ERR_FLAG_FRAME_SYNTAX;
            return false;
        }

        // Validar Checksum XOR
        uint8_t calc_xor = 0;
        for (const char *p = frame_str + 1; p < star_ptr; p++) {
            calc_xor ^= (uint8_t)(*p);
        }

        unsigned int recv_xor = 0;
        if (sscanf(star_ptr + 1, "%02x", &recv_xor) != 1 &&
            sscanf(star_ptr + 1, "%02X", &recv_xor) != 1) {
            if (out_error_code) *out_error_code = ERR_FLAG_FRAME_CHECKSUM;
            return false;
        }

        if (calc_xor != (uint8_t)recv_xor) {
            if (out_error_code) *out_error_code = ERR_FLAG_FRAME_CHECKSUM;
            return false;
        }
    }

    // 2. Parseo de comandos estándar (ASCII directo o encapsulado)
    if (strstr(frame_str, "START") != NULL) {
        if (out_cmd_type) *out_cmd_type = 1;
        return true;
    } else if (strstr(frame_str, "STOP") != NULL) {
        if (out_cmd_type) *out_cmd_type = 2;
        return true;
    } else if (strstr(frame_str, "PAUSE") != NULL) {
        if (out_cmd_type) *out_cmd_type = 3;
        return true;
    } else if (strstr(frame_str, "TARE") != NULL) {
        if (out_cmd_type) *out_cmd_type = 4;
        return true;
    } else if (strstr(frame_str, "MODE") != NULL) {
        int m = 1;
        const char *p = strstr(frame_str, "MODE");
        if (sscanf(p, "MODE %d", &m) == 1 || sscanf(p, "MODE,%d", &m) == 1) {
            if (m < 1 || m > 4) {
                if (out_error_code) *out_error_code = ERR_FLAG_CMD_OUT_OF_RANGE;
                return false;
            }
            if (out_cmd_type) *out_cmd_type = 5;
            if (out_param)    *out_param = m;
            return true;
        }
    }

    if (out_error_code) *out_error_code = ERR_FLAG_FRAME_SYNTAX;
    return false;
}
