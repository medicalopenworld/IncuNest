idf_component_register(
    SRCS
        "sb_audio_dsp.c"
        "sb_mic_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES usb_comm
    PRIV_REQUIRES esp_driver_i2s esp_timer
)
