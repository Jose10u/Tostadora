/**
 * @file oled_display.h
 * @brief Driver gráfico para pantalla OLED 128x64 SSD1306 (JMD0.96C) sobre I2C.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_WIDTH                  128
#define OLED_HEIGHT                 64
#define OLED_BUFFER_SIZE            (OLED_WIDTH * OLED_HEIGHT / 8) // 1024 bytes

/**
 * @brief Inicializa el controlador SSD1306 sondeando las direcciones 0x3C y 0x3D en I2C.
 * @return ESP_OK si la pantalla respondió e inicializó la bomba de carga.
 */
esp_err_t oled_display_init(void);

/**
 * @brief Borra todo el búfer gráfico de la pantalla.
 */
void oled_display_clear(void);

/**
 * @brief Transfiere el búfer gráfico local hacia la memoria RAM interna del SSD1306.
 */
esp_err_t oled_display_update(void);

/**
 * @brief Dibuja un píxel individual.
 */
void oled_display_draw_pixel(int x, int y, bool color);

/**
 * @brief Dibuja una línea horizontal optimizada.
 */
void oled_display_draw_hline(int x, int y, int w, bool color);

/**
 * @brief Dibuja un rectángulo con opción de relleno.
 */
void oled_display_draw_rect(int x, int y, int w, int h, bool fill, bool color);

/**
 * @brief Dibuja una barra de progreso gráfica proporcional.
 */
void oled_display_draw_progress(int x, int y, int w, int h, float percent);

/**
 * @brief Dibuja un carácter de la fuente estándar 5x7.
 */
void oled_display_draw_char(int x, int y, char c, bool color, uint8_t size);

/**
 * @brief Dibuja una cadena de texto en las coordenadas especificadas.
 */
void oled_display_draw_string(int x, int y, const char *str, bool color, uint8_t size);

/**
 * @brief Imprime texto formateado (estilo printf) en pantalla.
 */
void oled_display_printf(int x, int y, bool color, uint8_t size, const char *fmt, ...);

/**
 * @brief Renderiza el panel de supervisión en tiempo real de la tostadora en la pantalla OLED.
 */
void oled_display_render_dashboard(const char *phase_title, int elapsed_sec, int total_sec,
                                   int mode_num, const char *mode_name,
                                   int blower_pct, int ptc_pct, int motor_pct,
                                   float temp_c, bool tc_connected, float weight_g,
                                   bool sd_mounted, bool wifi_connected,
                                   const char *time_str);

/**
 * @brief Retorna si la pantalla OLED está operativa.
 */
bool oled_display_is_available(void);

#ifdef __cplusplus
}
#endif

#endif // OLED_DISPLAY_H
