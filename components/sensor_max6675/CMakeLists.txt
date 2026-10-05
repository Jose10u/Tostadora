idf_component_register(
    SRCS "sensor_max6675.c"
    INCLUDE_DIRS "include" "."
    REQUIRES spi_bus_manager config_pins esp_driver_spi freertos
)
