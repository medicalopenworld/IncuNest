/**
 * @file lvgl_port.h
 * @brief Public API for the LVGL ↔ ESP-IDF integration layer.
 *
 * @details Manages the LVGL task, tick timer (esp_timer), recursive mutex,
 *          flush callback registration, and touch input device registration.
 *          This is the ONLY component allowed to call lv_timer_handler().
 *
 * Normativa aplicable:
 * - RNF-001 — ≥20 fps minimum frame rate
 * - RNF-002 — touch latency ≤200 ms
 * - RNF-006 — UI updates via IPC queue, no direct LVGL access from CommTask
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Fase 1 — Implement LVGL port
// Reference: 08_LVGL_CONFIGURATION.md, manufacturer lvgl_port.c

/**
 * @brief Initialize LVGL, register display and touch drivers, start LVGL task.
 * Must be called after display_driver_init() and touch_driver_init().
 */
esp_err_t lvgl_port_init(void);

/**
 * @brief Acquire the LVGL recursive mutex.
 * @param timeout_ms Timeout in ms. -1 = wait forever.
 * @return true if mutex acquired, false on timeout.
 */
bool lvgl_port_lock(int timeout_ms);

/** @brief Release the LVGL recursive mutex. */
void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif
