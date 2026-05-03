/**
 * @file screen_home.c
 * @brief Home dashboard — status bar + 3 parameter tiles + navigation buttons.
 *
 * All children attached directly to the screen object (no intermediate
 * lv_obj container) to avoid default-theme white backgrounds on containers.
 *
 * Layout (800×480):
 *   y=0..44   — status bar
 *   y=88..258 — 3 tiles × 240×170, centred horizontally
 *   y=410..462 — 2 nav buttons × 180×48
 *
 * Tile X positions: margin=(800-3×240-2×20)/2=20
 *   tile0: x=20, tile1: x=280, tile2: x=540
 * Button X positions: margin=(800-2×180-24)/2=208
 *   btn0: x=208, btn1: x=412
 */

#include "screen_home.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "widget_status_bar.h"
#include "widget_temp_display.h"
#include "lvgl.h"

/* --- persistent screen + widget handles --- */
static lv_obj_t *s_scr       = NULL;
static lv_obj_t *s_bar       = NULL;
static lv_obj_t *s_tile_air  = NULL;
static lv_obj_t *s_tile_hum  = NULL;
static lv_obj_t *s_tile_skin = NULL;

/* --- button callbacks --- */
static void btn_settings_cb(lv_event_t *e)
{
    (void)e;
    ui_manager_navigate(SCREEN_SETTINGS);
}

static void btn_alarms_cb(lv_event_t *e)
{
    (void)e;
    ui_manager_navigate(SCREEN_ALARMS);
}

/* --- helper: create a nav button directly on the screen --- */
static lv_obj_t *make_nav_btn(lv_obj_t *parent, const char *text,
                               lv_coord_t x, lv_coord_t y,
                               lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 180, 48);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, UI_COLOR_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, UI_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, UI_FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_SEC, LV_PART_MAIN);
    lv_obj_center(lbl);
    return btn;
}

/* ------------------------------------------------------------------ */

void screen_home_show(void)
{
    if (s_scr == NULL) {
        s_scr = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(s_scr, UI_COLOR_BG, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(s_scr, 0, LV_PART_MAIN);

        s_bar = widget_status_bar_create(s_scr);

        widget_temp_cfg_t cfg_air = {
            .title        = "Temp. Aire",
            .unit         = "\xC2\xB0""C",
            .accent_color = UI_COLOR_ACCENT,
        };
        s_tile_air = widget_temp_display_create(s_scr, &cfg_air);
        lv_obj_set_pos(s_tile_air, 20, 88);

        widget_temp_cfg_t cfg_hum = {
            .title        = "Humedad",
            .unit         = "%",
            .accent_color = lv_color_hex(0x16A085),
        };
        s_tile_hum = widget_temp_display_create(s_scr, &cfg_hum);
        lv_obj_set_pos(s_tile_hum, 280, 88);

        widget_temp_cfg_t cfg_skin = {
            .title        = "Temp. Piel",
            .unit         = "\xC2\xB0""C",
            .accent_color = lv_color_hex(0x8E44AD),
        };
        s_tile_skin = widget_temp_display_create(s_scr, &cfg_skin);
        lv_obj_set_pos(s_tile_skin, 540, 88);

        make_nav_btn(s_scr, LV_SYMBOL_SETTINGS "  Config.",  208, 408, btn_settings_cb);
        make_nav_btn(s_scr, LV_SYMBOL_BELL     "  Alarmas",  412, 408, btn_alarms_cb);
    }
    lv_disp_load_scr(s_scr);
}

/* ------------------------------------------------------------------ */

void screen_home_update_air_temp(const char *measured, const char *setpoint)
{
    if (s_tile_air) widget_temp_display_update(s_tile_air, measured, setpoint);
}

void screen_home_update_humidity(const char *measured, const char *setpoint)
{
    if (s_tile_hum) widget_temp_display_update(s_tile_hum, measured, setpoint);
}

void screen_home_update_skin_temp(const char *measured, const char *setpoint)
{
    if (s_tile_skin) widget_temp_display_update(s_tile_skin, measured, setpoint);
}

void screen_home_set_comm_state(bool connected)
{
    if (s_bar) widget_status_bar_set_comm(s_bar, connected);
}

void screen_home_set_alarm_count(uint8_t count)
{
    if (s_bar) widget_status_bar_set_alarms(s_bar, count);
}

