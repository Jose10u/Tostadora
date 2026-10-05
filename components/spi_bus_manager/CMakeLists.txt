idf_component_register(
    SRCS "spi_bus_manager.c"
    INCLUDE_DIRS "include" "."
    REQUIRES config_pins esp_driver_spi freertos
)
