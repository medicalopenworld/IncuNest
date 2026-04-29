/**
 * @file ui_manager.c
 * @brief Screen navigation manager implementation.
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */

#include "ui_manager.h"
#include "screen_home.h"
#include "lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "UI_MGR";

esp_err_t ui_manager_init(void)
{
    lvgl_port_lock(-1);
    screen_home_show();
    lvgl_port_unlock();
    ESP_LOGI(TAG, "Home screen loaded");
    return ESP_OK;
}
