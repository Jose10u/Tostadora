idf_component_register(
    SRCS "sdcard_logger.c"
    INCLUDE_DIRS "include" "."
    REQUIRES spi_bus_manager config_pins esp_driver_spi fatfs esp_driver_sdspi sdmmc freertos
)
