idf_component_register(
    SRCS
        "sb_env_convert.c"
        "sb_env_sensors.c"
    INCLUDE_DIRS "include"
    REQUIRES usb_comm
    PRIV_REQUIRES esp_driver_i2c esp_adc esp_timer
)
