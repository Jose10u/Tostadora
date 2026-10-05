idf_component_register(
    SRCS "actuators.c"
    INCLUDE_DIRS "include" "."
    REQUIRES config_pins esp_driver_ledc esp_driver_gpio esp_timer freertos
)
