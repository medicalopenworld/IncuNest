/**
 * @file widget_temp_display.c
 * @brief Parameter tile widget — title + large measured value + small setpoint.
 *
 * Card layout (240×170 px tile):
 *   ┌──────────────────────────────┐
 *   │ ▌ Temp. Aire          °C    │  ← accent strip + title + unit
 *   │                             │
 *   │        --.-                 │  ← measured value (large)
 *   │                             │
 *   │   SP: --.-                  │  ← setpoint (small, bottom)
 *   └──────────────────────────────┘
 *
 * Handles stored in lv_obj user_data (lv_mem_alloc'd struct).
 */

#include "widget_temp_display.h"
#include "ui_theme.h"

#include <stdio.h>

typedef struct {
    lv_obj_t *measured_label;
    lv_obj_t *setpoint_label;
} temp_tile_data_t;

/* ------------------------------------------------------------------ */

lv_obj_t *widget_temp_display_create(lv_obj_t *parent, const widget_temp_cfg_t *cfg)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 240, 170);
    lv_obj_set_style_bg_color(card, UI_COLOR_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, UI_CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, UI_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Accent strip — left edge */
    lv_obj_t *strip = lv_obj_create(card);
    lv_obj_set_size(strip, 5, 170);
    lv_obj_set_style_bg_color(strip, cfg->accent_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(strip, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(strip, 0, LV_PART_MAIN);
    lv_obj_align(strip, LV_ALIGN_LEFT_MID, 0, 0);

    /* Title label */
    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, cfg->title);
    lv_obj_set_style_text_font(title, UI_FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT_SEC, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

    /* Unit label — top right */
    lv_obj_t *unit = lv_label_create(card);
    lv_label_set_text(unit, cfg->unit);
    lv_obj_set_style_text_font(unit, UI_FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_color(unit, cfg->accent_color, LV_PART_MAIN);
    lv_obj_align(unit, LV_ALIGN_TOP_RIGHT, -10, 10);

    /* Measured value — center, large */
    lv_obj_t *meas = lv_label_create(card);
    lv_label_set_text(meas, "--.-");
    lv_obj_set_style_text_font(meas, UI_FONT_VALUE_LARGE, LV_PART_MAIN);
    lv_obj_set_style_text_color(meas, UI_COLOR_TEXT_PRI, LV_PART_MAIN);
    lv_obj_align(meas, LV_ALIGN_CENTER, 12, -8);

    /* Setpoint — bottom left */
    lv_obj_t *sp = lv_label_create(card);
    lv_label_set_text(sp, "SP: --.-");
    lv_obj_set_style_text_font(sp, UI_FONT_SMALL, LV_PART_MAIN);
    lv_obj_set_style_text_color(sp, UI_COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_align(sp, LV_ALIGN_BOTTOM_LEFT, 14, -10);

    temp_tile_data_t *d = lv_mem_alloc(sizeof(temp_tile_data_t));
    d->measured_label = meas;
    d->setpoint_label = sp;
    lv_obj_set_user_data(card, d);

    return card;
}

/* ------------------------------------------------------------------ */

void widget_temp_display_update(lv_obj_t *tile,
                                const char *measured,
                                const char *setpoint)
{
    temp_tile_data_t *d = lv_obj_get_user_data(tile);
    if (!d) return;

    if (measured) lv_label_set_text(d->measured_label, measured);

    if (setpoint) {
        char buf[24];
        snprintf(buf, sizeof(buf), "SP: %s", setpoint);
        lv_label_set_text(d->setpoint_label, buf);
    }
}

/* ------------------------------------------------------------------ */

void widget_temp_display_set_state(lv_obj_t *tile, bool alarm)
{
    temp_tile_data_t *d = lv_obj_get_user_data(tile);
    if (!d) return;

    lv_color_t color = alarm ? UI_COLOR_ALARM : UI_COLOR_TEXT_PRI;
    lv_obj_set_style_text_color(d->measured_label, color, LV_PART_MAIN);
}
