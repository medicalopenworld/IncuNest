/**
 * @file ui_theme.h
 * @brief IncuNest UI color palette and font references (LVGL 8.x).
 */
#pragma once

#include "lvgl.h"

/* --- Background layers --- */
#define UI_COLOR_BG         lv_color_hex(0x0D1B2A)   /* main screen background */
#define UI_COLOR_CARD       lv_color_hex(0x1A2B3C)   /* tile / card background */
#define UI_COLOR_BAR        lv_color_hex(0x0A1520)   /* status bar background  */

/* --- Accent colours --- */
#define UI_COLOR_ACCENT     lv_color_hex(0x2B7CE9)   /* primary blue */
#define UI_COLOR_OK         lv_color_hex(0x2ECC71)   /* normal / in-range */
#define UI_COLOR_WARN       lv_color_hex(0xF39C12)   /* warning */
#define UI_COLOR_ALARM      lv_color_hex(0xE84040)   /* alarm / critical */

/* --- Text --- */
#define UI_COLOR_TEXT_PRI   lv_color_hex(0xE0E8F0)   /* primary text */
#define UI_COLOR_TEXT_SEC   lv_color_hex(0x8A9BBD)   /* secondary / labels */
#define UI_COLOR_TEXT_DIM   lv_color_hex(0x445566)   /* disabled / dim */

/* --- Border / separator --- */
#define UI_COLOR_BORDER     lv_color_hex(0x243447)

/* --- Fonts (must be enabled via Kconfig) --- */
#define UI_FONT_TITLE       (&lv_font_montserrat_20)
#define UI_FONT_VALUE_LARGE (&lv_font_montserrat_32)
#define UI_FONT_VALUE_MED   (&lv_font_montserrat_24)
#define UI_FONT_LABEL       (&lv_font_montserrat_16)
#define UI_FONT_SMALL       (&lv_font_montserrat_14)

/* --- Layout constants (px) --- */
#define UI_STATUS_BAR_H     44
#define UI_BOTTOM_BAR_H     52
#define UI_CARD_RADIUS      12
#define UI_PAD_CARD         12
