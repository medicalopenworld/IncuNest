/**
 * @file lv_conf.h
 * @brief LVGL 8.4.x configuration for IncuNest HMI (CrowPanel Advance 7").
 *
 * @details Replaces the Arduino lv_conf.h from the current firmware.
 *          Key changes from the Arduino version:
 *          1. LV_MEM_CUSTOM_ALLOC now uses PSRAM (not SRAM internal)
 *          2. LV_TICK_CUSTOM uses esp_timer_get_time() (not millis())
 *          3. LV_ASSERT_HANDLER calls esp_restart() (not while(1))
 *          4. Only font sizes actually used by the UI are enabled
 *
 * Normativa aplicable:
 * - RNF-007 — LVGL heap in PSRAM to reserve SRAM for bounce buffers
 * - RNF-005 — Watchdog must not trigger from LVGL asserts
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */

#if 1  /* Enable lv_conf.h */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* =========================================================================
   COLOR
   ========================================================================= */

/** RGB565 — native color depth of the CrowPanel RGB panel */
#define LV_COLOR_DEPTH 16

/** No byte swap needed — ESP32-S3 RGB bus handles byte order correctly */
#define LV_COLOR_16_SWAP 0

/** Screen transparency not needed */
#define LV_COLOR_SCREEN_TRANSP 0

/* =========================================================================
   MEMORY
   ========================================================================= */

/**
 * Use custom allocator to direct LVGL objects to PSRAM.
 * CHANGE vs Arduino: current firmware uses MALLOC_CAP_8BIT (SRAM internal).
 * New: MALLOC_CAP_SPIRAM frees SRAM for bounce buffers and ISR stacks.
 */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM
    #define LV_MEM_CUSTOM_INCLUDE    <esp_heap_caps.h>
    #define LV_MEM_CUSTOM_ALLOC(sz)  heap_caps_malloc((sz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    #define LV_MEM_CUSTOM_FREE(p)    heap_caps_free(p)
    #define LV_MEM_CUSTOM_REALLOC(p,sz) heap_caps_realloc((p), (sz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#endif

#define LV_MEM_BUF_MAX_NUM 64

/* =========================================================================
   HAL
   ========================================================================= */

/** Display refresh every 30 ms ≈ 33 fps theoretical */
#define LV_DISP_DEF_REFR_PERIOD 30

/** Poll touch every 30 ms */
#define LV_INDEV_DEF_READ_PERIOD 30

/**
 * CHANGE vs Arduino: millis() replaced by esp_timer_get_time().
 * esp_timer is more accurate under FreeRTOS load (millis() can drift).
 */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE       "esp_timer.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR ((uint32_t)(esp_timer_get_time() / 1000LL))
#endif

#define LV_DPI_DEF 130

/* =========================================================================
   DRAWING
   ========================================================================= */

#define LV_DRAW_COMPLEX 1
#if LV_DRAW_COMPLEX
    #define LV_SHADOW_CACHE_SIZE    0
    #define LV_CIRCLE_CACHE_SIZE    4
#endif

/** Simple layer buffer in SRAM (needs to be fast for compositing) */
#define LV_LAYER_SIMPLE_BUF_SIZE          (24 * 1024)
#define LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE (3 * 1024)

#define LV_IMG_CACHE_DEF_SIZE  0
#define LV_GRADIENT_MAX_STOPS  2
#define LV_GRAD_CACHE_DEF_SIZE 0
#define LV_DITHER_GRADIENT     0
#define LV_DISP_ROT_MAX_BUF    (10 * 1024)

/* =========================================================================
   LOGGING (disable in production — enable for debug builds)
   ========================================================================= */

#define LV_USE_LOG 0

/* =========================================================================
   ASSERTS
   ========================================================================= */

#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/**
 * CRITICAL CHANGE vs Arduino: while(1) replaced by esp_restart().
 * Original while(1) causes Task Watchdog Timer reset without any log.
 * New handler logs the assert location and restarts cleanly.
 * Reference: 05_KNOWN_ISSUES_AND_ROOT_CAUSES.md §Problem 1
 */
#define LV_ASSERT_HANDLER_INCLUDE "esp_log.h"
#define LV_ASSERT_HANDLER \
    do { \
        ESP_LOGE("LVGL", "Assert failed at %s:%d — restarting", __FILE__, __LINE__); \
        extern void esp_restart(void); \
        esp_restart(); \
    } while (0)

/* =========================================================================
   FONTS
   Optimization vs Arduino: only enable fonts actually used by the UI.
   Each enabled size adds ~20-60 KB to flash. Disabled = 0 flash cost.
   ========================================================================= */

#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 1  /**< Labels, status bar */
#define LV_FONT_MONTSERRAT_16 1  /**< Secondary values */
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1  /**< Setpoint labels */
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1  /**< Section headers */
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 1  /**< Temperature values */
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1  /**< Main temperature display */

#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* =========================================================================
   WIDGETS — enable only what the IncuNest UI uses
   ========================================================================= */

#define LV_USE_ARC         1
#define LV_USE_BAR         1
#define LV_USE_BTN         1
#define LV_USE_BTNMATRIX   0
#define LV_USE_CANVAS      0
#define LV_USE_CHECKBOX    1
#define LV_USE_DROPDOWN    1
#define LV_USE_IMG         1
#define LV_USE_LABEL       1
#define LV_USE_LINE        1
#define LV_USE_ROLLER      1
#define LV_USE_SLIDER      1
#define LV_USE_SWITCH      1
#define LV_USE_TEXTAREA    1
#define LV_USE_TABLE       0
#define LV_USE_CHART       1  /**< Historical charts screen */
#define LV_USE_COLORWHEEL  0
#define LV_USE_IMGBTN      1
#define LV_USE_KEYBOARD    1  /**< WiFi credentials input */
#define LV_USE_LED         1  /**< Alarm indicator */
#define LV_USE_LIST        1  /**< Alarm list */
#define LV_USE_MENU        0
#define LV_USE_METER       0
#define LV_USE_MSGBOX      1  /**< Confirmation popups */
#define LV_USE_SPAN        0
#define LV_USE_SPINBOX     1  /**< Temperature spinner */
#define LV_USE_SPINNER     1  /**< Loading indicator */
#define LV_USE_TABVIEW     1  /**< Settings sections */
#define LV_USE_TILEVIEW    0
#define LV_USE_WIN         0

/* =========================================================================
   EXTRA THEMES
   ========================================================================= */

#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_BASIC   0
#define LV_USE_THEME_MONO    0

/* =========================================================================
   LAYOUTS
   ========================================================================= */

#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* =========================================================================
   MISC
   ========================================================================= */

#define LV_SPRINTF_CUSTOM    0
#define LV_USE_USER_DATA     1
#define LV_ENABLE_GC         0

#endif /* LV_CONF_H */
#endif /* Enable lv_conf.h */
