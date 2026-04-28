idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        config
        drivers
        nvs_flash
        freertos
        log
)
## Phase 1: add REQUIRES lvgl_port
## Phase 2: add REQUIRES ui
## Phase 4: add REQUIRES alarm
## Phase 5: add REQUIRES comm
## Phase 7: add REQUIRES wifi

## Tell the LVGL component where to find our lv_conf.h
## (uncomment in Phase 1 after LVGL is downloaded)
## idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
## target_include_directories(${lvgl_lib} BEFORE INTERFACE
##     "${CMAKE_CURRENT_SOURCE_DIR}/../components/lvgl_port")
## target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
