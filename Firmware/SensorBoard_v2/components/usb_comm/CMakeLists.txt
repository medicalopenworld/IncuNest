idf_component_register(
    SRCS
        "sensorBoard_comm.c"
        "sensorBoard_cmd_handler.c"
        "sensorBoard_cmd_builder.c"
        "sensorBoard_crc16.c"
        "sensorBoard_frame.c"
        "sensorBoard_json.c"
        "sensorBoard_status.c"
        "sensorBoard_cmd_registry.c"
        "sensorBoard_host_watch.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
)
