/**
 * @file lvgl_port.c
 * @brief LVGL integration layer for ESP-IDF (display driver, touch, task, tick timer).
 *
 * @details Integrates LVGL with esp_lcd_panel_rgb and esp_lcd_touch_gt911.
 *          Implements the LVGL task, esp_timer tick, recursive mutex, and
 *          flush/read callbacks. Phase 0 stubs — full implementation in Phase 1.
 *
 * Normativa aplicable:
 * - RNF-001 — ≥20 fps, RNF-002 — ≤200 ms touch latency
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */

#include "lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "LVGL_PORT";

// TODO: Fase 1 — Implement full LVGL port
// Reference: 08_LVGL_CONFIGURATION.md, manufacturer lvgl_port.c

esp_err_t lvgl_port_init(void)
{
    ESP_LOGW(TAG, "lvgl_port_init() — TODO: Fase 1");
    return ESP_OK;
}

bool lvgl_port_lock(int timeout_ms)
{
    (void)timeout_ms;
    return true;
}

void lvgl_port_unlock(void)
{
}
