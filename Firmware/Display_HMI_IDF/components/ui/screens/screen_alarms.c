/**
 * @file screen_alarms.c
 * @brief Alarms screen stub (FASE 2).  Back button at TOP-LEFT so its
 *        Y-coordinate (16-60) never overlaps the Home nav buttons (y 408-456).
 */

#include "screen_alarms.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "lvgl.h"

static lv_obj_t *s_scr      = NULL;
static lv_obj_t *s_back_btn = NULL;

static void back_cb(lv_event_t *e)
{
    (void)e;
    ui_manager_navigate(SCREEN_HOME);
}

void screen_alarms_show(void)
{
    if (s_scr == NULL) {
        s_scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(s_scr, UI_COLOR_BG, LV_PART_MAIN);
        lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(s_scr, 0, LV_PART_MAIN);

        s_back_btn = lv_btn_create(s_scr);
        lv_obj_set_size(s_back_btn, 120, 44);
        lv_obj_set_pos(s_back_btn, 16, 16);
        lv_obj_set_style_bg_color(s_back_btn, UI_COLOR_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(s_back_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(s_back_btn, 8, LV_PART_MAIN);
        lv_obj_add_event_cb(s_back_btn, back_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl = lv_label_create(s_back_btn);
        lv_label_set_text(lbl, LV_SYMBOL_LEFT " Volver");
        lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_PRI, LV_PART_MAIN);
        lv_obj_center(lbl);

        lv_obj_t *title = lv_label_create(s_scr);
        lv_label_set_text(title, "Alarmas");
        lv_obj_set_style_text_font(title, UI_FONT_TITLE, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, UI_COLOR_TEXT_PRI, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

        lv_obj_t *note = lv_label_create(s_scr);
        lv_label_set_text(note, "Sin alarmas activas");
        lv_obj_set_style_text_font(note, UI_FONT_LABEL, LV_PART_MAIN);
        lv_obj_set_style_text_color(note, UI_COLOR_TEXT_DIM, LV_PART_MAIN);
        lv_obj_align(note, LV_ALIGN_CENTER, 0, 0);
    }
    lv_disp_load_scr(s_scr);
}

void screen_alarms_set_nav_enabled(bool enabled)
{
    if (s_back_btn == NULL) return;
    if (enabled)
        lv_obj_clear_state(s_back_btn, LV_STATE_DISABLED);
    else
        lv_obj_add_state(s_back_btn, LV_STATE_DISABLED);
}
