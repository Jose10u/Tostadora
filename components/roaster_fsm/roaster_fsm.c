/**
 * @file roaster_fsm.c
 * @brief Implementación de la máquina de estados, perfiles de tostión y seguridad térmica.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include "roaster_fsm.h"
#include "config_pins.h"
#include "actuators.h"
#include "sensor_max6675.h"
#include "sensor_hx711.h"
#include "sensor_ina226.h"
#include "rtc_ds3231.h"
#include "sdcard_logger.h"
#include "oled_display.h"
#include "button_controller.h"
#include "wifi_manager.h"
#include "serial_telemetry.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "ROASTER_FSM";

static const mode_profile_t s_mode_profiles[ROAST_MODE_COUNT] = {
    [ROAST_MODE_1_LOW]  = {
        .name = "BAJO",
        .desc = "Clara (City)",
        .target_temp_c = 198.0f,
        .max_time_sec = 540, // 9:00 min
        .ptc_duty = 95,      // ~37% PTC
        .blower_duty = 75,   // ~30% Blower
        .motor_duty = MOTOR_DUTY_MAX_POWER
    },
    [ROAST_MODE_2_MED]  = {
        .name = "MEDIO",
        .desc = "Media (Full City)",
        .target_temp_c = 208.0f,
        .max_time_sec = 660, // 11:00 min
        .ptc_duty = 116,     // ~45% PTC
        .blower_duty = 88,   // ~35% Blower
        .motor_duty = MOTOR_DUTY_MAX_POWER
    },
    [ROAST_MODE_3_HIGH] = {
        .name = "ALTO",
        .desc = "Oscura (Italian)",
        .target_temp_c = 218.0f,
        .max_time_sec = 780, // 13:00 min
        .ptc_duty = 128,     // ~50% PTC (Tope seguro 10V)
        .blower_duty = 100,  // ~40% Blower
        .motor_duty = MOTOR_DUTY_MAX_POWER
    },
    [ROAST_MODE_MANUAL] = {
        .name = "MANUAL",
        .desc = "Personalizado",
        .target_temp_c = 200.0f,
        .max_time_sec = 720, // 12:00 min
        .ptc_duty = 80,
        .blower_duty = 80,
        .motor_duty = MOTOR_DUTY_MAX_POWER
    }
};

static roast_mode_t s_current_mode      = ROAST_MODE_1_LOW;
static sys_state_t s_system_state       = SYS_STATE_STANDBY;
static sys_state_t s_state_before_pause = SYS_STATE_STANDBY;

static int s_elapsed_sec        = 0;
static int s_cooldown_timer     = 0;
static int s_tick_counter_20ms  = 0;

static float s_real_temp_c      = 0.0f;
static bool s_tc_connected      = false;
static float s_est_power_w      = 0.0f;
static float s_est_energy_kwh   = 0.0f;
static char s_datetime_str[24]  = "----/--/-- --:--:--";
static char s_time_str[12]      = "--:--:--";

static SemaphoreHandle_t s_fsm_mutex = NULL;

static void update_energy_estimation(float dt_sec)
{
    uint8_t blw = 0, mtr = 0, ptc = 0;
    actuators_get_duties(&blw, &mtr, &ptc);

    // Modelo de estimación de potencia a 10V:
    float p_ptc = ((float)ptc / (float)DUTY_LIMIT_10V) * 65.0f;
    float p_blw = ((float)blw / (float)DUTY_LIMIT_10V) * 12.0f;
    float p_mtr = (mtr > 0) ? 8.0f : 0.0f;

    s_est_power_w = p_ptc + p_blw + p_mtr;
    s_est_energy_kwh += (s_est_power_w * dt_sec) / (3600.0f * 1000.0f);
}

esp_err_t roaster_fsm_init(void)
{
    s_fsm_mutex = xSemaphoreCreateMutex();
    s_current_mode = ROAST_MODE_1_LOW;
    s_system_state = SYS_STATE_STANDBY;
    s_elapsed_sec = 0;
    s_cooldown_timer = 0;
    s_est_power_w = 0.0f;
    s_est_energy_kwh = 0.0f;

    ESP_LOGI(TAG, "Máquina de estados FSM inicializada (Modo 1: %s).", s_mode_profiles[s_current_mode].name);
    return ESP_OK;
}

void roaster_fsm_start_preheat(void)
{
    if (xSemaphoreTake(s_fsm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (s_system_state == SYS_STATE_STANDBY || s_system_state == SYS_STATE_COOLDOWN) {
            ESP_LOGI(TAG, ">>> INICIANDO PRECALENTAMIENTO (Objetivo: %.1f °C) <<<", PREHEAT_TARGET_TEMP_C);
            s_system_state = SYS_STATE_PREHEATING;
            s_elapsed_sec = 0;
            motor_set_speed(MOTOR_DUTY_MAX_POWER);
            blower_set_power(85);
            ptc_set_power(120);
        }
        xSemaphoreGive(s_fsm_mutex);
    }
}

void roaster_fsm_start_roast(void)
{
    if (xSemaphoreTake(s_fsm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, ">>> INICIANDO TOSTIÓN EN MODO %d (%s: Obj %.1f °C, Límite %d s) <<<",
                 s_current_mode + 1, s_mode_profiles[s_current_mode].name,
                 s_mode_profiles[s_current_mode].target_temp_c,
                 s_mode_profiles[s_current_mode].max_time_sec);
        s_system_state = SYS_STATE_ROASTING;
        s_elapsed_sec = 0;
        motor_set_speed(s_mode_profiles[s_current_mode].motor_duty);
        blower_set_power(s_mode_profiles[s_current_mode].blower_duty);
        ptc_set_power(s_mode_profiles[s_current_mode].ptc_duty);
        xSemaphoreGive(s_fsm_mutex);
    }
}

void roaster_fsm_pause_resume(void)
{
    if (xSemaphoreTake(s_fsm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (s_system_state == SYS_STATE_ROASTING ||
            s_system_state == SYS_STATE_PREHEATING ||
            s_system_state == SYS_STATE_CHARGING) {
            s_state_before_pause = s_system_state;
            s_system_state = SYS_STATE_PAUSED;
            ptc_set_power(0);
            motor_set_speed(0);
            blower_set_power(0);
            ESP_LOGW(TAG, ">>> Secuencia PAUSADA en t=%d s <<<", s_elapsed_sec);
        } else if (s_system_state == SYS_STATE_PAUSED) {
            s_system_state = s_state_before_pause;
            ESP_LOGI(TAG, ">>> Secuencia REANUDADA <<<");
            if (s_system_state == SYS_STATE_PREHEATING) {
                motor_set_speed(MOTOR_DUTY_MAX_POWER);
                blower_set_power(85);
                ptc_set_power(120);
            } else if (s_system_state == SYS_STATE_CHARGING) {
                motor_set_speed(MOTOR_DUTY_MAX_POWER);
                blower_set_power(75);
                ptc_set_power(60);
            } else {
                motor_set_speed(s_mode_profiles[s_current_mode].motor_duty);
                blower_set_power(s_mode_profiles[s_current_mode].blower_duty);
                ptc_set_power(s_mode_profiles[s_current_mode].ptc_duty);
            }
        }
        xSemaphoreGive(s_fsm_mutex);
    }
}

void roaster_fsm_stop(void)
{
    if (xSemaphoreTake(s_fsm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGW(TAG, ">>> Secuencia DETENIDA: Activando Enfriamiento Rápido <<<");
        ptc_set_power(0);
        s_system_state = SYS_STATE_COOLDOWN;
        s_cooldown_timer = COOLDOWN_DURATION_SEC;
        blower_set_power(DUTY_LIMIT_10V);
        motor_set_speed(MOTOR_DUTY_MAX_POWER);
        xSemaphoreGive(s_fsm_mutex);
    }
}

void roaster_fsm_cycle_mode(void)
{
    if (xSemaphoreTake(s_fsm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_current_mode = (roast_mode_t)((s_current_mode + 1) % 3);
        ESP_LOGI(TAG, ">>> MODO SELECCIONADO: Modo %d (%s - %s) <<<",
                 s_current_mode + 1, s_mode_profiles[s_current_mode].name, s_mode_profiles[s_current_mode].desc);
        if (s_system_state == SYS_STATE_ROASTING) {
            motor_set_speed(s_mode_profiles[s_current_mode].motor_duty);
            blower_set_power(s_mode_profiles[s_current_mode].blower_duty);
            ptc_set_power(s_mode_profiles[s_current_mode].ptc_duty);
        }
        xSemaphoreGive(s_fsm_mutex);
    }
}

void roaster_fsm_set_mode(roast_mode_t mode)
{
    if (mode >= ROAST_MODE_COUNT) return;
    if (xSemaphoreTake(s_fsm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_current_mode = mode;
        ESP_LOGI(TAG, ">>> Modo asignado a %d (%s) <<<", s_current_mode + 1, s_mode_profiles[s_current_mode].name);
        if (s_system_state == SYS_STATE_ROASTING) {
            motor_set_speed(s_mode_profiles[s_current_mode].motor_duty);
            blower_set_power(s_mode_profiles[s_current_mode].blower_duty);
            ptc_set_power(s_mode_profiles[s_current_mode].ptc_duty);
        }
        xSemaphoreGive(s_fsm_mutex);
    }
}

void roaster_fsm_set_pwm(int ptc_pct, int blw_pct, int mtr_pct)
{
    if (ptc_pct >= 0 && ptc_pct <= 100) {
        uint8_t d = (ptc_pct * 255) / 100;
        ptc_set_power(d);
    }
    if (blw_pct >= 0 && blw_pct <= 100) {
        uint8_t d = (blw_pct * 255) / 100;
        blower_set_power(d);
    }
    if (mtr_pct >= 0 && mtr_pct <= 100) {
        uint8_t d = (mtr_pct * 255) / 100;
        motor_set_speed(d);
    }
}

void roaster_fsm_process_20ms(void)
{
    button_controller_poll();

    // 1. Lectura inmediata de termocupla y evaluación de seguridad absoluta (>= 225°C)
    sensor_max6675_read(&s_real_temp_c, &s_tc_connected);

    if (s_tc_connected && s_real_temp_c >= EMERGENCY_TEMP_CUTOFF_C) {
        ptc_set_power(0);
        blower_set_power(DUTY_LIMIT_10V);
        motor_set_speed(MOTOR_DUTY_MAX_POWER);
        if (s_system_state != SYS_STATE_EMERGENCY) {
            s_system_state = SYS_STATE_EMERGENCY;
            ESP_LOGE(TAG, "==========================================================");
            ESP_LOGE(TAG, "¡¡¡ CORTE DE SEGURIDAD POR SOBRETEMPERATURA (%.1f °C >= 225 °C) !!!", s_real_temp_c);
            ESP_LOGE(TAG, "  PTC al 0%% y Blower al 100%% para protección total.");
            ESP_LOGE(TAG, "==========================================================");
        }
    }

    // 2. Gestión de eventos de botones
    if (button_check_long_pressed(BTN_INDEX_MODE_TARE)) {
        if (s_system_state == SYS_STATE_STANDBY || s_system_state == SYS_STATE_CHARGING) {
            ESP_LOGI(TAG, ">>> [BOTÓN] Ejecutando tara de báscula HX711... <<<");
            sensor_hx711_tare(10);
        }
    } else if (button_check_pressed(BTN_INDEX_MODE_TARE)) {
        roaster_fsm_cycle_mode();
    }

    if (button_check_pressed(BTN_INDEX_START_STOP)) {
        if (s_system_state == SYS_STATE_STANDBY) {
            roaster_fsm_start_preheat();
        } else if (s_system_state == SYS_STATE_PREHEATING || s_system_state == SYS_STATE_CHARGING) {
            roaster_fsm_start_roast();
        } else if (s_system_state == SYS_STATE_ROASTING || s_system_state == SYS_STATE_PAUSED) {
            roaster_fsm_stop();
        } else if (s_system_state == SYS_STATE_COOLDOWN || s_system_state == SYS_STATE_EMERGENCY) {
            all_actuators_stop();
            s_system_state = SYS_STATE_STANDBY;
            s_elapsed_sec = 0;
        }
    }

    if (button_check_pressed(BTN_INDEX_PAUSE_RESUME)) {
        roaster_fsm_pause_resume();
    }

    // 3. Temporizador de 1 segundo (50 muestras de 20 ms)
    s_tick_counter_20ms++;
    if (s_tick_counter_20ms >= 50) {
        s_tick_counter_20ms = 0;

        float v = 0.0f, a = 0.0f, w = 0.0f;
        sensor_ina226_read(&v, &a, &w);
        float rtc_temp = 0.0f;
        rtc_ds3231_read(s_datetime_str, s_time_str, &rtc_temp);
        update_energy_estimation(1.0f);

        float weight_g = sensor_hx711_get_weight();
        uint8_t blw = 0, mtr = 0, ptc = 0;
        actuators_get_duties(&blw, &mtr, &ptc);
        int blw_pct = (blw * 100) / 255;
        int ptc_pct = (ptc * 100) / 255;
        int mtr_pct = (mtr * 100) / 255;

        // Máquina de estados
        if (s_system_state == SYS_STATE_PREHEATING) {
            s_elapsed_sec++;
            oled_display_render_dashboard("PRECAL", s_elapsed_sec, 600,
                                          s_current_mode + 1, s_mode_profiles[s_current_mode].name,
                                          blw_pct, ptc_pct, mtr_pct, s_real_temp_c, s_tc_connected,
                                          weight_g, sdcard_logger_is_mounted(),
                                          wifi_manager_is_sta_connected(), s_time_str);

            if (s_tc_connected && s_real_temp_c >= PREHEAT_TARGET_TEMP_C) {
                ESP_LOGI(TAG, ">>> ¡PRECALENTADO LISTO (%.1f °C >= 185 °C)! Cargar café y pulsar START <<<", s_real_temp_c);
                s_system_state = SYS_STATE_CHARGING;
                ptc_set_power(60);
                blower_set_power(75);
                motor_set_speed(MOTOR_DUTY_MAX_POWER);
            }
        } else if (s_system_state == SYS_STATE_CHARGING) {
            oled_display_render_dashboard("CARGA", 0, s_mode_profiles[s_current_mode].max_time_sec,
                                          s_current_mode + 1, s_mode_profiles[s_current_mode].name,
                                          blw_pct, ptc_pct, mtr_pct, s_real_temp_c, s_tc_connected,
                                          weight_g, sdcard_logger_is_mounted(),
                                          wifi_manager_is_sta_connected(), s_time_str);
        } else if (s_system_state == SYS_STATE_ROASTING) {
            s_elapsed_sec++;
            oled_display_render_dashboard("TOSTION", s_elapsed_sec, s_mode_profiles[s_current_mode].max_time_sec,
                                          s_current_mode + 1, s_mode_profiles[s_current_mode].name,
                                          blw_pct, ptc_pct, mtr_pct, s_real_temp_c, s_tc_connected,
                                          weight_g, sdcard_logger_is_mounted(),
                                          wifi_manager_is_sta_connected(), s_time_str);

            sdcard_logger_record(s_elapsed_sec, s_datetime_str, "TOSTION",
                                 s_mode_profiles[s_current_mode].name, s_real_temp_c,
                                 weight_g, mtr_pct, blw_pct, ptc_pct,
                                 10.0f, (s_est_power_w / 10.0f), s_est_power_w);

            bool temp_reached = (s_tc_connected && s_real_temp_c >= s_mode_profiles[s_current_mode].target_temp_c);
            bool time_reached = (s_elapsed_sec >= s_mode_profiles[s_current_mode].max_time_sec);

            if (temp_reached || time_reached) {
                ESP_LOGI(TAG, "==========================================================");
                ESP_LOGI(TAG, ">>> ¡TOSTIÓN COMPLETADA! Corte térmico o límite de tiempo <<<");
                ESP_LOGI(TAG, "==========================================================");
                roaster_fsm_stop();
            }
        } else if (s_system_state == SYS_STATE_COOLDOWN) {
            s_cooldown_timer--;
            oled_display_render_dashboard("ENFRIA", COOLDOWN_DURATION_SEC - s_cooldown_timer, COOLDOWN_DURATION_SEC,
                                          s_current_mode + 1, s_mode_profiles[s_current_mode].name,
                                          blw_pct, ptc_pct, mtr_pct, s_real_temp_c, s_tc_connected,
                                          weight_g, sdcard_logger_is_mounted(),
                                          wifi_manager_is_sta_connected(), s_time_str);
            if (s_cooldown_timer <= 0) {
                all_actuators_stop();
                s_system_state = SYS_STATE_STANDBY;
                s_elapsed_sec = 0;
            }
        } else if (s_system_state == SYS_STATE_STANDBY) {
            oled_display_render_dashboard("LISTO", 0, s_mode_profiles[s_current_mode].max_time_sec,
                                          s_current_mode + 1, s_mode_profiles[s_current_mode].name,
                                          blw_pct, ptc_pct, mtr_pct, s_real_temp_c, s_tc_connected,
                                          weight_g, sdcard_logger_is_mounted(),
                                          wifi_manager_is_sta_connected(), s_time_str);
        } else if (s_system_state == SYS_STATE_PAUSED) {
            oled_display_render_dashboard("PAUSA", s_elapsed_sec, s_mode_profiles[s_current_mode].max_time_sec,
                                          s_current_mode + 1, s_mode_profiles[s_current_mode].name,
                                          blw_pct, ptc_pct, mtr_pct, s_real_temp_c, s_tc_connected,
                                          weight_g, sdcard_logger_is_mounted(),
                                          wifi_manager_is_sta_connected(), s_time_str);
        } else if (s_system_state == SYS_STATE_EMERGENCY) {
            oled_display_render_dashboard("!225C!", 0, 0,
                                          s_current_mode + 1, s_mode_profiles[s_current_mode].name,
                                          blw_pct, ptc_pct, mtr_pct, s_real_temp_c, s_tc_connected,
                                          weight_g, sdcard_logger_is_mounted(),
                                          wifi_manager_is_sta_connected(), s_time_str);
        }

        // 4. Formateo y emisión periódica por puerto serial con Timestamp y Manejo de Errores (cada 5s)
        static uint8_t s_hb_sec_counter = 0;
        static uint32_t s_hb_seq = 20; // Inicia para coincidir con la secuencia #25, #30, #35... de la terminal
        s_hb_sec_counter++;
        if (s_hb_sec_counter >= 5) {
            s_hb_sec_counter = 0;
            s_hb_seq += 5;

            uint16_t err_flags = ERR_FLAG_NONE;
            if (!s_tc_connected) {
                err_flags |= ERR_FLAG_TC_DISCONNECTED;
            }
            if (s_real_temp_c >= EMERGENCY_TEMP_CUTOFF_C) {
                err_flags |= ERR_FLAG_OVERTEMPERATURE;
            }
            if (!sensor_hx711_is_available()) {
                err_flags |= ERR_FLAG_HX711_OFFLINE;
            }
            if (!sdcard_logger_is_mounted()) {
                err_flags |= ERR_FLAG_SD_NOT_MOUNTED;
            }
            if (ptc_pct > 0 && blw_pct == 0) {
                err_flags |= ERR_FLAG_INTERLOCK_BLOWER;
            }

            const char *st_tag = "INIT";
            switch (s_system_state) {
                case SYS_STATE_STANDBY:    st_tag = "INIT";    break;
                case SYS_STATE_PREHEATING: st_tag = "PRECAL";  break;
                case SYS_STATE_CHARGING:   st_tag = "CARGA";   break;
                case SYS_STATE_ROASTING:   st_tag = "TOST";    break;
                case SYS_STATE_PAUSED:     st_tag = "PAUSA";   break;
                case SYS_STATE_COOLDOWN:   st_tag = "ENFRIA";  break;
                case SYS_STATE_EMERGENCY:  st_tag = "EMERG";   break;
            }

            serial_frame_data_t f_data = {
                .heartbeat_num = s_hb_seq,
                .datetime_str  = s_datetime_str,
                .volts         = v,
                .amps          = a,
                .watts         = w,
                .energy_wh     = s_est_energy_kwh * 1000.0f,
                .weight_g      = weight_g,
                .temp_c        = s_real_temp_c,
                .state_str     = st_tag,
                .mode_num      = (int)s_current_mode + 1,
                .mode_name     = s_mode_profiles[s_current_mode].name,
                .ptc_active    = (ptc_pct > 0),
                .blower_pct    = (uint8_t)blw_pct,
                .motor_pct     = (uint8_t)mtr_pct,
                .error_flags   = err_flags
            };

            serial_telemetry_send_heartbeat(&f_data);
        }
    }
}

void roaster_fsm_get_telemetry(web_telemetry_t *t)
{
    if (t == NULL) return;

    const char *state_str = "STANDBY";
    const char *state_desc = "Listo para iniciar";
    int step_idx = 0;

    switch (s_system_state) {
        case SYS_STATE_STANDBY:   state_str = "STANDBY";   state_desc = "Esperando inicio"; step_idx = 0; break;
        case SYS_STATE_PREHEATING:state_str = "PREHEATING";state_desc = "Precalentando cámara a 185°C"; step_idx = 1; break;
        case SYS_STATE_CHARGING:  state_str = "CHARGING";  state_desc = "Cámara lista a 185°C. Cargar grano"; step_idx = 1; break;
        case SYS_STATE_ROASTING:  state_str = "ROASTING";  state_desc = "Tostión activa en marcha"; step_idx = 2; break;
        case SYS_STATE_PAUSED:    state_str = "PAUSED";    state_desc = "Secuencia en pausa"; step_idx = 2; break;
        case SYS_STATE_COOLDOWN:  state_str = "COOLDOWN";  state_desc = "Enfriamiento rápido activo"; step_idx = 3; break;
        case SYS_STATE_EMERGENCY: state_str = "EMERGENCY"; state_desc = "¡Alerta corte por sobretemperatura >= 225°C!"; step_idx = 3; break;
    }

    float tgt = s_mode_profiles[s_current_mode].target_temp_c;
    int cfg_time = s_mode_profiles[s_current_mode].max_time_sec;
    if (s_system_state == SYS_STATE_PREHEATING) {
        tgt = PREHEAT_TARGET_TEMP_C;
        cfg_time = 600;
    }

    uint8_t blw = 0, mtr = 0, ptc = 0;
    actuators_get_duties(&blw, &mtr, &ptc);

    t->state_str          = state_str;
    t->state_desc         = state_desc;
    t->step_index         = step_idx;
    t->mode               = (int)s_current_mode;
    t->mode_name          = s_mode_profiles[s_current_mode].desc;
    t->temp_actual        = s_real_temp_c;
    t->temp_target        = tgt;
    t->weight_g           = sensor_hx711_get_weight();
    t->time_config_sec    = cfg_time;
    t->time_elapsed_sec   = s_elapsed_sec;
    t->time_remaining_sec = (cfg_time > s_elapsed_sec) ? (cfg_time - s_elapsed_sec) : 0;
    t->power_w            = s_est_power_w;
    t->energy_kwh         = s_est_energy_kwh;
    t->pwm_ptc            = (ptc * 100) / 255;
    t->pwm_blw            = (blw * 100) / 255;
    t->pwm_mtr            = (mtr * 100) / 255;
    t->sd_ok              = sdcard_logger_is_mounted();
    t->tc_connected       = s_tc_connected;
    t->emergency          = (s_system_state == SYS_STATE_EMERGENCY || s_real_temp_c >= EMERGENCY_TEMP_CUTOFF_C);

    static char ip_buf[24];
    if (wifi_manager_is_sta_connected()) {
        wifi_manager_get_sta_ip(ip_buf, sizeof(ip_buf));
        t->wifi_connected = true;
    } else {
        wifi_manager_get_ap_ip(ip_buf, sizeof(ip_buf));
        t->wifi_connected = false;
    }
    t->wifi_ip  = ip_buf;
    t->rtc_time = s_datetime_str;
}

void roaster_fsm_handle_web_command(const web_command_t *cmd)
{
    if (cmd == NULL) return;

    switch (cmd->type) {
        case WEB_CMD_START_PREHEAT:
            roaster_fsm_start_preheat();
            break;
        case WEB_CMD_START_ROAST:
            roaster_fsm_start_roast();
            break;
        case WEB_CMD_PAUSE_RESUME:
            roaster_fsm_pause_resume();
            break;
        case WEB_CMD_STOP:
            roaster_fsm_stop();
            break;
        case WEB_CMD_TARE:
            sensor_hx711_tare(10);
            break;
        case WEB_CMD_SET_MODE:
            roaster_fsm_set_mode((roast_mode_t)cmd->mode);
            break;
        case WEB_CMD_SET_PWM:
            roaster_fsm_set_pwm(cmd->ptc_pct, cmd->blw_pct, cmd->mtr_pct);
            break;
    }
}
