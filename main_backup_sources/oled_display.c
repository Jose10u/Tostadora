/**
 * @file oled_display.c
 * @brief Implementación del driver gráfico para el display OLED SSD1306 JMD0.96C.
 * @author Sistema de Tostión de Café IoT
 * @date 2026
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "oled_display.h"
#include "oled_font.h"
#include "i2c_bus_manager.h"
#include "config_pins.h"
#include "esp_log.h"

static const char *TAG = "OLED_SSD1306";

#define SSD1306_CTRL_CMD    0x00
#define SSD1306_CTRL_DATA   0x40

static i2c_master_dev_handle_t s_oled_dev = NULL;
static bool s_oled_available              = false;
static uint8_t s_oled_addr                = 0;
static uint8_t s_oled_buffer[OLED_BUFFER_SIZE];

static esp_err_t oled_write_cmd(uint8_t cmd)
{
    if (s_oled_dev == NULL) return ESP_FAIL;
    uint8_t buf[2] = {SSD1306_CTRL_CMD, cmd};
    return i2c_master_transmit(s_oled_dev, buf, sizeof(buf), 50);
}

static esp_err_t oled_write_cmd_list(const uint8_t *cmds, size_t len)
{
    if (s_oled_dev == NULL || cmds == NULL || len == 0) return ESP_FAIL;
    uint8_t buf[len + 1];
    buf[0] = SSD1306_CTRL_CMD;
    memcpy(&buf[1], cmds, len);
    return i2c_master_transmit(s_oled_dev, buf, len + 1, 100);
}

void oled_display_clear(void)
{
    memset(s_oled_buffer, 0, OLED_BUFFER_SIZE);
}

esp_err_t oled_display_update(void)
{
    if (!s_oled_available || s_oled_dev == NULL) return ESP_ERR_INVALID_STATE;

    uint8_t range_cmds[] = {
        0x21, 0, 127, // Set Column Address (0 a 127)
        0x22, 0, 7    // Set Page Address (0 a 7)
    };
    for (size_t i = 0; i < sizeof(range_cmds); i++) {
        oled_write_cmd(range_cmds[i]);
    }

    uint8_t chunk_buf[65];
    chunk_buf[0] = SSD1306_CTRL_DATA;

    for (size_t offset = 0; offset < OLED_BUFFER_SIZE; offset += 64) {
        memcpy(&chunk_buf[1], &s_oled_buffer[offset], 64);
        esp_err_t ret = i2c_master_transmit(s_oled_dev, chunk_buf, 65, 50);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

void oled_display_draw_pixel(int x, int y, bool color)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    int page = y / 8;
    int bit  = y % 8;
    int index = page * OLED_WIDTH + x;

    if (color) {
        s_oled_buffer[index] |= (1 << bit);
    } else {
        s_oled_buffer[index] &= ~(1 << bit);
    }
}

void oled_display_draw_hline(int x, int y, int w, bool color)
{
    if (y < 0 || y >= OLED_HEIGHT || w <= 0) return;
    int x_end = x + w;
    if (x < 0) x = 0;
    if (x_end > OLED_WIDTH) x_end = OLED_WIDTH;

    for (int i = x; i < x_end; i++) {
        oled_display_draw_pixel(i, y, color);
    }
}

void oled_display_draw_rect(int x, int y, int w, int h, bool fill, bool color)
{
    if (w <= 0 || h <= 0) return;
    if (fill) {
        for (int i = 0; i < h; i++) {
            oled_display_draw_hline(x, y + i, w, color);
        }
    } else {
        oled_display_draw_hline(x, y, w, color);
        oled_display_draw_hline(x, y + h - 1, w, color);
        for (int i = y; i < y + h; i++) {
            oled_display_draw_pixel(x, i, color);
            oled_display_draw_pixel(x + w - 1, i, color);
        }
    }
}

void oled_display_draw_progress(int x, int y, int w, int h, float percent)
{
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    oled_display_draw_rect(x, y, w, h, false, true);

    int inner_w = w - 4;
    int inner_h = h - 4;
    int fill_w = (int)(((float)inner_w * percent) / 100.0f);

    if (inner_w > 0 && inner_h > 0) {
        oled_display_draw_rect(x + 2, y + 2, inner_w, inner_h, true, false);
        if (fill_w > 0) {
            oled_display_draw_rect(x + 2, y + 2, fill_w, inner_h, true, true);
        }
    }
}

void oled_display_draw_char(int x, int y, char c, bool color, uint8_t size)
{
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *glyph = s_font5x7[c - 32];

    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            bool px = (line & (1 << row)) != 0;
            bool draw_color = px ? color : !color;
            if (px) {
                if (size == 1) {
                    oled_display_draw_pixel(x + col, y + row, draw_color);
                } else {
                    oled_display_draw_rect(x + (col * size), y + (row * size), size, size, true, draw_color);
                }
            }
        }
    }
}

void oled_display_draw_string(int x, int y, const char *str, bool color, uint8_t size)
{
    if (str == NULL) return;
    int cur_x = x;
    int spacing = (size == 1) ? 6 : (size * 6);

    while (*str) {
        if (*str == '\n') {
            y += (size * 8);
            cur_x = x;
        } else {
            oled_display_draw_char(cur_x, y, *str, color, size);
            cur_x += spacing;
        }
        str++;
    }
}

void oled_display_printf(int x, int y, bool color, uint8_t size, const char *fmt, ...)
{
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    oled_display_draw_string(x, y, buf, color, size);
}

void oled_display_render_dashboard(const char *phase_title, int elapsed_sec, int total_sec,
                                   int mode_num, const char *mode_name,
                                   int blower_pct, int ptc_pct, int motor_pct,
                                   float temp_c, bool tc_connected, float weight_g,
                                   bool sd_mounted, bool wifi_connected,
                                   const char *time_str)
{
    if (!s_oled_available) return;

    oled_display_clear();

    // 1. Barra superior inversa (0..10 px)
    oled_display_draw_rect(0, 0, 128, 10, true, true);
    char title_buf[10];
    strncpy(title_buf, phase_title ? phase_title : "TOSTADOR", 8);
    title_buf[8] = '\0';
    oled_display_draw_string(2, 1, title_buf, false, 1);

    if (sd_mounted) {
        oled_display_draw_string(74, 1, "SD", false, 1);
    }
    if (time_str) {
        oled_display_draw_string(88, 1, time_str, false, 1);
    }

    // 2. Fila 1 (y = 12): Estado y Modo
    oled_display_printf(3, 12, true, 1, "EST:%-6s M%d:%-4s", phase_title ? phase_title : "LISTO", mode_num, mode_name ? mode_name : "BAJO");

    // 3. Fila 2 (y = 21): Blower y PTC
    if (ptc_pct > 0) {
        oled_display_printf(3, 21, true, 1, "BLW:%2d%%  PTC:%2d%%(HT)", blower_pct, ptc_pct);
    } else {
        oled_display_printf(3, 21, true, 1, "BLW:%2d%%  PTC: OFF", blower_pct);
    }

    // 4. Fila 3 (y = 30): Temperatura y Peso
    char t_str[12];
    if (!tc_connected) {
        snprintf(t_str, sizeof(t_str), "T:!DESCON!");
    } else {
        snprintf(t_str, sizeof(t_str), "T:%5.1f~C", temp_c);
    }
    oled_display_printf(3, 30, true, 1, "%-10s P:%4.1fg", t_str, weight_g);

    // 5. Fila 4 (y = 39): Motor + SD + WiFi
    oled_display_printf(3, 39, true, 1, "MTR:%3d%% %s %s", motor_pct,
                        sd_mounted ? "[SD]" : "--",
                        wifi_connected ? "[WF]" : "[AP]");

    // 6. Barra de progreso y cronómetro (y = 49..63)
    oled_display_draw_hline(0, 49, 128, true);
    float prog_pct = 0.0f;
    if (total_sec > 0) {
        prog_pct = ((float)elapsed_sec / (float)total_sec) * 100.0f;
        if (prog_pct > 100.0f) prog_pct = 100.0f;
    }
    oled_display_draw_progress(2, 52, 68, 9, prog_pct);

    if (total_sec == 0) {
        oled_display_printf(74, 53, true, 1, "[START]");
    } else {
        oled_display_printf(74, 53, true, 1, "%02d:%02d/%02d:%02d",
                            elapsed_sec / 60, elapsed_sec % 60,
                            total_sec / 60, total_sec % 60);
    }

    oled_display_update();
}

bool oled_display_is_available(void)
{
    return s_oled_available;
}

esp_err_t oled_display_init(void)
{
    if (i2c_bus_manager_init() != ESP_OK) {
        return ESP_FAIL;
    }

    i2c_master_bus_handle_t bus = i2c_bus_manager_get_handle();
    if (bus == NULL) return ESP_FAIL;

    ESP_LOGI(TAG, "Sondeando pantalla OLED SSD1306 (0x3C / 0x3D)...");
    uint8_t addrs[] = {OLED_I2C_ADDR_PRIMARY, OLED_I2C_ADDR_SECONDARY};

    for (size_t i = 0; i < sizeof(addrs); i++) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address  = addrs[i],
            .scl_speed_hz    = 100000,
        };

        i2c_master_dev_handle_t temp_dev = NULL;
        if (i2c_master_bus_add_device(bus, &dev_cfg, &temp_dev) == ESP_OK) {
            uint8_t test_cmd[2] = {SSD1306_CTRL_CMD, 0xE3};
            if (i2c_master_transmit(temp_dev, test_cmd, sizeof(test_cmd), 50) == ESP_OK) {
                s_oled_dev       = temp_dev;
                s_oled_addr      = addrs[i];
                s_oled_available = true;
                ESP_LOGI(TAG, ">>> PANTALLA OLED DETECTADA EN 0x%02X <<<", s_oled_addr);
                break;
            }
            i2c_master_bus_rm_device(temp_dev);
        }
    }

    if (!s_oled_available) {
        ESP_LOGW(TAG, "Pantalla OLED no detectada en 0x3C ni 0x3D.");
        return ESP_ERR_NOT_FOUND;
    }

    // Inicialización del controlador SSD1306 para panel 128x64
    static const uint8_t init_cmds[] = {
        0xAE,         // Display OFF
        0xD5, 0x80,   // Set Display Clock Divide
        0xA8, 0x3F,   // Multiplex Ratio 64 líneas
        0xD3, 0x00,   // Display Offset = 0
        0x40,         // Start Line = 0
        0x8D, 0x14,   // Habilitar Charge Pump interna (7.5V)
        0x20, 0x00,   // Memory Addressing Mode = Horizontal
        0xA1,         // Segment Re-map
        0xC8,         // COM Output Scan Direction
        0xDA, 0x12,   // COM Pins Config
        0x81, 0xCF,   // Contraste óptimo
        0xD9, 0xF1,   // Pre-charge Period
        0xDB, 0x40,   // VCOMH Deselect
        0xA4,         // Resume to RAM
        0xA6,         // Normal Display
        0xAF          // Display ON
    };

    oled_write_cmd_list(init_cmds, sizeof(init_cmds));
    oled_display_clear();

    // Pantalla de bienvenida inicial
    oled_display_draw_rect(0, 0, 128, 64, false, true);
    oled_display_draw_rect(0, 0, 128, 12, true, true);
    oled_display_draw_string(14, 2, "TOSTADORA IoT", false, 1);
    oled_display_draw_string(10, 22, "SISTEMA INICIADO", true, 1);
    oled_display_draw_string(10, 36, "MODO 10V / 7A", true, 1);
    oled_display_draw_string(10, 50, "ESPERANDO COMANDO", true, 1);
    oled_display_update();

    return ESP_OK;
}
