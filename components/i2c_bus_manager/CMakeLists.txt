idf_component_register(
    SRCS "i2c_bus_manager.c"
    INCLUDE_DIRS "include" "."
    REQUIRES config_pins esp_driver_i2c freertos
)
