/**
 * @file roaster_fsm.h
 * @brief Máquina de estados finitos (FSM) y perfiles térmicos de la Tostadora de Café IoT.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 *
 * @note Administra los 3 modos de tueste, precalentamiento a 185°C,
 *       corte de seguridad absoluto a 225°C y telemetría FreeRTOS.
 */

#ifndef ROASTER_FSM_H
#define ROASTER_FSM_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "web_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum roast_mode_t
 * @brief Modos de tostión seleccionables por el usuario.
 */
typedef enum {
    ROAST_MODE_1_LOW = 0,   ///< Modo 1: Tostión Baja (Clara / "City") -> 198 °C / 9:00 min
    ROAST_MODE_2_MED = 1,   ///< Modo 2: Tostión Media (Balanceada / "Full City") -> 208 °C / 11:00 min
    ROAST_MODE_3_HIGH = 2,  ///< Modo 3: Tostión Alta (Oscura / "Italian/French") -> 218 °C / 13:00 min
    ROAST_MODE_MANUAL = 3,  ///< Modo Manual: Configuración personalizada de PWM
    ROAST_MODE_COUNT = 4
} roast_mode_t;

/**
 * @enum sys_state_t
 * @brief Estados del proceso secuencial de tostión.
 */
typedef enum {
    SYS_STATE_STANDBY = 0,  ///< Reposo: listo para iniciar precalentamiento
    SYS_STATE_PREHEATING,   ///< Precalentamiento de la cámara hasta 180°C - 190°C (185°C)
    SYS_STATE_CHARGING,     ///< Cámara precalentada: lista para cargar grano verde y seleccionar modo
    SYS_STATE_ROASTING,     ///< Tostión activa aplicando perfil térmico y temporal
    SYS_STATE_PAUSED,       ///< Pausa: actuadores detenidos y cronómetro congelado
    SYS_STATE_COOLDOWN,     ///< Enfriamiento rápido de seguridad (PTC 0%, Blower 100%)
    SYS_STATE_EMERGENCY,    ///< Corte irrevocable por sobretemperatura (>= 225.0 °C)
} sys_state_t;

/**
 * @struct mode_profile_t
 * @brief Estructura de parámetros por modo de tueste.
 */
typedef struct {
    const char *name;       ///< Nombre corto ("BAJO", "MEDIO", "ALTO", "MANUAL")
    const char *desc;       ///< Descripción sensorial ("Clara / City", etc.)
    float target_temp_c;    ///< Temperatura de descarga (198°C, 208°C, 218°C)
    int max_time_sec;       ///< Tiempo máximo del ciclo (540s, 660s, 780s)
    uint8_t ptc_duty;       ///< Duty PWM del calefactor PTC (0..128)
    uint8_t blower_duty;    ///< Duty PWM del soplador centrífugo
    uint8_t motor_duty;     ///< Duty PWM del motorreductor (255 / 100%)
} mode_profile_t;

/**
 * @brief Inicializa la máquina de estados, variables globales y sincronización FreeRTOS.
 */
esp_err_t roaster_fsm_init(void);

/**
 * @brief Inicia el precalentamiento de la cámara hacia 185 °C.
 */
void roaster_fsm_start_preheat(void);

/**
 * @brief Inicia la fase de tostión activa según el modo configurado.
 */
void roaster_fsm_start_roast(void);

/**
 * @brief Conmuta entre pausa y reanudación del proceso.
 */
void roaster_fsm_pause_resume(void);

/**
 * @brief Detiene el ciclo actual y entra en enfriamiento forzado.
 */
void roaster_fsm_stop(void);

/**
 * @brief Conmuta cíclicamente de modo (Modo 1 -> Modo 2 -> Modo 3).
 */
void roaster_fsm_cycle_mode(void);

/**
 * @brief Establece un modo específico directamente (0..3).
 */
void roaster_fsm_set_mode(roast_mode_t mode);

/**
 * @brief Ajusta los ciclos de trabajo PWM de forma manual (0..100%).
 */
void roaster_fsm_set_pwm(int ptc_pct, int blw_pct, int mtr_pct);

/**
 * @brief Ejecuta el ciclo de supervisión en tiempo real (invocar periódicamente a 20 ms).
 */
void roaster_fsm_process_20ms(void);

/**
 * @brief Obtiene la telemetría actual estructurada para el servidor web IoT.
 */
void roaster_fsm_get_telemetry(web_telemetry_t *t);

/**
 * @brief Despacha comandos recibidos desde el servidor web HTTP hacia la máquina de estados.
 */
void roaster_fsm_handle_web_command(const web_command_t *cmd);

#ifdef __cplusplus
}
#endif

#endif // ROASTER_FSM_H
