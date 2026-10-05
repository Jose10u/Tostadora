idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        config_pins
        actuators
        i2c_bus_manager
        spi_bus_manager
        sensor_max6675
        sensor_hx711
        sensor_ina226
        rtc_ds3231
        sdcard_logger
        oled_display
        button_controller
        roaster_fsm
        serial_telemetry
        wifi_manager
        web_server
)
