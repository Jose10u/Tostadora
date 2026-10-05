import os
import shutil

base_dir = r"c:\Users\hurta\OneDrive\Documentos\embproyecto\tostadora_potencia"
main_dir = os.path.join(base_dir, "main")
comp_dir = os.path.join(base_dir, "components")

os.makedirs(comp_dir, exist_ok=True)

components_spec = [
    {
        "name": "config_pins",
        "srcs": [],
        "public_headers": ["config_pins.h"],
        "private_headers": [],
        "requires": ["driver", "esp_driver_gpio", "esp_driver_ledc"]
    },
    {
        "name": "i2c_bus_manager",
        "srcs": ["i2c_bus_manager.c"],
        "public_headers": ["i2c_bus_manager.h"],
        "private_headers": [],
        "requires": ["config_pins", "esp_driver_i2c", "freertos"]
    },
    {
        "name": "spi_bus_manager",
        "srcs": ["spi_bus_manager.c"],
        "public_headers": ["spi_bus_manager.h"],
        "private_headers": [],
        "requires": ["config_pins", "esp_driver_spi", "freertos"]
    },
    {
        "name": "actuators",
        "srcs": ["actuators.c"],
        "public_headers": ["actuators.h"],
        "private_headers": [],
        "requires": ["config_pins", "esp_driver_ledc", "esp_driver_gpio", "esp_timer", "freertos"]
    },
    {
        "name": "sensor_max6675",
        "srcs": ["sensor_max6675.c"],
        "public_headers": ["sensor_max6675.h"],
        "private_headers": [],
        "requires": ["spi_bus_manager", "config_pins", "esp_driver_spi", "freertos"]
    },
    {
        "name": "sensor_hx711",
        "srcs": ["sensor_hx711.c"],
        "public_headers": ["sensor_hx711.h"],
        "private_headers": [],
        "requires": ["config_pins", "esp_driver_gpio", "esp_timer", "freertos"]
    },
    {
        "name": "sensor_ina226",
        "srcs": ["sensor_ina226.c"],
        "public_headers": ["sensor_ina226.h"],
        "private_headers": [],
        "requires": ["i2c_bus_manager", "config_pins", "esp_driver_i2c", "freertos"]
    },
    {
        "name": "rtc_ds3231",
        "srcs": ["rtc_ds3231.c"],
        "public_headers": ["rtc_ds3231.h"],
        "private_headers": [],
        "requires": ["i2c_bus_manager", "config_pins", "esp_driver_i2c", "freertos"]
    },
    {
        "name": "sdcard_logger",
        "srcs": ["sdcard_logger.c"],
        "public_headers": ["sdcard_logger.h"],
        "private_headers": [],
        "requires": ["spi_bus_manager", "config_pins", "esp_driver_spi", "fatfs", "esp_driver_sdspi", "sdmmc", "freertos"]
    },
    {
        "name": "oled_display",
        "srcs": ["oled_display.c"],
        "public_headers": ["oled_display.h"],
        "private_headers": ["oled_font.h"],
        "requires": ["i2c_bus_manager", "config_pins", "esp_driver_i2c", "freertos"]
    },
    {
        "name": "button_controller",
        "srcs": ["button_controller.c"],
        "public_headers": ["button_controller.h"],
        "private_headers": [],
        "requires": ["config_pins", "esp_driver_gpio", "esp_timer", "freertos"]
    },
    {
        "name": "serial_telemetry",
        "srcs": ["serial_telemetry.c"],
        "public_headers": ["serial_telemetry.h"],
        "private_headers": [],
        "requires": ["freertos"]
    },
    {
        "name": "wifi_manager",
        "srcs": ["wifi_manager.c"],
        "public_headers": ["wifi_manager.h"],
        "private_headers": ["wifi_credentials.h"],
        "requires": ["esp_wifi", "esp_netif", "esp_event", "nvs_flash", "freertos"]
    },
    {
        "name": "web_server",
        "srcs": ["web_server.c"],
        "public_headers": ["web_server.h"],
        "private_headers": ["web_dashboard.h"],
        "requires": ["esp_http_server", "freertos"]
    },
    {
        "name": "roaster_fsm",
        "srcs": ["roaster_fsm.c"],
        "public_headers": ["roaster_fsm.h"],
        "private_headers": [],
        "requires": [
            "config_pins", "actuators", "sensor_max6675", "sensor_hx711",
            "sensor_ina226", "rtc_ds3231", "sdcard_logger", "oled_display",
            "button_controller", "wifi_manager", "web_server", "serial_telemetry", "freertos"
        ]
    }
]

for c in components_spec:
    c_dir = os.path.join(comp_dir, c["name"])
    inc_dir = os.path.join(c_dir, "include")
    os.makedirs(inc_dir, exist_ok=True)

    for h in c["public_headers"]:
        src_file = os.path.join(main_dir, h)
        if os.path.exists(src_file):
            shutil.copy2(src_file, os.path.join(inc_dir, h))

    for ph in c["private_headers"]:
        src_file = os.path.join(main_dir, ph)
        if os.path.exists(src_file):
            shutil.copy2(src_file, os.path.join(c_dir, ph))

    for s in c["srcs"]:
        src_file = os.path.join(main_dir, s)
        if os.path.exists(src_file):
            shutil.copy2(src_file, os.path.join(c_dir, s))

    req_str = " ".join(c["requires"])
    if c["srcs"]:
        src_str = " ".join(f'"{s}"' for s in c["srcs"])
        cm_content = f"""idf_component_register(
    SRCS {src_str}
    INCLUDE_DIRS "include" "."
    REQUIRES {req_str}
)
"""
    else:
        cm_content = f"""idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES {req_str}
)
"""
    with open(os.path.join(c_dir, "CMakeLists.txt"), "w", encoding="utf-8") as f:
        f.write(cm_content)

print(f"Creados exitosamente {len(components_spec)} componentes en {comp_dir}")
