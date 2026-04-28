idf_component_register(
    SRCS
        "display/display_driver.c"
        "touch/touch_driver.c"
        "nvs/nvs_storage.c"
    INCLUDE_DIRS
        "."
        "display"
        "touch"
        "nvs"
    PRIV_REQUIRES
        config
    REQUIRES
        esp_lcd
        driver
        nvs_flash
        freertos
        log
)
## Phase 1: add REQUIRES espressif__esp_lcd_touch_gt911

