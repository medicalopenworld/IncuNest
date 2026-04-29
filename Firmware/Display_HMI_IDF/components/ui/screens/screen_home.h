/**
 * @file screen_home.h
 * @brief Phase 1 home screen — proof-of-life LVGL render with version label.
 *
 * @details Phase 1: single centered label "IncuNest vX.Y.Z / Display OK".
 *          Phase 2: full dashboard with temperature widgets, status bar, alarm banner.
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Create and load the home screen. Must be called within lvgl_port_lock(). */
void screen_home_show(void);

#ifdef __cplusplus
}
#endif
