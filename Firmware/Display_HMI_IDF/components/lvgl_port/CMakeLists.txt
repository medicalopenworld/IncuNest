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
)

## When LVGL is available (after idf_component.yml download), add it:
## REQUIRES lvgl__lvgl espressif__esp_lcd_touch_gt911
## This will be enabled in Phase 1.
