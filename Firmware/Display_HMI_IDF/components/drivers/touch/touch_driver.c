/**
 * @file touch_driver.c
 * @brief Initializes and reads the Goodix GT911 capacitive touch controller.
 *
 * @details Uses espressif/esp_lcd_touch_gt911 IDF component.
 *          I2C port is shared with the backlight controller and must be
 *          initialized by display_driver_init() before calling touch_driver_init().
 *
 * Normativa aplicable:
 * - IEC 62366-1 §5.1 — touch latency ≤200 ms
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */

#include "touch_driver.h"
#include "app_config.h"

// TODO: Fase 1 — Implement GT911 initialization using esp_lcd_touch_gt911 component
// Reference: 03_DISPLAY_DRIVER_ANALYSIS.md §2, manufacturer IDF code waveshare_rgb_lcd_port.c

#include "esp_log.h"

static const char *TAG = "TOUCH";

esp_err_t touch_driver_init(void)
{
    ESP_LOGW(TAG, "touch_driver_init() — TODO: Fase 1");
    return ESP_OK;
}

void touch_driver_read(uint16_t *x, uint16_t *y, bool *pressed)
{
    if (x)       *x       = 0;
    if (y)       *y       = 0;
    if (pressed) *pressed = false;
}

touch_handle_t touch_driver_get_handle(void)
{
    return NULL;
}
