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

# LV_TICK_CUSTOM_SYS_TIME_EXPR has no Kconfig entry in lvgl__lvgl; inject via compile flag.
# CONFIG_LV_TICK_CUSTOM=y and CONFIG_LV_TICK_CUSTOM_INCLUDE="esp_timer.h" are in sdkconfig.defaults.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_definitions(${lvgl_lib} PUBLIC
    "LV_TICK_CUSTOM_SYS_TIME_EXPR=((uint32_t)(esp_timer_get_time()/1000LL))"
)
