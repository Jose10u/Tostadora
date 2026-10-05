idf_component_register(
    SRCS "roaster_fsm.c"
    INCLUDE_DIRS "include" "."
    REQUIRES config_pins actuators sensor_max6675 sensor_hx711 sensor_ina226 rtc_ds3231 sdcard_logger oled_display button_controller wifi_manager web_server serial_telemetry freertos
)
