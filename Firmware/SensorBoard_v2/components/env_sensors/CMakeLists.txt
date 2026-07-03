idf_component_register(
    SRCS
        "sb_env_convert.c"
        "sb_env_sensors.c"
    INCLUDE_DIRS "include"
    REQUIRES usb_comm esp_driver_i2c # i2c publico: include/sb_env_i2c.h expone el handle del bus
    PRIV_REQUIRES esp_adc esp_timer
)
