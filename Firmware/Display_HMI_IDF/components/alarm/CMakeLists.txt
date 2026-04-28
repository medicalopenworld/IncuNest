idf_component_register(
    SRCS
        "alarm_manager.c"
        "alarm_audio.c"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        config
    REQUIRES
        freertos
        log
        nvs_flash
)
## Phase 4: full alarm system implementation
