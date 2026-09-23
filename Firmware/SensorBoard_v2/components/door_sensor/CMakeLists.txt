idf_component_register(
    SRCS
        "sb_door_logic.c"
        "sb_door_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES usb_comm
    PRIV_REQUIRES esp_driver_gpio esp_timer
)
