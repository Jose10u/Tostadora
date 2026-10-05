/**
 * @file config_pins.h
 * @brief Configuración centralizada de pines GPIO, buses y parámetros eléctricos.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 *
 * @note Cumple con los requerimientos de la rúbrica de Sistemas Embebidos y Tiempo Real.
 *       Define los presupuestos máximos de potencia (10.0V / 7.0A -> ~70W).
 */

#ifndef CONFIG_PINS_H
#define CONFIG_PINS_H

#include "driver/gpio.h"
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup PinesActuadores Pines de Actuadores de Potencia
 * @{
 */
#define PIN_BLOWER              GPIO_NUM_25   ///< Blower centrífugo BG0903 (MOSFET D4184, 1 kHz PWM)
#define PIN_MOTOR_RPWM          GPIO_NUM_26   ///< Motorreductor Puente H BTS7960 (RPWM / Horario)
#define PIN_MOTOR_LPWM          GPIO_NUM_27   ///< Motorreductor Puente H BTS7960 (LPWM / Antihorario)
#define PIN_MOTOR_R_EN          GPIO_NUM_14   ///< Habilitador Derecho BTS7960 (R_EN -> Nivel Alto)
#define PIN_MOTOR_L_EN          GPIO_NUM_32   ///< Habilitador Izquierdo BTS7960 (L_EN -> Nivel Alto)
#define PIN_PTC                 GPIO_NUM_4    ///< Resistencia Calefactora PTC (MOSFET de Potencia)
/** @} */

/**
 * @defgroup PinesSPI Bus SPI Maestro Compartido (Termocupla + MicroSD)
 * @{
 */
#define PIN_SPI_SCLK            GPIO_NUM_18   ///< Reloj SPI compartido (SCLK)
#define PIN_SPI_MISO            GPIO_NUM_19   ///< Master In Slave Out compartido (SD DO / MAX6675 SO)
#define PIN_SPI_MOSI            GPIO_NUM_23   ///< Master Out Slave In compartido (SD DI)
#define PIN_MAX6675_CS          GPIO_NUM_15   ///< Chip Select Sensor Termocupla MAX6675
#define PIN_SD_CS               GPIO_NUM_5    ///< Chip Select Tarjeta MicroSD
#define TOSTADORA_SPI_HOST      SPI2_HOST     ///< Host SPI maestro unificado
#define SD_MOUNT_POINT          "/sdcard"     ///< Punto de montaje FATFS en VFS
/** @} */

/**
 * @defgroup PinesI2C Bus I2C Maestro Compartido (OLED + INA226 + RTC DS3231)
 * @{
 */
#define PIN_I2C_SDA             GPIO_NUM_21   ///< Línea de Datos I2C (SDA)
#define PIN_I2C_SCL             GPIO_NUM_22   ///< Línea de Reloj I2C (SCL)

#define OLED_I2C_ADDR_PRIMARY   0x3C          ///< Dirección I2C principal de la pantalla OLED JMD0.96C
#define OLED_I2C_ADDR_SECONDARY 0x3D          ///< Dirección I2C secundaria de la pantalla OLED
#define INA226_I2C_ADDR         0x40          ///< Dirección I2C del monitor de tensión/corriente INA226
#define DS3231_I2C_ADDR         0x68          ///< Dirección I2C del reloj en tiempo real RTC DS3231
/** @} */

/**
 * @defgroup PinesHX711 Celda de Carga y ADC de 24 bits HX711
 * @{
 */
#define PIN_HX711_DT            GPIO_NUM_34   ///< Pin GPI de solo entrada (DOUT de HX711)
#define PIN_HX711_SCK           GPIO_NUM_2    ///< Pin de salida digital con pull-down natural (PD_SCK de HX711)
#define HX711_CAL_FACTOR_DEFAULT (-7050.0f)   ///< Factor de calibración predeterminado (cuentas/gramo)
/** @} */

/**
 * @defgroup PinesBotones Pulsadores de Interacción Física
 * @{
 */
#define PIN_BTN_START_STOP      GPIO_NUM_13   ///< Botón 1: Iniciar Precalentamiento / Tostar / Detener
#define PIN_BTN_PAUSE_RESUME    GPIO_NUM_33   ///< Botón 2: Pausar / Continuar
#define PIN_BTN_MODE            GPIO_NUM_16   ///< Botón 3: Selección de Modo (Click) / Tara Báscula (Mantener 1.5s)
/** @} */

/**
 * @defgroup ParametrosPotencia Parámetros del Modo de Restricción Eléctrica (10.0V / 7.0A)
 * @{
 */
#define RESTRICTION_MAX_VOLTS       10.0f     ///< Límite superior seguro de tensión (10.0 V)
#define RESTRICTION_MAX_AMPS        7.0f      ///< Límite superior seguro de corriente (7.0 A)
#define SUPPLY_NOMINAL_VOLTS        12.0f     ///< Tensión de la fuente primaria de alimentación (12.0 V)

#define DUTY_LIMIT_10V              212       ///< Límite de PWM (83.1% sobre 255) para restringir a 10.0V
#define MOTOR_DUTY_MAX_POWER        255       ///< 100% de potencia para vencer caja reductora y par del tambor

#define PREHEAT_TARGET_TEMP_C       185.0f    ///< Temperatura objetivo de precalentamiento (180°C - 190°C)
#define EMERGENCY_TEMP_CUTOFF_C     225.0f    ///< Corte absoluto irrevocable por software (>= 225.0 °C)
#define COOLDOWN_DURATION_SEC       60        ///< Duración estándar del ciclo de enfriamiento forzado (s)
/** @} */

/**
 * @defgroup ConfigLEDC Canales y Temporizadores LEDC PWM
 * @{
 */
#define PWM_MODE                    LEDC_LOW_SPEED_MODE
#define PWM_DUTY_RES                LEDC_TIMER_8_BIT

#define TIMER_BLOWER                LEDC_TIMER_0
#define FREQ_BLOWER_HZ              1000
#define CH_BLOWER                   LEDC_CHANNEL_0

#define TIMER_MOTOR                 LEDC_TIMER_1
#define FREQ_MOTOR_HZ               1000
#define CH_MOTOR_RPWM               LEDC_CHANNEL_1
#define CH_MOTOR_LPWM               LEDC_CHANNEL_2

#define TIMER_PTC                   LEDC_TIMER_2
#define FREQ_PTC_HZ                 1000
#define CH_PTC                      LEDC_CHANNEL_3
/** @} */

#ifdef __cplusplus
}
#endif

#endif // CONFIG_PINS_H
