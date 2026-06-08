idf_component_register(
    SRCS
        "sensorBoard_comm.c"
        "sensorBoard_cmd_handler.c"
        "sensorBoard_crc16.c"
        "sensorBoard_frame.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES esp_tinyusb esp_timer cjson
)
