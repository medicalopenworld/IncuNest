/**
 * @file ui_manager.h
 * @brief Screen navigation manager — state machine for active screen.
 *
 * @details ui_manager is the ONLY component that calls lv_disp_load_scr().
 *          All screen transitions go through ui_manager_navigate_to().
 *          Phase 1: init only, shows home screen.
 *          Phase 2: full navigation state machine.
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UI and load the first screen.
 * Must be called after lvgl_port_init(). Acquires LVGL lock internally.
 */
esp_err_t ui_manager_init(void);

#ifdef __cplusplus
}
#endif
