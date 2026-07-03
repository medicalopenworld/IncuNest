idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES usb_comm env_sensors
)
