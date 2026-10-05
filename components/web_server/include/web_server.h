#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WEB_CMD_START_PREHEAT = 0,
    WEB_CMD_START_ROAST,
    WEB_CMD_PAUSE_RESUME,
    WEB_CMD_STOP,
    WEB_CMD_TARE,
    WEB_CMD_SET_MODE,
    WEB_CMD_SET_PWM,
} web_cmd_type_t;

typedef struct {
    web_cmd_type_t type;
    int mode;        // 0: Bajo, 1: Medio, 2: Alto, 3: Manual
    int ptc_pct;     // 0..100
    int blw_pct;     // 0..100
    int mtr_pct;     // 0..100
} web_command_t;

typedef struct {
    const char *state_str;      // "STANDBY", "PREHEATING", "CHARGING", "ROASTING", "PAUSED", "COOLDOWN", "EMERGENCY"
    const char *state_desc;
    int step_index;             // 0: LISTO, 1: Precalentando, 2: Tostión, 3: Enfriamiento
    int mode;                   // 0..3
    const char *mode_name;      // "Tostión 1 (Bajo)", etc.
    float temp_actual;          // MAX6675 °C
    float temp_target;          // Target °C (185, 198, 208, 218)
    float weight_g;             // HX711 g
    int time_config_sec;        // Segundos totales configurados
    int time_remaining_sec;     // Segundos restantes
    int time_elapsed_sec;       // Segundos transcurridos
    float power_w;              // Potencia instantánea estimada en Watts
    float energy_kwh;           // Energía acumulada estimada en kWh
    int pwm_ptc;                // PTC % (0..100)
    int pwm_blw;                // Blower % (0..100)
    int pwm_mtr;                // Motor % (0..100)
    bool sd_ok;                 // Tarjeta MicroSD disponible
    bool tc_connected;          // Termocupla MAX6675 conectada
    bool emergency;             // Corte de seguridad 225°C activo
    const char *wifi_ip;        // Dirección IP activa
    bool wifi_connected;        // Conexión STA activa al router
    const char *rtc_time;       // Fecha y hora del RTC DS3231
} web_telemetry_t;

typedef void (*web_cmd_callback_t)(const web_command_t *cmd);

/**
 * @brief Inicializa el servidor HTTP embebido del ESP32 en el puerto 80.
 */
httpd_handle_t web_server_start(void (*get_telem_cb)(web_telemetry_t *t), web_cmd_callback_t cmd_cb);

/**
 * @brief Detiene el servidor HTTP embebido.
 */
void web_server_stop(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H
