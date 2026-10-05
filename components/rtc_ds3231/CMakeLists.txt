idf_component_register(
    SRCS "rtc_ds3231.c"
    INCLUDE_DIRS "include" "."
    REQUIRES i2c_bus_manager config_pins esp_driver_i2c freertos
)
