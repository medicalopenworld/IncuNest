/**
 * @file display_driver.h
 * @brief Public API for the CrowPanel Advance 7" RGB LCD driver.
 *
 * @details Initializes the ESP32-S3 RGB LCD panel (800×480, RGB565) using
 *          esp_lcd_panel_rgb with PSRAM framebuffer and SRAM bounce buffers.
 *          Controls backlight via I2C to STC8H1K28 @ 0x30.
 *          This driver has NO knowledge of LVGL — the lvgl_port component
 *          registers display_driver_flush() as the LVGL flush callback.
 *
 * Normativa aplicable:
 * - IEC 60601-2-19 §201.9.6.2.1.101 — backlight minimum 20% enforced by
 *   display_driver_set_backlight()
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the RGB LCD panel and backlight controller.
 *
 * @details Sequence:
 *  1. Initialize I2C bus (shared with touch driver).
 *  2. Send backlight-off command to STC8H1K28 (prevents flash on init).
 *  3. Configure and create the esp_lcd_rgb_panel with:
 *     - PSRAM framebuffer (fb_in_psram=1)
 *     - SRAM bounce buffers (20 lines × 800 px)
 *     - VSYNC callback stub (replaced by lvgl_port in Phase 1)
 *  4. Initialize the panel (starts DMA streaming).
 *  5. Fill framebuffer with solid blue as Phase 0 proof-of-life.
 *
 * Must be called before display_driver_set_backlight() or display_driver_flush().
 *
 * @return ESP_OK on success, or an error code if hardware init fails.
 */
esp_err_t display_driver_init(void);

/**
 * @brief Copy a rectangular pixel region to the LCD framebuffer.
 *
 * @details Used by lvgl_port as the LVGL flush callback. In direct-mode
 *          (bounce buffer), this copies color_data into the PSRAM framebuffer
 *          at the specified coordinates and triggers DMA transmission.
 *
 * @param[in] x1         Left column (inclusive)
 * @param[in] y1         Top row (inclusive)
 * @param[in] x2         Right column (inclusive)
 * @param[in] y2         Bottom row (inclusive)
 * @param[in] color_data Pointer to RGB565 pixel data (row-major order)
 */
void display_driver_flush(int x1, int y1, int x2, int y2, const void *color_data);

/**
 * @brief Set the LCD backlight brightness via I2C to STC8H1K28.
 *
 * @details The STC8H1K28 supports only ON/OFF via I2C (no PWM dimming).
 *          Any brightness_pct >= DISPLAY_BL_MIN_PCT turns backlight ON.
 *          brightness_pct = 0 turns backlight OFF.
 *          Values below DISPLAY_BL_MIN_PCT (20%) are clamped to 0 (off)
 *          per IEC 60601-2-19 §201.9.6.2.1.101.
 *
 * @param[in] brightness_pct  Brightness percentage 0–100.
 *                            0 = off, 1–19 = forced off (normative min),
 *                            20–100 = on.
 */
void display_driver_set_backlight(uint8_t brightness_pct);

/**
 * @brief Get the raw panel handle (needed by lvgl_port to register flush callback).
 *
 * @return esp_lcd_panel_handle_t — NULL if display_driver_init() has not been called.
 */
esp_lcd_panel_handle_t display_driver_get_panel(void);

/**
 * @brief Notify the display driver that a VSYNC event has occurred.
 *
 * @details Called from the VSYNC ISR callback registered during init.
 *          In Phase 1 this will wake the LVGL task. In Phase 0 it is a no-op.
 *          Returns true if a FreeRTOS yield is needed (called from ISR context).
 *
 * @return true if a higher-priority task was woken (yield needed), false otherwise.
 */
bool display_driver_notify_vsync(void);

/**
 * @brief Register the VSYNC notify function (called by lvgl_port in Phase 1).
 *
 * @details Allows lvgl_port to inject a task-notification function that is
 *          called from the VSYNC ISR. Must be IRAM-safe.
 *
 * @param[in] fn  Pointer to the VSYNC notify function, or NULL to remove.
 */
void display_driver_set_vsync_notify(bool (*fn)(void));

#ifdef __cplusplus
}
#endif
