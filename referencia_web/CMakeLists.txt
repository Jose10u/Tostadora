idf_component_register(
    SRCS "main.c" "lux_control.c" "wifi_connect.c" "web_server.c"
    INCLUDE_DIRS "."
    REQUIRES freertos driver esp_driver_gpio esp_adc esp_timer esp_wifi esp_event esp_netif nvs_flash esp_http_server log
)