idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        config
        drivers
        lvgl_port
        ui
        nvs_flash
        freertos
        log
)
## Phase 4: add REQUIRES alarm
## Phase 5: add REQUIRES comm
## Phase 7: add REQUIRES wifi
