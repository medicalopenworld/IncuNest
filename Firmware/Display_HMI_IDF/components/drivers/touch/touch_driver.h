/**
 * @file touch_driver.h
 * @brief Public API for the Goodix GT911 touch controller driver.
 *
 * @details Initializes the GT911 via I2C (SDA=15, SCL=16) using the
 *          espressif/esp_lcd_touch_gt911 IDF managed component.
 *          I2C must already be initialized by display_driver_init() before
 *          calling touch_driver_init().
 *
 *          Phase 0: stub implementation (returns NULL / no touch).
 *          Phase 1: full implementation using esp_lcd_touch_gt911.
 *
 * Normativa aplicable:
 * - IEC 62366-1 §5.1 — touch input must respond within 200 ms (RNF-002)
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle for the touch controller.
 * Phase 0: void*. Phase 1: cast to esp_lcd_touch_handle_t from esp_lcd_touch.h.
 */
typedef void *touch_handle_t;

/**
 * @brief Initialize the GT911 touch controller.
 *
 * @details Assumes I2C port HW_I2C_PORT is already initialized.
 *          Makes up to 3 initialization attempts with 500 ms between retries.
 *          Phase 0: no-op stub. Phase 1: full GT911 init.
 *
 * @return ESP_OK on success, ESP_FAIL if all attempts fail.
 */
esp_err_t touch_driver_init(void);

/**
 * @brief Read the latest touch state.
 *
 * @param[out] x      X coordinate of first touch point (0 if not touched)
 * @param[out] y      Y coordinate of first touch point (0 if not touched)
 * @param[out] pressed true if screen is currently touched
 */
void touch_driver_read(uint16_t *x, uint16_t *y, bool *pressed);

/**
 * @brief Get the raw touch controller handle.
 * Phase 0: returns NULL. Phase 1: returns esp_lcd_touch_handle_t.
 */
touch_handle_t touch_driver_get_handle(void);

#ifdef __cplusplus
}
#endif
