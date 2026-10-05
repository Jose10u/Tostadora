idf_component_register(
    SRCS "sensor_hx711.c"
    INCLUDE_DIRS "include" "."
    REQUIRES config_pins esp_driver_gpio esp_timer freertos
)
