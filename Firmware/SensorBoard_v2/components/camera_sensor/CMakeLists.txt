idf_component_register(
    SRCS
        "sb_camera_sensor.c"
        "sb_cam_builder.c"
    INCLUDE_DIRS "include"
    REQUIRES usb_comm
    PRIV_REQUIRES env_sensors esp_timer
)
