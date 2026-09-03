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
        "sensorBoard_usb_orient.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
    PRIV_REQUIRES esp_hal_usb  # usb_wrap_ll_phy_enable_pin_exchg (autoswap D+/D-)
)
