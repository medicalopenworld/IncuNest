idf_component_register(
    SRCS
        "wifi_manager.c"
        "ota_manager.c"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        config
    REQUIRES
        freertos
        log
        esp_wifi
        esp_http_server
        app_update
)
## Phase 7: WiFi and OTA implementation
