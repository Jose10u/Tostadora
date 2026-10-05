/**
 * @file serial_telemetry.h
 * @brief Formateo de tramas de datos con timestamp y manejo de errores por puerto serial (UART).
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 *
 * @note Implementa el estándar de tramas de telemetría y diagnóstico con timestamp del RTC DS3231:
 *   - Formato visual tipo Heartbeat para terminal y depuración (idéntico a especificación).
 *   - Formato de trama estructurada NMEA/CSV con Checksum XOR para graficadores y registro.
 *   - Detección, clasificación y reporte de errores en hardware y sensores.
 *   - Validación y parseo seguro de comandos seriales entrantes con verificación de errores.
 */

#ifndef SERIAL_TELEMETRY_H
#define SERIAL_TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ErroresTrama Máscara de Bits de Errores de Telemetría
 * @{
 */
#define ERR_FLAG_NONE               0x0000  ///< Sin errores (Operación normal)
#define ERR_FLAG_TC_DISCONNECTED    0x0001  ///< Termocupla MAX6675 desconectada o circuito abierto
#define ERR_FLAG_OVERTEMPERATURE    0x0002  ///< Sobretemperatura crítica (>= 225.0 °C)
#define ERR_FLAG_HX711_OFFLINE      0x0004  ///< Celda de carga / Transmisor HX711 no responde
#define ERR_FLAG_INTERLOCK_BLOWER   0x0008  ///< Interbloqueo: intento de activar PTC con Blower apagado
#define ERR_FLAG_SD_NOT_MOUNTED     0x0010  ///< Tarjeta MicroSD no detectada o fallo de montaje FATFS
#define ERR_FLAG_FRAME_CHECKSUM     0x0020  ///< Error de checksum en trama de comando serial entrante
#define ERR_FLAG_FRAME_SYNTAX       0x0040  ///< Error de sintaxis o delimitadores en trama serial
#define ERR_FLAG_CMD_OUT_OF_RANGE   0x0080  ///< Parámetro fuera de rango permitido (PWM > 100%, Temp > 225)
/** @} */

/**
 * @struct serial_frame_data_t
 * @brief Datos consolidados para generación de la trama de transmisión serial.
 */
typedef struct {
    uint32_t heartbeat_num;     ///< Contador secuencial de latidos (Heartbeat #)
    const char *datetime_str;   ///< Fecha y hora real "YYYY-MM-DD HH:MM:SS" (desde RTC DS3231)
    float volts;                ///< Tensión de alimentación (V)
    float amps;                 ///< Corriente del sistema (A)
    float watts;                ///< Potencia instantánea (W)
    float energy_wh;            ///< Energía acumulada (Wh)
    float weight_g;             ///< Peso actual medido por la celda HX711 (g)
    float temp_c;               ///< Temperatura actual de la termocupla (°C)
    const char *state_str;      ///< Estado FSM ("INIT", "PRECAL", "CARGA", "TOST", "PAUSA", "ENFRIA")
    int mode_num;               ///< Número de modo (1: Bajo, 2: Medio, 3: Alto, 4: Manual)
    const char *mode_name;      ///< Nombre del modo ("BAJO", "MEDIO", "ALTO", "MANUAL")
    bool ptc_active;            ///< Estado de la resistencia calefactora (true: ON / false: NO)
    uint8_t blower_pct;         ///< Velocidad del soplador centrífugo en porcentaje (0..100%)
    uint8_t motor_pct;          ///< Velocidad del motor de tambor en porcentaje (0..100%)
    uint16_t error_flags;       ///< Máscara de bits de errores activos
} serial_frame_data_t;

/**
 * @brief Inicializa el módulo de telemetría y formateo serial.
 */
esp_err_t serial_telemetry_init(void);

/**
 * @brief Formatea y transmite por UART la línea de telemetría con timestamp y diagnóstico de errores.
 *        Produce exactamente la estructura visual de la terminal:
 *        `[Heartbeat #N | YYYY-MM-DD HH:MM:SS] V: 0.00V | I: 0.00A | P: 0.0W | E: 0.000Wh | Peso: 0.0 g | Temp: 25.00 °C | Estado: INIT | PTC: NO | Err: Sin errores`
 * @param[in] data Puntero a la estructura con los datos actualizados del sistema.
 */
void serial_telemetry_send_heartbeat(const serial_frame_data_t *data);

/**
 * @brief Genera una trama compacta con Checksum XOR estándar (tipo NMEA) para dataloggers externos.
 *        Formato: `$TOST,HEARTBEAT,DATETIME,STATE,MODE,TEMP,PESO,V,I,W,E,PTC,BLW,MTR,ERR*CHKSUM\r\n`
 * @param[in] data Puntero a los datos de telemetría.
 * @param[out] out_buf Búfer de destino para almacenar la trama.
 * @param[in] max_len Tamaño máximo del búfer.
 * @return Longitud en bytes de la trama generada.
 */
size_t serial_telemetry_build_checksum_frame(const serial_frame_data_t *data, char *out_buf, size_t max_len);

/**
 * @brief Valida y decodifica una trama de comando serial entrante comprobando integridad y errores.
 * @param[in] frame_str Cadena recibida por el puerto serie.
 * @param[out] out_cmd_type Tipo de comando decodificado.
 * @param[out] out_param Parámetro numérico opcional asociado al comando.
 * @param[out] out_error_code Código de error si la trama fue rechazada (0 si es válida).
 * @return true si la trama es sintáctica y lógicamente válida.
 */
bool serial_telemetry_parse_command(const char *frame_str, int *out_cmd_type, int *out_param, uint16_t *out_error_code);

/**
 * @brief Obtiene una descripción textual en español del código de error actual.
 * @param[in] error_flags Máscara de bits de error.
 * @param[out] buf Búfer donde escribir la descripción.
 * @param[in] max_len Longitud del búfer.
 */
void serial_telemetry_get_error_desc(uint16_t error_flags, char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_TELEMETRY_H
