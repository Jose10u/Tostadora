idf_component_register(
    SRCS "button_controller.c"
    INCLUDE_DIRS "include" "."
    REQUIRES config_pins esp_driver_gpio esp_timer freertos
)
