idf_component_register(
    SRCS
        "lvgl_port.c"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        config
        drivers
    REQUIRES
        freertos
        log
        esp_timer
        lvgl__lvgl
)

# Inject our lv_conf.h into LVGL's include search path.
# LV_CONF_INCLUDE_SIMPLE=1 makes LVGL use: #include "lv_conf.h"
# BEFORE INTERFACE ensures this directory is searched before LVGL's own dir.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_definitions(${lvgl_lib} PUBLIC LV_CONF_INCLUDE_SIMPLE=1)
target_include_directories(${lvgl_lib} BEFORE INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
