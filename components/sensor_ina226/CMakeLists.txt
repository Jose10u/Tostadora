idf_component_register(
    SRCS "sensor_ina226.c"
    INCLUDE_DIRS "include" "."
    REQUIRES i2c_bus_manager config_pins esp_driver_i2c freertos
)
