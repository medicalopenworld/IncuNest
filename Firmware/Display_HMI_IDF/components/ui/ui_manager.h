/**
 * @file ui_manager.h
 * @brief Screen navigation manager and data update API.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCREEN_HOME     = 0,
    SCREEN_SETTINGS,
    SCREEN_ALARMS,
    SCREEN_INFO,
} screen_id_t;

/* Initialization — call once from app_main after lvgl_port_init() */
esp_err_t ui_manager_init(void);

/* Navigate to a screen (acquires LVGL lock internally) */
void ui_manager_navigate(screen_id_t screen);

/* Update dashboard parameter tiles (acquires LVGL lock internally) */
void ui_manager_update_air_temp(const char *measured, const char *setpoint);
void ui_manager_update_humidity(const char *measured, const char *setpoint);
void ui_manager_update_skin_temp(const char *measured, const char *setpoint);

/* Update status bar indicators (acquires LVGL lock internally) */
void ui_manager_set_comm_state(bool connected);
void ui_manager_set_alarm_count(uint8_t count);

#ifdef __cplusplus
}
#endif
