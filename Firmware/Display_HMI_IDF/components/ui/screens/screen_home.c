/**
 * @file screen_home.c
 * @brief Home screen — Phase 1 proof-of-life LVGL render.
 *
 * @details Creates a dark background screen with a centered white label
 *          showing the firmware version and "Display OK" confirmation.
 *          Phase 2 will replace this with the full dashboard.
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */

#include "screen_home.h"
#include "version.h"
#include "lvgl.h"

void screen_home_show(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D1B2A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text_fmt(label, "IncuNest v%s\nDisplay OK", FW_VERSION_STR);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label);

    lv_disp_load_scr(scr);
}
