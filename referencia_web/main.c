/*
 * main.c
 *
 * Orquesta los 3 modulos:
 *   1) lux_control  -> sensor de luz, LEDs, relevo, boton de modo
 *   2) wifi_connect  -> conexion WiFi (STA) + escaneo de referencia
 *   3) web_server    -> pagina web que muestra modo / lux / estado
 */

#include "nvs_flash.h"
#include "esp_log.h"

#include "lux_control.h"
#include "wifi_connect.h"
#include "web_server.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1) Sensor, LEDs y relevo (arranca su propia tarea en segundo plano)
    lux_control_init();
    lux_control_start_task();

    // 2) WiFi: bloquea hasta obtener IP
    wifi_connect_init_sta();

    // 3) Servidor web: ya puede leer el estado de lux_control
    web_server_start();

    ESP_LOGI(TAG, "Sistema listo. Abre en el navegador la IP mostrada arriba.");
}
